/**
 * @file    main.cpp
 * @brief   AgroXiao — MVP Red LoRa Bidireccional
 *
 * Nodo Gateway (LORA_NODE_GATEWAY=1):
 *   Comandos Serial disponibles:
 *     ping                          → envía PING, espera PONG, imprime RTT/RSSI/SNR
 *     range_test <N>                → envía N pings y muestra estadísticas
 *     config <sf> <bw> <cr> <pwr>  → reconfigura LoRa en vivo
 *     status                        → muestra config actual + última métrica
 *
 * Nodo Remote (LORA_NODE_GATEWAY=0):
 *   Responde automáticamente con PONG a cada PING recibido.
 *   LED integrado parpadea en cada TX/RX.
 *
 * Compilar/flashear:
 *   pio run -e gateway --target upload
 *   pio run -e remote  --target upload
 */

#include <Arduino.h>
#include "config.h"
#include "lora_manager.h"
#include "metrics.h"

#if LORA_NODE_GATEWAY
#include "command_parser.h"
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Instancias de módulos
// ─────────────────────────────────────────────────────────────────────────────
static LoRaManager      lora;
static MetricsCollector metrics;

#if LORA_NODE_GATEWAY
static CommandParser parser;

// Auto-ping cada AUTO_PING_MS ms (0 = desactivado)
#define AUTO_PING_MS 2000

// Estado de la máquina de pings
static uint32_t _pingId        = 0;
static bool     _waitingPong   = false;
static uint32_t _pingTimestamp = 0;   // millis() al enviar el PING
static uint32_t _lastAutoPing  = 0;

// Estado del range_test
static bool     _rangeActive   = false;
static uint16_t _rangeTotal    = 0;
static uint16_t _rangeSent     = 0;

// ─── Declaraciones forward (Gateway) ─────────────────────────────────────────
static void handleCommand(const Command& cmd);
static void sendNextPing();
static void printRangeSummary();
static void printStatus();
#endif // LORA_NODE_GATEWAY

// ─────────────────────────────────────────────────────────────────────────────
// Helpers de LED
// ─────────────────────────────────────────────────────────────────────────────
static inline void ledOn()  { digitalWrite(PIN_LED, HIGH); }
static inline void ledOff() { digitalWrite(PIN_LED, LOW);  }
static void ledBlink(uint8_t times = 1, uint32_t ms = 80) {
    for (uint8_t i = 0; i < times; i++) {
        ledOn();  delay(ms);
        ledOff(); if (i < times - 1) delay(ms);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1500);

    pinMode(PIN_LED, OUTPUT);
    ledOff();

    Serial.println("═══════════════════════════════════════════");
#if LORA_NODE_GATEWAY
    Serial.println("  AgroXiao — GATEWAY  |  RadioLib + SX1262");
#else
    Serial.println("  AgroXiao — REMOTE   |  RadioLib + SX1262");
#endif
    Serial.println("═══════════════════════════════════════════");
    Serial.printf("  Freq  : %.1f MHz\n", LORA_FREQ);
    Serial.printf("  SF    : %d\n",        LORA_SF);
    Serial.printf("  BW    : %.0f kHz\n",  LORA_BW);
    Serial.printf("  CR    : 4/%d\n",      LORA_CR);
    Serial.printf("  Power : %d dBm\n",    LORA_POWER);
    Serial.println("═══════════════════════════════════════════\n");

    Serial.println("Inicializando SX1262...");
    if (!lora.begin()) {
        Serial.println("[FATAL] Inicialización fallida. Revisá el cableado y reiniciá.");
        while (true) { delay(1000); }
    }

    if (!lora.listen()) {
        Serial.println("[FATAL] No se pudo iniciar recepción.");
        while (true) { delay(1000); }
    }

    ledBlink(3);

#if LORA_NODE_GATEWAY
    Serial.println("[GW]  Listo. Comandos: ping | range_test <N> | config <sf> <bw> <cr> <pwr> | status");
#else
    Serial.println("[REM] Listo. Escuchando PINGs...");
#endif
    Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────
void loop() {

#if LORA_NODE_GATEWAY
    // ── 1. Leer Serial y parsear comandos ─────────────────────────────────────
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (parser.feed(c)) {
            handleCommand(parser.getCommand());
        }
    }

    // ── 2. Sondear radio ──────────────────────────────────────────────────────
    TestPacket pkt;
    if (lora.poll(pkt)) {
        if (pkt.msg_type == MSG_PONG && _waitingPong) {
            _waitingPong = false;
            ledBlink(1, 50);

            uint32_t rtt = (uint32_t)(millis() - pkt.timestamp);
            float    rssi = lora.getLastRSSI();
            float    snr  = lora.getLastSNR();

            metrics.recordReceived(rssi, snr, rtt);

            Serial.printf("[RX] Pong #%04lu | RSSI: %.1f dBm | SNR: %.1f dB | RTT: %lu ms\n",
                          (unsigned long)pkt.packet_id,
                          rssi, snr, (unsigned long)rtt);

            // Continuar range_test si está activo
            if (_rangeActive) {
                if (_rangeSent < _rangeTotal) {
                    delay(TX_SPACING_MS);
                    sendNextPing();
                } else {
                    printRangeSummary();
                    _rangeActive = false;
                }
            }
        }
    }

    // ── 3. Auto-ping periódico ────────────────────────────────────────────────
    if (AUTO_PING_MS > 0 && !_waitingPong && !_rangeActive &&
        (millis() - _lastAutoPing) >= AUTO_PING_MS) {
        _lastAutoPing = millis();
        sendNextPing();
    }

    // ── 4. Timeout de PONG ────────────────────────────────────────────────────
    if (_waitingPong && (millis() - _pingTimestamp) > PING_TIMEOUT_MS) {
        _waitingPong = false;
        Serial.printf("[TX] Timeout esperando Pong #%04lu\n", (unsigned long)_pingId);

        if (_rangeActive) {
            if (_rangeSent < _rangeTotal) {
                delay(TX_SPACING_MS);
                sendNextPing();
            } else {
                printRangeSummary();
                _rangeActive = false;
            }
        }
    }

#else // ─── MODO REMOTE ────────────────────────────────────────────────────────

    TestPacket pkt;
    if (lora.poll(pkt)) {
        if (pkt.msg_type == MSG_PING) {
            ledBlink(1, 30);  // LED breve al recibir

            Serial.printf("[RX] Ping #%04lu recibido | RSSI: %.1f dBm | SNR: %.1f dB\n",
                          (unsigned long)pkt.packet_id,
                          lora.getLastRSSI(), lora.getLastSNR());

            delay(TX_SPACING_MS);

            if (lora.sendPong(pkt.packet_id, pkt.timestamp)) {
                Serial.printf("[TX] Pong #%04lu enviado\n", (unsigned long)pkt.packet_id);
                ledBlink(2, 30);  // Doble parpadeo al responder
            }

            lora.listen();
        }
    }

#endif // LORA_NODE_GATEWAY
}

