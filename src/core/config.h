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
// I2C — AHT10 (temp/hum ambiente) + SHT30 (sonda de temperatura)
// GPIO5 = D4 (SDA hardware libre), GPIO4 = D3 (SCL alternativo — D5/GPIO6 lo ocupa RS485)
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_I2C_SDA  5    // D4 / GPIO5
#define PIN_I2C_SCL  4    // D3 / GPIO4

// ─────────────────────────────────────────────────────────────────────────────
// MONITOREO DE BATERÍA — Nodo Remote
// El pad BAT solo entrega alimentación de la celda; para medir voltaje hace
// falta un punto de sense hacia un ADC del ESP32S3.
// En este firmware se usa GPIO1/D0 como entrada ADC cuando existe ese divisor.
// ─────────────────────────────────────────────────────────────────────────────
#define BATTERY_SENSE_ENABLED   1    // 0 = no intentar medir batería
#define PIN_BAT_ADC             1    // GPIO1 = D0 (ADC)
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
