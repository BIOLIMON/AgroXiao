#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// MODO DE OPERACIÓN  (compile-time)
// true  = Gateway  (conectado a PC, comandos Serial, inicia pings)
// false = Remote   (nodo de campo, auto-pong, LED de estado)
// Sobreescribible con build_flags: -DLORA_NODE_GATEWAY=1 / =0
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LORA_NODE_GATEWAY
#define LORA_NODE_GATEWAY false
#endif

// ─────────────────────────────────────────────────────────────────────────────
// PINES — Conector B2B interno (WIO-SX1262 shield sobre XIAO ESP32S3)
// ─────────────────────────────────────────────────────────────────────────────
#define PIN_NSS   41   // CS
#define PIN_SCK   7    // SCK
#define PIN_MOSI  9    // MOSI
#define PIN_MISO  8    // MISO
#define PIN_RST   42   // RST
#define PIN_DIO1  39   // DIO1 (IRQ)
#define PIN_BUSY  40   // BUSY
#define PIN_ANT_SW 38  // Switch de antena — HIGH para habilitar TX/RX

// LED integrado del XIAO ESP32S3
#define PIN_LED   21

// ─────────────────────────────────────────────────────────────────────────────
// PARÁMETROS LoRa
// ─────────────────────────────────────────────────────────────────────────────
#define LORA_FREQ       915.0f   // MHz: 915=América, 868=Europa, 433=genérico
#define LORA_SF         10       // Spreading Factor 7-12 (10=alcance alto)
#define LORA_BW         125.0f   // Ancho de banda [kHz]: 125, 250, 500
#define LORA_CR         5        // Coding Rate: 5=4/5, 6=4/6, 7=4/7, 8=4/8
#define LORA_POWER      22       // Potencia TX [dBm]: -9 a +22
#define LORA_PREAMBLE   8        // Longitud del preámbulo [símbolos]
#define LORA_SYNC       0x12     // Sync word: 0x12=privado, 0x34=LoRaWAN público
#define LORA_TCXO_V     1.8f     // Tensión TCXO [V] — requerido para WIO-SX1262

// ─────────────────────────────────────────────────────────────────────────────
// PROTOCOLO DE PAQUETES
// ─────────────────────────────────────────────────────────────────────────────
#define MSG_PING  0x01
#define MSG_PONG  0x02

// ─────────────────────────────────────────────────────────────────────────────
// TIMING
// ─────────────────────────────────────────────────────────────────────────────
#define PING_TIMEOUT_MS   3000   // ms máximo de espera por PONG
#define TX_SPACING_MS     200    // ms mínimo entre transmisiones consecutivas

// ─────────────────────────────────────────────────────────────────────────────
// SERIAL
// ─────────────────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200