// ─────────────────────────────────────────────────────────────────────────────
// Funciones auxiliares Gateway
// ─────────────────────────────────────────────────────────────────────────────
#if LORA_NODE_GATEWAY

static void sendNextPing() {
    _pingId++;
    _pingTimestamp = millis();
    _waitingPong   = true;

    metrics.recordSent();
    if (_rangeActive) _rangeSent++;

    if (lora.sendPing(_pingId)) {
        Serial.printf("[TX] Ping #%04lu enviado...\n", (unsigned long)_pingId);
    } else {
        _waitingPong = false;
        Serial.printf("[ERR] Fallo al enviar Ping #%04lu\n", (unsigned long)_pingId);
    }
    lora.listen();
}

static void handleCommand(const Command& cmd) {
    switch (cmd.type) {
        case CommandType::PING:
            if (_waitingPong) {
                Serial.println("[WARN] Ya hay un ping pendiente. Esperá la respuesta.");
                return;
            }
            sendNextPing();
            break;

        case CommandType::RANGE_TEST:
            if (_rangeActive) {
                Serial.println("[WARN] Ya hay un range_test en curso.");
                return;
            }
            metrics.reset();
            _rangeActive = true;
            _rangeTotal  = cmd.range_n;
            _rangeSent   = 0;
            Serial.printf("[GW] Iniciando range_test: %d paquetes\n", cmd.range_n);
            sendNextPing();
            break;

        case CommandType::CONFIG:
            Serial.printf("[GW] Reconfigurando: SF=%d BW=%.0f CR=4/%d PWR=%d dBm\n",
                          cmd.cfg_sf, cmd.cfg_bw, cmd.cfg_cr, cmd.cfg_pwr);
            if (lora.reconfigure(cmd.cfg_sf, cmd.cfg_bw, cmd.cfg_cr, cmd.cfg_pwr)) {
                Serial.println("[OK]  Reconfiguración exitosa.");
            } else {
                Serial.println("[ERR] Fallo en reconfiguración.");
            }
            break;

        case CommandType::STATUS:
            printStatus();
            break;

        default:
            break;
    }
}

static void printRangeSummary() {
    RangeSummary s = metrics.getSummary();
    Serial.println("\n=== Range Test Results ===");
    Serial.printf("Paquetes enviados  : %lu\n",   (unsigned long)s.sent);
    Serial.printf("Paquetes recibidos : %lu\n",   (unsigned long)s.received);
    Serial.printf("Pérdida            : %.1f%%\n", s.packet_loss_pct);
    if (s.received > 0) {
        Serial.printf("RSSI   min/max/avg : %.1f / %.1f / %.1f dBm\n",
                      s.rssi_min, s.rssi_max, s.rssi_avg);
        Serial.printf("SNR    min/max/avg : %.1f / %.1f / %.1f dB\n",
                      s.snr_min, s.snr_max, s.snr_avg);
        Serial.printf("RTT    min/max/avg : %lu / %lu / %lu ms\n",
                      (unsigned long)s.rtt_min_ms,
                      (unsigned long)s.rtt_max_ms,
                      (unsigned long)s.rtt_avg_ms);
    }
    Serial.println("==========================\n");
}

static void printStatus() {
    Serial.println("\n=== Status ===");
    Serial.printf("Modo     : GATEWAY\n");
    Serial.printf("Freq     : %.1f MHz\n",  lora.getFreq());
    Serial.printf("SF       : %d\n",         lora.getSF());
    Serial.printf("BW       : %.0f kHz\n",  lora.getBW());
    Serial.printf("CR       : 4/%d\n",       lora.getCR());
    Serial.printf("Power    : %d dBm\n",     lora.getPower());
    Serial.printf("Uptime   : %lu s\n",      (unsigned long)(millis() / 1000));
    if (metrics.getPacketsSent() > 0) {
        Serial.printf("Enviados : %lu\n",    (unsigned long)metrics.getPacketsSent());
        Serial.printf("Recibidos: %lu\n",    (unsigned long)metrics.getPacketsReceived());
        Serial.printf("Último RSSI: %.1f dBm\n", metrics.getLastRSSI());
        Serial.printf("Último SNR : %.1f dB\n",  metrics.getLastSNR());
        Serial.printf("Último RTT : %lu ms\n",   (unsigned long)metrics.getLastRTT());
    }
    Serial.println("==============\n");
}

#endif // LORA_NODE_GATEWAY
