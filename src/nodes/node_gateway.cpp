#include "node_gateway.h"
#include "core/config_manager.h"
#include "core/config.h"

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::init(const NodeConfig& cfg, LoRaManager& lora, MetricsCollector& metrics) {
    _cfg     = &cfg;
    _lora    = &lora;
    _metrics = &metrics;

    Serial.printf("[GW]  Nodo: %s (ID 0x%08lX)\n",
                  cfg.node_name, (unsigned long)cfg.node_id);
    Serial.printf("[GW]  Auto-ping: %lu ms | Timeout: %lu ms\n",
                  (unsigned long)cfg.auto_ping_ms,
                  (unsigned long)cfg.ping_timeout_ms);
    Serial.printf("[GW]  Mesh beacon: %lu ms | Max hops: %u\n",
                  (unsigned long)cfg.mesh_beacon_ms,
                  cfg.mesh_max_hops);
    Serial.println("[GW]  Comandos: ping | range_test <N> | config <sf> <bw> <cr> <pwr> | status | config_mode");
    Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::loop() {
    // ── 1. Leer Serial y parsear comandos ─────────────────────────────────────
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (_parser.feed(c)) {
            _handleCommand(_parser.getCommand());
        }
    }

    // ── 2. Sondear radio ──────────────────────────────────────────────────────
    TestPacket pkt;
    if (_lora->poll(pkt)) {
        if (pkt.msg_type == MSG_PONG && _waitingPong &&
            (pkt.dest_id == _cfg->node_id || pkt.dest_id == NODE_BROADCAST_ID)) {
            _waitingPong = false;
            ledBlink(1, 50);

            uint32_t rtt  = (uint32_t)(millis() - pkt.timestamp);
            float    rssi = _lora->getLastRSSI();
            float    snr  = _lora->getLastSNR();

            _metrics->recordReceived(rssi, snr, rtt);

            Serial.printf("[RX] Pong #%04lu | RSSI: %.1f dBm | SNR: %.1f dB | RTT: %lu ms\n",
                          (unsigned long)pkt.packet_id, rssi, snr, (unsigned long)rtt);
            if (pkt.batteryPercent == BAT_PERCENT_UNAVAILABLE || pkt.batteryVoltage < 0.0f) {
                Serial.print("     Bat: N/D");
            } else {
                Serial.printf("     Bat: %.2fV (%d%%)", pkt.batteryVoltage, pkt.batteryPercent);
                if (pkt.batteryPercent < 20) Serial.print("  [!] BATERIA BAJA");
            }
            if (pkt.nitrogen != NPK_VALUE_UNAVAILABLE &&
                pkt.phosphorus != NPK_VALUE_UNAVAILABLE &&
                pkt.potassium != NPK_VALUE_UNAVAILABLE) {
                Serial.printf(" | NPK: N=%u P=%u K=%u mg/kg",
                              pkt.nitrogen, pkt.phosphorus, pkt.potassium);
            }
            if (!isnan(pkt.tempAmbient)) {
                Serial.printf(" | Tamb=%.1f°C Hum=%.1f%%", pkt.tempAmbient, pkt.humidity);
            }
            if (!isnan(pkt.tempProbe)) {
                Serial.printf(" | Tsonda=%.1f°C", pkt.tempProbe);
            }
            if (pkt.watermarkCb != WM_VALUE_UNAVAILABLE) {
                Serial.printf(" | WM=%d cb", pkt.watermarkCb);
            }
            Serial.println();

            if (_rangeActive) {
                if (_rangeSent < _rangeTotal) {
                    delay(TX_SPACING_MS);
                    _sendNextPing();
                } else {
                    _printRangeSummary();
                    _rangeActive = false;
                }
            }
        } else if (pkt.msg_type == MSG_NODE_STATUS &&
                   (pkt.dest_id == _cfg->node_id || pkt.dest_id == NODE_BROADCAST_ID)) {
            _handleNodeStatus(pkt);
        }
    }

    // ── 2.5. Beacon HELLO periódico para discovery mesh ──────────────────────
    if (_cfg->mesh_beacon_ms > 0 && !_waitingPong &&
        (millis() - _lastHelloMs) >= _cfg->mesh_beacon_ms) {
        _lastHelloMs = millis();
        _sendHello();
    }

    // ── 3. Auto-ping periódico ────────────────────────────────────────────────
    const uint32_t effectiveAutoPing =
        (_cfg->mesh_max_hops > 1 && _cfg->auto_ping_ms > 0 && _cfg->auto_ping_ms < 3200)
            ? 3200
            : _cfg->auto_ping_ms;

    if (effectiveAutoPing > 0 && !_waitingPong && !_rangeActive &&
        (millis() - _lastAutoPing) >= effectiveAutoPing) {
        _lastAutoPing = millis();
        _sendNextPing();
    }

    // ── 4. Timeout de PONG ────────────────────────────────────────────────────
    const uint32_t effectivePingTimeout =
        (_cfg->mesh_max_hops > 1 && _cfg->ping_timeout_ms < 4500)
            ? 4500
            : _cfg->ping_timeout_ms;

    if (_waitingPong && (millis() - _pingTimestamp) > effectivePingTimeout) {
        _waitingPong = false;
        Serial.printf("[GW] Timeout esperando Pong #%04lu\n", (unsigned long)_pingId);

        if (_rangeActive) {
            if (_rangeSent < _rangeTotal) {
                delay(TX_SPACING_MS);
                _sendNextPing();
            } else {
                _printRangeSummary();
                _rangeActive = false;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::_sendNextPing() {
    _pingId++;
    _pingTimestamp = millis();
    _waitingPong   = true;

    _metrics->recordSent();
    if (_rangeActive) _rangeSent++;

    if (_lora->sendPing(_cfg->node_id, NODE_BROADCAST_ID, _pingId,
                        _cfg->mesh_max_hops)) {
        Serial.printf("[TX] Ping #%04lu enviado...\n", (unsigned long)_pingId);
    } else {
        _waitingPong = false;
        Serial.printf("[ERR] Fallo al enviar Ping #%04lu\n", (unsigned long)_pingId);
    }
    _lora->listen();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::_sendHello() {
    _helloSeq++;
    if (_lora->sendHello(_cfg->node_id, _helloSeq, _cfg->mesh_max_hops)) {
        Serial.printf("[MESH] HELLO #%04lu broadcast\n", (unsigned long)_helloSeq);
    } else {
        Serial.printf("[MESH] Error enviando HELLO #%04lu\n", (unsigned long)_helloSeq);
    }
    _lora->listen();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::_handleNodeStatus(const TestPacket& pkt) {
    int freeIdx = -1;
    int idx = -1;

    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (_neighbors[i].used && _neighbors[i].node_id == pkt.source_id) {
            idx = i;
            break;
        }
        if (!_neighbors[i].used && freeIdx < 0) {
            freeIdx = i;
        }
    }

    if (idx < 0) {
        idx = freeIdx;
        if (idx < 0) {
            Serial.printf("[MESH] Tabla vecinos llena. Nodo 0x%08lX descartado\n",
                          (unsigned long)pkt.source_id);
            return;
        }
        _neighbors[idx].used = true;
        _neighbors[idx].node_id = pkt.source_id;
    }

    _neighbors[idx].last_rssi   = _lora->getLastRSSI();
    _neighbors[idx].last_snr    = _lora->getLastSNR();
    _neighbors[idx].battery_v   = pkt.batteryVoltage;
    _neighbors[idx].battery_pct = pkt.batteryPercent;
    _neighbors[idx].nitrogen    = pkt.nitrogen;
    _neighbors[idx].phosphorus  = pkt.phosphorus;
    _neighbors[idx].potassium   = pkt.potassium;
    _neighbors[idx].temp_ambient = pkt.tempAmbient;
    _neighbors[idx].humidity    = pkt.humidity;
    _neighbors[idx].temp_probe  = pkt.tempProbe;
    _neighbors[idx].watermark_cb = pkt.watermarkCb;
    _neighbors[idx].hops        = pkt.hop_count;
    _neighbors[idx].last_seen_ms = millis();

    auto& nb = _neighbors[idx];
    Serial.printf("[MESH] Nodo 0x%08lX | RSSI %.1f | SNR %.1f | hops %u",
                  (unsigned long)pkt.source_id, nb.last_rssi, nb.last_snr, nb.hops);

    if (nb.battery_pct == BAT_PERCENT_UNAVAILABLE || nb.battery_v < 0.0f) {
        Serial.print(" | Bat N/D");
    } else {
        Serial.printf(" | Bat %.2fV (%u%%)", nb.battery_v, nb.battery_pct);
    }

    if (nb.nitrogen != NPK_VALUE_UNAVAILABLE &&
        nb.phosphorus != NPK_VALUE_UNAVAILABLE &&
        nb.potassium != NPK_VALUE_UNAVAILABLE) {
        Serial.printf(" | N=%u P=%u K=%u mg/kg", nb.nitrogen, nb.phosphorus, nb.potassium);
    }

    if (!isnan(nb.temp_ambient)) {
        Serial.printf(" | Tamb=%.1f°C Hum=%.1f%%", nb.temp_ambient, nb.humidity);
    }
    if (!isnan(nb.temp_probe)) {
        Serial.printf(" | Tsonda=%.1f°C", nb.temp_probe);
    }
    if (nb.watermark_cb != WM_VALUE_UNAVAILABLE) {
        Serial.printf(" | WM=%d cb", nb.watermark_cb);
    }

    Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::_handleCommand(const Command& cmd) {
    switch (cmd.type) {
        case CommandType::PING:
            if (_waitingPong) {
                Serial.println("[WARN] Ya hay un ping pendiente.");
                return;
            }
            _sendNextPing();
            break;

        case CommandType::RANGE_TEST:
            if (_rangeActive) {
                Serial.println("[WARN] Ya hay un range_test en curso.");
                return;
            }
            _metrics->reset();
            _rangeActive = true;
            _rangeTotal  = cmd.range_n;
            _rangeSent   = 0;
            Serial.printf("[GW] Iniciando range_test: %d paquetes\n", cmd.range_n);
            _sendNextPing();
            break;

        case CommandType::CONFIG:
            Serial.printf("[GW] Reconfigurando: SF=%d BW=%.0f CR=4/%d PWR=%d dBm\n",
                          cmd.cfg_sf, cmd.cfg_bw, cmd.cfg_cr, cmd.cfg_pwr);
            if (_lora->reconfigure(cmd.cfg_sf, cmd.cfg_bw, cmd.cfg_cr, cmd.cfg_pwr)) {
                Serial.println("[OK]  Reconfiguración exitosa.");
            } else {
                Serial.println("[ERR] Fallo en reconfiguración.");
            }
            break;

        case CommandType::STATUS:
            _printStatus();
            break;

        case CommandType::CONFIG_MODE:
            Serial.println("[GW] Entrando a Config Mode... (reiniciando)");
            delay(500);
            if (!ConfigManager::requestConfigModeOnce()) {
                Serial.println("[ERR] No se pudo programar Config Mode en NVS");
                return;
            }
            ESP.restart();
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::_printRangeSummary() {
    RangeSummary s = _metrics->getSummary();
    Serial.println("\n=== Range Test Results ===");
    Serial.printf("Paquetes enviados  : %lu\n",    (unsigned long)s.sent);
    Serial.printf("Paquetes recibidos : %lu\n",    (unsigned long)s.received);
    Serial.printf("Perdida            : %.1f%%\n", s.packet_loss_pct);
    if (s.received > 0) {
        Serial.printf("RSSI   min/max/avg : %.1f / %.1f / %.1f dBm\n",
                      s.rssi_min, s.rssi_max, s.rssi_avg);
        Serial.printf("SNR    min/max/avg : %.1f / %.1f / %.1f dB\n",
                      s.snr_min,  s.snr_max,  s.snr_avg);
        Serial.printf("RTT    min/max/avg : %lu / %lu / %lu ms\n",
                      (unsigned long)s.rtt_min_ms,
                      (unsigned long)s.rtt_max_ms,
                      (unsigned long)s.rtt_avg_ms);
    }
    Serial.println("==========================\n");
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeGateway::_printStatus() {
    Serial.println("\n=== Status ===");
    Serial.printf("Nodo     : %s (ID 0x%08lX)\n", _cfg->node_name, (unsigned long)_cfg->node_id);
    Serial.printf("Rol      : %s\n",  roleToStr(_cfg->role));
    Serial.printf("Freq     : %.1f MHz\n", _lora->getFreq());
    Serial.printf("SF       : %d\n",       _lora->getSF());
    Serial.printf("BW       : %.0f kHz\n", _lora->getBW());
    Serial.printf("CR       : 4/%d\n",     _lora->getCR());
    Serial.printf("Power    : %d dBm\n",   _lora->getPower());
    Serial.printf("Uptime   : %lu s\n",    (unsigned long)(millis() / 1000));
    if (_metrics->getPacketsSent() > 0) {
        Serial.printf("Enviados : %lu\n",       (unsigned long)_metrics->getPacketsSent());
        Serial.printf("Recibidos: %lu\n",       (unsigned long)_metrics->getPacketsReceived());
        Serial.printf("Ult RSSI : %.1f dBm\n",  _metrics->getLastRSSI());
        Serial.printf("Ult SNR  : %.1f dB\n",   _metrics->getLastSNR());
        Serial.printf("Ult RTT  : %lu ms\n",    (unsigned long)_metrics->getLastRTT());
    }

    uint8_t neighborsCount = 0;
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (_neighbors[i].used) neighborsCount++;
    }
    Serial.printf("Vecinos  : %u\n", neighborsCount);

    Serial.println("==============\n");
}
