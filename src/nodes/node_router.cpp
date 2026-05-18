#include "node_router.h"

#include "core/config.h"
#include "core/config_manager.h"

#include <esp_system.h>

void NodeRouter::init(const NodeConfig& cfg, LoRaManager& lora) {
    _cfg  = &cfg;
    _lora = &lora;
    randomSeed((uint32_t)esp_random());

    Serial.printf("[RTR] Nodo: %s (ID 0x%08lX)\n",
                  cfg.node_name, (unsigned long)cfg.node_id);
    Serial.printf("[RTR] Mesh max hops: %u\n", cfg.mesh_max_hops);
    Serial.println("[RTR] Reenvio mesh activo (serial: config_mode)");
    Serial.println();
}

void NodeRouter::loop() {
    _handleSerialConfigMode();

    TestPacket pkt;
    if (!_lora->poll(pkt)) {
        return;
    }

    if (pkt.source_id == _cfg->node_id) {
        return;
    }

    if (_isDuplicateAndMark(pkt)) {
        return;
    }

    bool forMe = (pkt.dest_id == _cfg->node_id);
    bool isBroadcast = (pkt.dest_id == NODE_BROADCAST_ID);

    if (forMe && pkt.msg_type == MSG_HELLO) {
        float batV = 0.0f;
        uint8_t batPct = 0;
        if (_lora->sendNodeStatus(_cfg->node_id, pkt.source_id, pkt.packet_id,
                                  batV, batPct,
                                  NPK_VALUE_UNAVAILABLE,
                                  NPK_VALUE_UNAVAILABLE,
                                  NPK_VALUE_UNAVAILABLE,
                                  0, pkt.hop_limit)) {
            Serial.printf("[RTR] STATUS -> 0x%08lX\n", (unsigned long)pkt.source_id);
        }
        _lora->listen();
    }

    bool ttlAvailable = (uint8_t)(pkt.hop_count + 1) <= pkt.hop_limit;
    bool shouldForward = ttlAvailable && (isBroadcast || !forMe);

    if (shouldForward) {
        uint32_t delayMs = random(ROUTER_FWD_JITTER_MIN_MS,
                                  ROUTER_FWD_JITTER_MAX_MS + 1);

        if (pkt.msg_type == MSG_PING || pkt.msg_type == MSG_HELLO) {
            delayMs = random(ROUTER_FWD_REQ_MIN_MS, ROUTER_FWD_REQ_MAX_MS + 1);
        } else if (pkt.msg_type == MSG_PONG || pkt.msg_type == MSG_NODE_STATUS) {
            delayMs = random(ROUTER_FWD_RESP_MIN_MS, ROUTER_FWD_RESP_MAX_MS + 1);
        }

        delay(delayMs);
        if (_lora->forwardPacket(pkt)) {
            Serial.printf("[RTR] FWD type %u src 0x%08lX dst 0x%08lX hop %u/%u\n",
                          pkt.msg_type,
                          (unsigned long)pkt.source_id,
                          (unsigned long)pkt.dest_id,
                          (unsigned)(pkt.hop_count + 1),
                          (unsigned)pkt.hop_limit);
        }
        _lora->listen();
    }
}

bool NodeRouter::_isDuplicateAndMark(const TestPacket& pkt) {
    return PacketDedupe::isDuplicateAndMark(_seen, pkt, SEEN_TTL_MS);
}

void NodeRouter::_handleSerialConfigMode() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            _lineBuf[_linePos] = '\0';
            if (strcmp(_lineBuf, "config_mode") == 0) {
                Serial.println("[RTR] Entrando a Config Mode...");
                delay(300);
                if (!ConfigManager::requestConfigModeOnce()) {
                    Serial.println("[ERR] No se pudo programar Config Mode en NVS");
                    _linePos = 0;
                    return;
                }
                ESP.restart();
            }
            _linePos = 0;
        } else if (_linePos < sizeof(_lineBuf) - 1) {
            _lineBuf[_linePos++] = c;
        }
    }
}
