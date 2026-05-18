#include "node_remote.h"
#include "core/config_manager.h"
#include "core/config.h"

#include <esp_system.h>
#include <esp_sleep.h>

#define SLEEP_DURATION_S  10ULL

static uint32_t s_pktSeq = 0;

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::init(const NodeConfig& cfg, LoRaManager& lora) {
    _cfg  = &cfg;
    _lora = &lora;

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    randomSeed((uint32_t)esp_random());
    _npk.begin();
    _env.begin();
    _wm.begin();

    Serial.printf("[REM] Nodo: %s (ID 0x%08lX)\n",
                  cfg.node_name, (unsigned long)cfg.node_id);
    Serial.printf("[REM] WM200SS: A=GPIO%d ADC=GPIO%d B=GPIO%d R=%dΩ (pseudo-AC)\n",
                  PIN_WM_A, PIN_WM_ADC, PIN_WM_B, WM_SERIES_R);
    Serial.println("[REM] Modo: light sleep | cmd serial: config_mode, diag, scan");
    Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::loop() {
    // Chequear comandos seriales pendientes (no bloqueante)
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            _lineBuf[_linePos] = '\0';
            if (strcmp(_lineBuf, "config_mode") == 0) {
                Serial.println("[REM] Entrando a Config Mode...");
                delay(300);
                if (!ConfigManager::requestConfigModeOnce()) {
                    Serial.println("[ERR] No se pudo programar Config Mode en NVS");
                } else {
                    ESP.restart();
                }
            } else if (strcmp(_lineBuf, "npk") == 0 || strcmp(_lineBuf, "diag") == 0) {
                _printSensorDiagnostic();
            } else if (strcmp(_lineBuf, "scan") == 0) {
                _npk.scanBus();
            }
            _linePos = 0;
        } else if (_linePos < sizeof(_lineBuf) - 1) {
            _lineBuf[_linePos++] = c;
        }
    }

    // ── Leer todos los sensores ───────────────────────────────────────────────
    float    batV   = _readBatteryVoltage();
    uint8_t  batPct = _getBatteryPercent(batV);
    uint16_t n = NPK_VALUE_UNAVAILABLE;
    uint16_t p = NPK_VALUE_UNAVAILABLE;
    uint16_t k = NPK_VALUE_UNAVAILABLE;
    _readNpk(n, p, k);
    EnvReading env     = _readEnv();
    float soilTempC    = !isnan(env.tempProbe) ? env.tempProbe : 24.0f;
    int16_t wm         = _readWatermark(soilTempC);

    // ── Imprimir lecturas ─────────────────────────────────────────────────────
    Serial.printf("[BAT] %.2fV (%d%%)\n", batV, batPct);
    if (n == NPK_VALUE_UNAVAILABLE) {
        Serial.println("[NPK] N/D");
    } else {
        Serial.printf("[NPK] N=%u mg/kg | P=%u mg/kg | K=%u mg/kg\n", n, p, k);
    }
    if (!isnan(env.tempProbe)) {
        Serial.printf("[DS18B20] Tsonda=%.1f°C\n", env.tempProbe);
    }
    if (wm != WM_VALUE_UNAVAILABLE) {
        Serial.printf("[WM]  %d cb (%.1f°C suelo)\n", wm, soilTempC);
    } else {
        Serial.println("[WM]  N/D");
    }

    // ── Enviar status al broadcast ────────────────────────────────────────────
    s_pktSeq++;
    if (_lora->sendNodeStatus(_cfg->node_id, NODE_BROADCAST_ID, s_pktSeq,
                               batV, batPct, n, p, k,
                               env.tempAmbient, env.humidity, env.tempProbe,
                               wm, 0, 1)) {
        Serial.printf("[TX] NodeStatus #%04lu enviado\n", (unsigned long)s_pktSeq);
        ledBlink(1, 50);
    }

    // ── Light sleep — USB y RAM se conservan ──
    Serial.printf("[REM] Durmiendo %llus...\n\n", SLEEP_DURATION_S);
    Serial.flush();
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_S * 1000000ULL);
    esp_light_sleep_start();
}

// ─────────────────────────────────────────────────────────────────────────────
bool NodeRemote::_isDuplicateAndMark(const TestPacket& pkt) {
    if (pkt.msg_type != MSG_PING && pkt.msg_type != MSG_HELLO) {
        return false;
    }
    return PacketDedupe::isDuplicateAndMark(_seen, pkt, SEEN_TTL_MS);
}

// ─────────────────────────────────────────────────────────────────────────────
float NodeRemote::_readBatteryVoltage() {
    if (!BATTERY_SENSE_ENABLED) return -1.0f;

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
    float soilTempC = !isnan(env.tempProbe) ? env.tempProbe : 24.0f;
    int16_t wm = _readWatermark(soilTempC);

    if (batPct == BAT_PERCENT_UNAVAILABLE || batV < 0.0f) {
        Serial.print("[DIAG] BAT N/D");
    } else {
        Serial.printf("[DIAG] BAT %.2fV (%u%%)", batV, batPct);
    }
    if (n != NPK_VALUE_UNAVAILABLE) {
        Serial.printf(" | N=%u P=%u K=%u mg/kg", n, p, k);
    } else {
        Serial.print(" | NPK N/D");
    }
    if (!isnan(env.tempProbe)) {
        Serial.printf(" | Tsonda=%.1f°C", env.tempProbe);
    }
    if (wm != WM_VALUE_UNAVAILABLE) {
        Serial.printf(" | WM=%d cb", wm);
    } else {
        Serial.print(" | WM N/D");
    }
    Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
EnvReading NodeRemote::_readEnv() {
    return _env.read();
}

// ─────────────────────────────────────────────────────────────────────────────
void NodeRemote::_readNpk(uint16_t& nitrogen, uint16_t& phosphorus, uint16_t& potassium) {
    if (!_npk.read(nitrogen, phosphorus, potassium)) {
        nitrogen   = NPK_VALUE_UNAVAILABLE;
        phosphorus = NPK_VALUE_UNAVAILABLE;
        potassium  = NPK_VALUE_UNAVAILABLE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
int16_t NodeRemote::_readWatermark(float tempC) {
    return _wm.read(tempC);
}
