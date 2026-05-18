#pragma once

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// Roles de nodo — configurables en runtime via NVS + dashboard web
// ─────────────────────────────────────────────────────────────────────────────
enum class NodeRole : uint8_t {
    UNCONFIGURED = 0,   // Sin configurar — entra a Config Mode en el próximo boot
    GATEWAY      = 1,   // Nodo base: inicia pings, comandos Serial, uplink futuro
    REMOTE       = 2,   // Nodo de campo: auto-pong, reporta batería y sensores
    ROUTER       = 3,   // Repetidor mesh: reenvío multi-hop
};

// ─────────────────────────────────────────────────────────────────────────────
// Configuración completa del nodo — persiste en NVS
// ─────────────────────────────────────────────────────────────────────────────
struct NodeConfig {
    // Identidad
    NodeRole role           = NodeRole::UNCONFIGURED;
    char     node_name[32]  = "AgroXiao";
    uint32_t node_id        = 0;        // Derivado del MAC en primera configuración

    // Parámetros LoRa
    float    lora_freq      = 915.0f;   // MHz: 915=América, 868=Europa, 433=genérico
    uint8_t  lora_sf        = 10;       // Spreading Factor 7–12
    float    lora_bw        = 125.0f;   // Ancho de banda [kHz]: 125, 250, 500
    uint8_t  lora_cr        = 5;        // Coding Rate: 5=4/5 … 8=4/8
    int8_t   lora_power     = 22;       // Potencia TX [dBm]: -9 a +22
    uint8_t  lora_sync      = 0x12;     // Sync word: 0x12=privado, 0x34=LoRaWAN
    uint8_t  lora_preamble  = 8;        // Longitud del preámbulo [símbolos]

    // Timing del Gateway
    uint32_t auto_ping_ms   = 2000;     // Intervalo de auto-ping [ms] (0 = desactivado)
    uint32_t ping_timeout_ms= 3000;     // Timeout esperando PONG [ms]

    // Parámetros Mesh (fase 1)
    uint32_t mesh_beacon_ms = 15000;    // Intervalo del beacon HELLO [ms] (0 = off)
    uint8_t  mesh_max_hops  = 3;        // Límite de saltos para flooding controlado
};

// Retorna un string legible del rol
inline const char* roleToStr(NodeRole r) {
    switch (r) {
        case NodeRole::GATEWAY:      return "GATEWAY";
        case NodeRole::REMOTE:       return "REMOTE";
        case NodeRole::ROUTER:       return "ROUTER";
        case NodeRole::UNCONFIGURED: return "UNCONFIGURED";
        default:                     return "UNKNOWN";
    }
}
