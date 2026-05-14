#include "node_remote.h"
#include "core/config_manager.h"
#include "core/config.h"

#include <esp_system.h>

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::init(const NodeConfig& cfg, LoRaManager& lora) {
    _cfg  = &cfg;
    _lora = &lora;

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    randomSeed((uint32_t)esp_random());
    _npk.begin();
    _env.begin();

    Serial.printf("[REM] Nodo: %s (ID 0x%08lX)\n",
                  cfg.node_name, (unsigned long)cfg.node_id);
    Serial.printf("[REM] RS485 NPK: UART GPIO%d/GPIO%d, EN GPIO%d\n",
                  PIN_RS485_TX, PIN_RS485_RX, PIN_RS485_EN);
    Serial.printf("[REM] I2C ENV: SDA GPIO%d, SCL GPIO%d\n", PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.printf("[REM] ADC batería en GPIO%d\n", PIN_BAT_ADC);
    Serial.println("[REM] Escuchando PINGs... (comando serial: config_mode)");
    Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::loop() {
    // Chequear comando serial mínimo: solo "config_mode"
    if (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            _lineBuf[_linePos] = '\0';
            if (strcmp(_lineBuf, "config_mode") == 0) {
                Serial.println("[REM] Entrando a Config Mode...");
                delay(300);
                if (!ConfigManager::requestConfigModeOnce()) {
                    Serial.println("[ERR] No se pudo programar Config Mode en NVS");
                    _linePos = 0;
                    return;
                }
                ESP.restart();
            } else if (strcmp(_lineBuf, "npk") == 0 || strcmp(_lineBuf, "diag") == 0) {
                _printSensorDiagnostic();
            }
            _linePos = 0;
        } else if (_linePos < sizeof(_lineBuf) - 1) {
            _lineBuf[_linePos++] = c;
        }
    }

    // Sondear radio
    TestPacket pkt;
    if (_lora->poll(pkt)) {
        if (_isDuplicateAndMark(pkt)) {
            return;
        }

        if (pkt.msg_type == MSG_PING &&
            (pkt.dest_id == _cfg->node_id || pkt.dest_id == NODE_BROADCAST_ID)) {
            ledBlink(1, 30);

            Serial.printf("[RX] Ping #%04lu | RSSI: %.1f dBm | SNR: %.1f dB\n",
                          (unsigned long)pkt.packet_id,
                          _lora->getLastRSSI(), _lora->getLastSNR());

            float   batV   = _readBatteryVoltage();
            uint8_t batPct = _getBatteryPercent(batV);
            uint16_t n = NPK_VALUE_UNAVAILABLE;
            uint16_t p = NPK_VALUE_UNAVAILABLE;
            uint16_t k = NPK_VALUE_UNAVAILABLE;
            _readNpk(n, p, k);
            EnvReading env = _readEnv();
            Serial.printf("[BAT] %.2fV (%d%%)\n", batV, batPct);
            if (n == NPK_VALUE_UNAVAILABLE || p == NPK_VALUE_UNAVAILABLE || k == NPK_VALUE_UNAVAILABLE) {
                Serial.println("[NPK] N/D");
            } else {
                Serial.printf("[NPK] N=%u mg/kg | P=%u mg/kg | K=%u mg/kg\n", n, p, k);
            }
            if (!isnan(env.tempAmbient)) {
                Serial.printf("[AHT] Tamb=%.1f°C | Hum=%.1f%%\n", env.tempAmbient, env.humidity);
            }
            if (!isnan(env.tempProbe)) {
                Serial.printf("[SHT] Tsonda=%.1f°C\n", env.tempProbe);
            }

            delay(TX_SPACING_MS + random(REMOTE_TX_JITTER_MIN_MS,
                                         REMOTE_TX_JITTER_MAX_MS + 1));

            if (_lora->sendPong(_cfg->node_id, pkt.source_id,
                                pkt.packet_id, pkt.timestamp,
                                batV, batPct, n, p, k,
                                env.tempAmbient, env.humidity, env.tempProbe,
                                pkt.hop_limit)) {
                Serial.printf("[TX] Pong #%04lu enviado\n", (unsigned long)pkt.packet_id);
                ledBlink(2, 30);
            }

            _lora->listen();
        } else if (pkt.msg_type == MSG_HELLO &&
                   (pkt.dest_id == _cfg->node_id || pkt.dest_id == NODE_BROADCAST_ID)) {
            float   batV   = _readBatteryVoltage();
            uint8_t batPct = _getBatteryPercent(batV);
            uint16_t n = NPK_VALUE_UNAVAILABLE;
            uint16_t p = NPK_VALUE_UNAVAILABLE;
            uint16_t k = NPK_VALUE_UNAVAILABLE;
            _readNpk(n, p, k);
            EnvReading env = _readEnv();

            delay(TX_SPACING_MS + random(REMOTE_TX_JITTER_MIN_MS,
                                         REMOTE_TX_JITTER_MAX_MS + 1));
            if (_lora->sendNodeStatus(_cfg->node_id, pkt.source_id,
                                      pkt.packet_id, batV, batPct, n, p, k,
                                      env.tempAmbient, env.humidity, env.tempProbe,
                                      0, pkt.hop_limit)) {
                Serial.printf("[MESH] STATUS -> 0x%08lX | Bat %.2fV (%u%%)",
                              (unsigned long)pkt.source_id, batV, batPct);
                if (n != NPK_VALUE_UNAVAILABLE) {
                    Serial.printf(" | N=%u P=%u K=%u", n, p, k);
                }
                if (!isnan(env.tempAmbient)) {
                    Serial.printf(" | Tamb=%.1f°C Hum=%.1f%%", env.tempAmbient, env.humidity);
                }
                if (!isnan(env.tempProbe)) {
                    Serial.printf(" | Tsonda=%.1f°C", env.tempProbe);
                }
                Serial.println();
            }
            _lora->listen();
        }
    }

    if (millis() - _lastDiagMs >= 10000) {
        _lastDiagMs = millis();
        _printSensorDiagnostic();
    }
}

bool NodeRemote::_isDuplicateAndMark(const TestPacket& pkt) {
    if (pkt.msg_type != MSG_PING && pkt.msg_type != MSG_HELLO) {
        return false;
    }

    return PacketDedupe::isDuplicateAndMark(_seen, pkt, SEEN_TTL_MS);
}

// ─────────────────────────────────────────────────────────────────────────────
float NodeRemote::_readBatteryVoltage() {
    if (!BATTERY_SENSE_ENABLED) {
        return -1.0f;
    }

    uint32_t sum = 0;
    for (uint8_t i = 0; i < BAT_ADC_SAMPLES; i++) {
        sum += analogReadMilliVolts(PIN_BAT_ADC);
    }
    float vADC_mv = (float)sum / BAT_ADC_SAMPLES;
    return (vADC_mv / 1000.0f) * 2.0f * BAT_ADC_CORRECTION;
}

// ─────────────────────────────────────────────────────────────────────────────
uint8_t NodeRemote::_getBatteryPercent(float voltage) {
    if (voltage < 0.0f) return BAT_PERCENT_UNAVAILABLE;
    if (voltage <= BAT_V_MIN) return 0;
    if (voltage >= BAT_V_MAX) return 100;
    return (uint8_t)((voltage - BAT_V_MIN) / (BAT_V_MAX - BAT_V_MIN) * 100.0f + 0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::_printSensorDiagnostic() {
    float batV = _readBatteryVoltage();
    uint8_t batPct = _getBatteryPercent(batV);
    uint16_t n = NPK_VALUE_UNAVAILABLE;
    uint16_t p = NPK_VALUE_UNAVAILABLE;
    uint16_t k = NPK_VALUE_UNAVAILABLE;
    _readNpk(n, p, k);
    EnvReading env = _readEnv();

    Serial.printf("[DIAG] BAT %.2fV (%u%%)", batV, batPct);
    if (n != NPK_VALUE_UNAVAILABLE) {
        Serial.printf(" | N=%u P=%u K=%u mg/kg", n, p, k);
    } else {
        Serial.print(" | NPK N/D");
    }
    if (!isnan(env.tempAmbient)) {
        Serial.printf(" | Tamb=%.1f°C Hum=%.1f%%", env.tempAmbient, env.humidity);
    }
    if (!isnan(env.tempProbe)) {
        Serial.printf(" | Tsonda=%.1f°C", env.tempProbe);
    }
    Serial.println();
}

EnvReading NodeRemote::_readEnv() {
    return _env.read();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::_readNpk(uint16_t& nitrogen, uint16_t& phosphorus, uint16_t& potassium) {
    if (!_npk.read(nitrogen, phosphorus, potassium)) {
        nitrogen = NPK_VALUE_UNAVAILABLE;
        phosphorus = NPK_VALUE_UNAVAILABLE;
        potassium = NPK_VALUE_UNAVAILABLE;
    }
}
