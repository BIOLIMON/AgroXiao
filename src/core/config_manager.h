#pragma once

#include "node_config.h"

// ─────────────────────────────────────────────────────────────────────────────
// ConfigManager — persistencia de NodeConfig en NVS via Preferences
//
// Namespace NVS: "agroxiao"
// Keys: role, name, node_id, freq, sf, bw, cr, pwr, sync, preamble,
//        auto_ping, ping_to, cfg_once
// ─────────────────────────────────────────────────────────────────────────────
class ConfigManager {
public:
    // Carga config desde NVS. Si no hay datos retorna config con defaults.
    static NodeConfig load();

    // Guarda config en NVS. Retorna true si OK.
    static bool save(const NodeConfig& cfg);

    // Borra todo el namespace NVS — equivale a factory reset.
    static void reset();

    // Retorna true si el nodo tiene un rol asignado (no UNCONFIGURED).
    static bool isConfigured(const NodeConfig& cfg);

    // Sanea parámetros fuera de rango para evitar estados inválidos.
    static void sanitize(NodeConfig& cfg);

    // Solicita entrar a Config Mode solo en el próximo boot.
    static bool requestConfigModeOnce();

    // Consume la solicitud one-shot de Config Mode y la limpia.
    static bool consumeConfigModeOnce();

    // Si detecta un firmware nuevo (post-flash), fuerza estado UNCONFIGURED
    // una sola vez para re-provisionar el nodo. Retorna true si aplicó reset.
    static bool markUnconfiguredOnNewFirmware(NodeConfig& cfg);

    // Genera un node_id de 32 bits a partir del MAC address del ESP32.
    // Los 4 bytes menos significativos del MAC son suficientemente únicos
    // para una red local de cientos de nodos.
    static uint32_t generateNodeId();
};
