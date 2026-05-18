#pragma once

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// PINES — Conector B2B interno (WIO-SX1262 shield sobre XIAO ESP32S3)
// FIJO — NO modificar
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_NSS    41   // CS
#define PIN_SCK    7    // SCK
#define PIN_MOSI   9    // MOSI
#define PIN_MISO   8    // MISO
#define PIN_RST    42   // RST
#define PIN_DIO1   39   // DIO1 (IRQ)
#define PIN_BUSY   40   // BUSY
#define PIN_ANT_SW 38   // Switch de antena — HIGH para habilitar TX/RX

// LED de usuario del shield Wio-SX1262
#define PIN_LED    48

// Botón de usuario del shield Wio-SX1262
#define PIN_USER_BTN 21

// Botón BOOT — mantener presionado al encender para entrar a Config Mode
#define PIN_BOOT   0

// ─────────────────────────────────────────────────────────────────────────────
// RS485 + sensor NPK (según la guía de cableado)
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_RS485_EN      6    // D5 / GPIO6
#define PIN_RS485_RX     44    // D7 / GPIO44
#define PIN_RS485_TX     43    // D6 / GPIO43

#define NPK_SENSOR_BAUD  4800
#define NPK_SENSOR_ADDR  0x01
#define NPK_SENSOR_TIMEOUT_MS  1000
#define NPK_VALUE_UNAVAILABLE  0xFFFF

// ─────────────────────────────────────────────────────────────────────────────
// NPK + MAX485 — Control de alimentación para ahorro de energía
// GPIO4 (D3) controla VCC del módulo MAX485 y sensor NPK via transistor PNP
// (BC557 o similar). Lógica activa-LOW: LOW=encendido, HIGH=apagado.
//
// Circuito:
//   3.3V ──────────────── Emisor BC557
//   GPIO4 ──[1kΩ]──────► Base BC557
//                         Colector ──► VCC MAX485 + VCC NPK
//   GND ────────────────► GND MAX485 + GND NPK
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_NPK_PWR        4    // GPIO4 / D3 — base del BC557 (via 1kΩ)
#define NPK_PWR_ON         LOW  // PNP activo-LOW
#define NPK_PWR_OFF        HIGH
#define NPK_PWR_WARMUP_MS  500  // ms tras encender antes de leer

// ─────────────────────────────────────────────────────────────────────────────
// One-Wire — DS18B20 (sonda de temperatura)
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_ONE_WIRE 2    // GPIO2 / D1

// ─────────────────────────────────────────────────────────────────────────────
// SENSOR WATERMARK 200SS — tensión de agua en suelo (divisor resistivo pseudo-AC)
//
// Circuito de hardware requerido:
//   PIN_WM_A ---[Sensor WM]--- PIN_WM_ADC ---[R1 10kΩ ext]--- PIN_WM_B
//
//   PIN_WM_A   = GPIO1  (D0)  — excitación polaridad A (salida digital)
//   PIN_WM_ADC = GPIO3  (D2)  — punto de medición ADC (ADC1_CH2)
//   PIN_WM_B   = GPIO5  (D4)  — excitación polaridad B (salida digital)
//                               (Reasignado de GPIO21 para evitar conflicto con botón del shield)
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_WM_A        1    // GPIO1  / D0  — excitación A
#define PIN_WM_ADC      3    // GPIO3  / D2  — ADC de medición
#define PIN_WM_B        5    // GPIO5  / D4  — excitación B

#define WM_SERIES_R  10000   // Ω — resistencia de serie del divisor de tensión
#define WM_VALUE_UNAVAILABLE  ((int16_t)-1)  // Centibares no disponible / falla

// ─────────────────────────────────────────────────────────────────────────────
// MONITOREO DE BATERÍA — Nodo Remote
// El pad BAT solo entrega alimentación de la celda; para medir voltaje hace
// falta un punto de sense hacia un ADC del ESP32S3.
// En este firmware se usa GPIO1/D0 como entrada ADC cuando existe ese divisor.
// ─────────────────────────────────────────────────────────────────────────────
#define BATTERY_SENSE_ENABLED   0    // Alimentado por pin 5V via boost converter — BAT pad no usado
#define PIN_BAT_ADC             1    // GPIO1 = D0 (ADC) — sin usar con sensing deshabilitado
#define BAT_ADC_SAMPLES        10    // Muestras para promediar
#define BAT_V_MIN             3.2f   // Voltaje mínimo de batería (0%)
#define BAT_V_MAX             4.2f   // Voltaje máximo de batería (100%)
#define BAT_ADC_CORRECTION    1.238f  // Calibrado con multímetro en este HW
#define BAT_PERCENT_UNAVAILABLE 255   // Valor reservado cuando no hay lectura

// ─────────────────────────────────────────────────────────────────────────────
// PROTOCOLO DE PAQUETES
// ─────────────────────────────────────────────────────────────────────────────
#define MSG_PING         0x01
#define MSG_PONG         0x02
#define MSG_HELLO        0x03  // Beacon de descubrimiento mesh
#define MSG_NODE_STATUS  0x04  // Respuesta con estado de nodo (batería)

// Dirección broadcast para tramas de red
#define NODE_BROADCAST_ID  0xFFFFFFFFUL

// ─────────────────────────────────────────────────────────────────────────────
// TIMING — constantes fijas de bajo nivel
// ─────────────────────────────────────────────────────────────────────────────
#define TX_SPACING_MS  200   // ms mínimo entre transmisiones consecutivas

// Jitter para evitar colisiones en reenvío y respuestas de nodos mesh
#define ROUTER_FWD_JITTER_MIN_MS   40
#define ROUTER_FWD_JITTER_MAX_MS  140
#define ROUTER_FWD_REQ_MIN_MS     180
#define ROUTER_FWD_REQ_MAX_MS     320
#define ROUTER_FWD_RESP_MIN_MS    220
#define ROUTER_FWD_RESP_MAX_MS    420
#define REMOTE_TX_JITTER_MIN_MS    20
#define REMOTE_TX_JITTER_MAX_MS   120

// ─────────────────────────────────────────────────────────────────────────────
// TCXO — requerido para WIO-SX1262
// ─────────────────────────────────────────────────────────────────────────────
#define LORA_TCXO_V  1.8f

// ─────────────────────────────────────────────────────────────────────────────
// SERIAL
// ─────────────────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200

// ─────────────────────────────────────────────────────────────────────────────
// HELPERS DE LED — inline para uso compartido entre node_gateway y node_remote
// ─────────────────────────────────────────────────────────────────────────────
inline void ledOn()  { digitalWrite(PIN_LED, HIGH); }
inline void ledOff() { digitalWrite(PIN_LED, LOW);  }

inline void ledBlink(uint8_t times = 1, uint32_t ms = 80) {
    for (uint8_t i = 0; i < times; i++) {
        ledOn();  delay(ms);
        ledOff(); if (i < times - 1) delay(ms);
    }
}
