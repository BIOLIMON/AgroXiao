#include "config_manager.h"
#include <Preferences.h>
#include <esp_mac.h>

static inline bool _isValidRole(NodeRole role) {
    return role == NodeRole::UNCONFIGURED ||
           role == NodeRole::GATEWAY ||
           role == NodeRole::REMOTE ||
           role == NodeRole::ROUTER;
}

// Namespace NVS — máximo 15 caracteres
static const char* NVS_NS = "agroxiao";

// ─────────────────────────────────────────────────────────────────────────────
NodeConfig ConfigManager::load() {
    NodeConfig cfg;
    Preferences prefs;

    if (!prefs.begin(NVS_NS, /*readOnly=*/true)) {
        // NVS vacío o namespace no existe — retorna defaults (UNCONFIGURED)
        return cfg;
    }

    cfg.role           = static_cast<NodeRole>(prefs.getUChar("role", 0));
    cfg.node_id        = prefs.getUInt("node_id", 0);
    cfg.lora_freq      = prefs.getFloat("freq",    cfg.lora_freq);
    cfg.lora_sf        = prefs.getUChar("sf",      cfg.lora_sf);
    cfg.lora_bw        = prefs.getFloat("bw",      cfg.lora_bw);
    cfg.lora_cr        = prefs.getUChar("cr",      cfg.lora_cr);
    cfg.lora_power     = (int8_t)prefs.getChar("pwr",      cfg.lora_power);
    cfg.lora_sync      = prefs.getUChar("sync",    cfg.lora_sync);
    cfg.lora_preamble  = prefs.getUChar("preamble",cfg.lora_preamble);
    cfg.auto_ping_ms   = prefs.getUInt("auto_ping",cfg.auto_ping_ms);
    cfg.ping_timeout_ms= prefs.getUInt("ping_to",  cfg.ping_timeout_ms);
    cfg.mesh_beacon_ms = prefs.getUInt("mesh_bcn", cfg.mesh_beacon_ms);
    cfg.mesh_max_hops  = prefs.getUChar("mesh_hops", cfg.mesh_max_hops);

    // Leer nombre (String → char[])
    String name = prefs.getString("name", cfg.node_name);
    strncpy(cfg.node_name, name.c_str(), sizeof(cfg.node_name) - 1);
    cfg.node_name[sizeof(cfg.node_name) - 1] = '\0';

    prefs.end();
    sanitize(cfg);
    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ConfigManager::save(const NodeConfig& cfg) {
    NodeConfig safeCfg = cfg;
    sanitize(safeCfg);

    Preferences prefs;

    if (!prefs.begin(NVS_NS, /*readOnly=*/false)) {
        Serial.println("[NVS] Error: no se pudo abrir para escritura");
        return false;
    }

    prefs.putUChar("role",      static_cast<uint8_t>(safeCfg.role));
    prefs.putUInt ("node_id",   safeCfg.node_id);
    prefs.putString("name",     safeCfg.node_name);
    prefs.putFloat ("freq",     safeCfg.lora_freq);
    prefs.putUChar ("sf",       safeCfg.lora_sf);
    prefs.putFloat ("bw",       safeCfg.lora_bw);
    prefs.putUChar ("cr",       safeCfg.lora_cr);
    prefs.putChar  ("pwr",      safeCfg.lora_power);
    prefs.putUChar ("sync",     safeCfg.lora_sync);
    prefs.putUChar ("preamble", safeCfg.lora_preamble);
    prefs.putUInt  ("auto_ping",safeCfg.auto_ping_ms);
    prefs.putUInt  ("ping_to",  safeCfg.ping_timeout_ms);
    prefs.putUInt  ("mesh_bcn", safeCfg.mesh_beacon_ms);
    prefs.putUChar ("mesh_hops",safeCfg.mesh_max_hops);

    prefs.end();
    Serial.println("[NVS] Configuración guardada.");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void ConfigManager::reset() {
    Preferences prefs;
    if (prefs.begin(NVS_NS, false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[NVS] Factory reset — NVS borrado.");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
bool ConfigManager::isConfigured(const NodeConfig& cfg) {
    return cfg.role != NodeRole::UNCONFIGURED && cfg.node_id != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
void ConfigManager::sanitize(NodeConfig& cfg) {
    if (!_isValidRole(cfg.role)) {
        cfg.role = NodeRole::UNCONFIGURED;
    }

    if (cfg.node_name[0] == '\0') {
        strncpy(cfg.node_name, "AgroXiao", sizeof(cfg.node_name) - 1);
        cfg.node_name[sizeof(cfg.node_name) - 1] = '\0';
    }

    cfg.lora_freq = constrain(cfg.lora_freq, 400.0f, 960.0f);
    cfg.lora_sf = constrain(cfg.lora_sf, (uint8_t)7, (uint8_t)12);
    cfg.lora_cr = constrain(cfg.lora_cr, (uint8_t)5, (uint8_t)8);
    cfg.lora_power = constrain(cfg.lora_power, (int8_t)-9, (int8_t)22);

    if (cfg.lora_bw != 125.0f && cfg.lora_bw != 250.0f && cfg.lora_bw != 500.0f) {
        cfg.lora_bw = 125.0f;
    }

    if (cfg.lora_preamble == 0) {
        cfg.lora_preamble = 8;
    }

    cfg.auto_ping_ms = constrain(cfg.auto_ping_ms, (uint32_t)0, (uint32_t)60000);
    cfg.ping_timeout_ms = constrain(cfg.ping_timeout_ms, (uint32_t)500, (uint32_t)30000);
    cfg.mesh_beacon_ms = constrain(cfg.mesh_beacon_ms, (uint32_t)0, (uint32_t)120000);
    cfg.mesh_max_hops = constrain(cfg.mesh_max_hops, (uint8_t)1, (uint8_t)10);
}

// ─────────────────────────────────────────────────────────────────────────────
bool ConfigManager::requestConfigModeOnce() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, /*readOnly=*/false)) {
        Serial.println("[NVS] Error: no se pudo guardar cfg_once");
        return false;
    }

    prefs.putBool("cfg_once", true);
    prefs.end();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ConfigManager::consumeConfigModeOnce() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, /*readOnly=*/false)) {
        return false;
    }

    bool requested = prefs.getBool("cfg_once", false);
    if (requested) {
        prefs.putBool("cfg_once", false);
    }
    prefs.end();
    return requested;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ConfigManager::markUnconfiguredOnNewFirmware(NodeConfig& cfg) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, /*readOnly=*/false)) {
        return false;
    }

    // Firma del build actual: cambia en cada compilación/subida.
    String currentBuildId = String(__DATE__) + " " + String(__TIME__);
    String storedBuildId = prefs.getString("fw_id", "");
    bool isNewFirmware = (storedBuildId != currentBuildId);

    if (isNewFirmware) {
        prefs.putString("fw_id", currentBuildId);
    }

    prefs.end();

    if (!isNewFirmware) {
        return false;
    }

    cfg.role = NodeRole::UNCONFIGURED;
    if (cfg.node_id == 0) {
        cfg.node_id = generateNodeId();
    }
    sanitize(cfg);
    save(cfg);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
uint32_t ConfigManager::generateNodeId() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // Usa los 4 bytes menos significativos del MAC
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16)
         | ((uint32_t)mac[4] << 8)  | mac[5];
}
