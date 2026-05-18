#include "web_config.h"
#include "core/config_manager.h"
#include "core/config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <esp_mac.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// Template HTML — placeholders %KEY% se reemplazan en _buildPage()
// Guardado en flash (PROGMEM) para no consumir RAM estática
// ─────────────────────────────────────────────────────────────────────────────
static const char HTML_TEMPLATE[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AgroXiao</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#f0f4f0;color:#222;padding:16px}
h1{color:#1a5c1a;font-size:1.3em;margin-bottom:4px}
.sub{color:#666;font-size:.82em;margin-bottom:16px}
.card{background:#fff;border-radius:10px;padding:16px;margin-bottom:16px;box-shadow:0 1px 4px rgba(0,0,0,.08)}
.card h2{font-size:.95em;color:#1a5c1a;margin-bottom:12px;border-bottom:1px solid #e0e8e0;padding-bottom:6px}
.field{margin-bottom:12px}
label{display:block;font-size:.85em;font-weight:600;color:#444;margin-bottom:4px}
input,select{width:100%;padding:8px 10px;border:1px solid #c8d8c8;border-radius:6px;font-size:.95em;background:#fafffa}
input:focus,select:focus{outline:2px solid #2d8a2d;border-color:#2d8a2d}
.info-box{background:#e8f5e8;border-left:4px solid #2d8a2d;border-radius:4px;padding:10px 12px;font-size:.82em;line-height:1.6;margin-bottom:16px}
.info-box b{color:#1a5c1a}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.badge{display:inline-block;background:#2d8a2d;color:#fff;font-size:.75em;padding:2px 8px;border-radius:10px;margin-left:8px;vertical-align:middle}
.btn{width:100%;padding:13px;background:#1a5c1a;color:#fff;border:none;border-radius:8px;font-size:1em;font-weight:600;cursor:pointer;margin-top:4px;letter-spacing:.3px}
.btn:hover{background:#145014}
.warn{background:#fff8e1;border-left:4px solid #f9a825;padding:8px 12px;font-size:.82em;border-radius:4px;margin-top:12px}
</style>
</head>
<body>
<h1>AgroXiao <span class="badge">Config Mode</span></h1>
<p class="sub">Configuracion del nodo — los cambios se aplican al reiniciar</p>

<div class="info-box">
<b>Node ID:</b> %NODE_ID%<br>
<b>MAC:</b> %MAC%<br>
<b>Firmware:</b> AgroXiao v2.0
</div>

<form method="POST" action="/save">

<div class="card">
<h2>Identidad del nodo</h2>
<div class="field">
<label>Nombre del nodo</label>
<input name="name" value="%NAME%" maxlength="31" placeholder="AgroXiao">
</div>
<div class="field">
<label>Rol</label>
<select name="role">
<option value="1" %GW_SEL%>Gateway (nodo base, conectado a PC)</option>
<option value="2" %REM_SEL%>Remote (nodo de campo, sensor)</option>
<option value="3" %RTR_SEL%>Router (repetidor mesh)</option>
</select>
</div>
</div>

<div class="card">
<h2>Parametros LoRa</h2>
<div class="row">
<div class="field">
<label>Frecuencia (MHz)</label>
<input name="freq" type="number" step="0.1" min="400" max="960" value="%FREQ%">
</div>
<div class="field">
<label>Spreading Factor</label>
<input name="sf" type="number" min="7" max="12" value="%SF%">
</div>
</div>
<div class="row">
<div class="field">
<label>Ancho de banda</label>
<select name="bw">
<option value="125" %BW125%>125 kHz</option>
<option value="250" %BW250%>250 kHz</option>
<option value="500" %BW500%>500 kHz</option>
</select>
</div>
<div class="field">
<label>Coding Rate</label>
<select name="cr">
<option value="5" %CR5%>4/5 (menor overhead)</option>
<option value="6" %CR6%>4/6</option>
<option value="7" %CR7%>4/7</option>
<option value="8" %CR8%>4/8 (mayor robustez)</option>
</select>
</div>
</div>
<div class="row">
<div class="field">
<label>Potencia TX (dBm)</label>
<input name="pwr" type="number" min="-9" max="22" value="%PWR%">
</div>
<div class="field">
<label>Sync Word (hex)</label>
<input name="sync" value="%SYNC%" maxlength="4" placeholder="0x12">
</div>
</div>
</div>

<div class="card">
<h2>Timing (solo Gateway)</h2>
<div class="row">
<div class="field">
<label>Auto-ping (ms, 0=off)</label>
<input name="auto_ping" type="number" min="0" max="60000" value="%AUTO_PING%">
</div>
<div class="field">
<label>Timeout PONG (ms)</label>
<input name="ping_to" type="number" min="500" max="30000" value="%PING_TO%">
</div>
</div>
</div>

<button class="btn" type="submit">Guardar y Reiniciar</button>

<div class="warn">
Al guardar, el nodo se reiniciara automaticamente en el rol seleccionado.<br>
Para volver a Config Mode: mantene presionado BOOT al encender.
</div>

</form>
</body>
</html>)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Servidor HTTP (instancia global al archivo, no al heap)
// ─────────────────────────────────────────────────────────────────────────────
static WebServer server(80);

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static String macToStr() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static bool parseLongStrict(const String& src, long& out, int base = 10) {
    char* end = nullptr;
    const char* s = src.c_str();
    out = strtol(s, &end, base);
    if (s == end) return false;
    while (*end == ' ' || *end == '\t') end++;
    return *end == '\0';
}

static bool parseFloatStrict(const String& src, float& out) {
    char* end = nullptr;
    const char* s = src.c_str();
    out = strtof(s, &end);
    if (s == end) return false;
    while (*end == ' ' || *end == '\t') end++;
    return *end == '\0';
}

static void sendBadRequest(const String& msg) {
    server.send(400, "text/plain; charset=utf-8", "[ERR] " + msg);
}

// ─────────────────────────────────────────────────────────────────────────────
String WebConfig::_buildPage(const NodeConfig& cfg) {
    String page = String(FPSTR(HTML_TEMPLATE));

    // Identidad
    char nodeIdBuf[12];
    snprintf(nodeIdBuf, sizeof(nodeIdBuf), "0x%08lX", (unsigned long)cfg.node_id);
    page.replace("%NODE_ID%", nodeIdBuf);
    page.replace("%MAC%",     macToStr());
    page.replace("%NAME%",    cfg.node_name);

    // Rol
    page.replace("%GW_SEL%",  cfg.role == NodeRole::GATEWAY ? "selected" : "");
    page.replace("%REM_SEL%", cfg.role == NodeRole::REMOTE  ? "selected" : "");
    page.replace("%RTR_SEL%", cfg.role == NodeRole::ROUTER  ? "selected" : "");

    // LoRa params
    char freqBuf[10];
    snprintf(freqBuf, sizeof(freqBuf), "%.1f", cfg.lora_freq);
    page.replace("%FREQ%", freqBuf);
    page.replace("%SF%",   String(cfg.lora_sf));

    int bwInt = (int)cfg.lora_bw;
    page.replace("%BW125%", bwInt == 125 ? "selected" : "");
    page.replace("%BW250%", bwInt == 250 ? "selected" : "");
    page.replace("%BW500%", bwInt == 500 ? "selected" : "");

    page.replace("%CR5%",  cfg.lora_cr == 5 ? "selected" : "");
    page.replace("%CR6%",  cfg.lora_cr == 6 ? "selected" : "");
    page.replace("%CR7%",  cfg.lora_cr == 7 ? "selected" : "");
    page.replace("%CR8%",  cfg.lora_cr == 8 ? "selected" : "");

    page.replace("%PWR%",  String(cfg.lora_power));

    char syncBuf[7];
    snprintf(syncBuf, sizeof(syncBuf), "0x%02X", cfg.lora_sync);
    page.replace("%SYNC%", syncBuf);

    // Timing
    page.replace("%AUTO_PING%", String(cfg.auto_ping_ms));
    page.replace("%PING_TO%",   String(cfg.ping_timeout_ms));

    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
void WebConfig::_handleRoot(const NodeConfig& cfg) {
    server.send(200, "text/html; charset=utf-8", _buildPage(cfg));
}

// ─────────────────────────────────────────────────────────────────────────────
void WebConfig::_handleSave(NodeConfig& cfg) {
    NodeConfig newCfg = cfg;

    // Leer y validar cada campo del POST
    if (server.hasArg("name")) {
        String name = server.arg("name");
        name.trim();
        if (name.length() == 0) {
            sendBadRequest("name no puede estar vacio");
            return;
        }
        strncpy(newCfg.node_name, name.c_str(), sizeof(newCfg.node_name) - 1);
        newCfg.node_name[sizeof(newCfg.node_name) - 1] = '\0';
    }

    if (server.hasArg("role")) {
        long r = 0;
        if (!parseLongStrict(server.arg("role"), r) || (r != 1 && r != 2 && r != 3)) {
            sendBadRequest("role invalido (1=Gateway, 2=Remote, 3=Router)");
            return;
        }
        if (r == 1) newCfg.role = NodeRole::GATEWAY;
        else if (r == 2) newCfg.role = NodeRole::REMOTE;
        else newCfg.role = NodeRole::ROUTER;
    }

    if (server.hasArg("freq")) {
        float freq = 0.0f;
        if (!parseFloatStrict(server.arg("freq"), freq) || freq < 400.0f || freq > 960.0f) {
            sendBadRequest("freq fuera de rango [400..960] MHz");
            return;
        }
        newCfg.lora_freq = freq;
    }

    if (server.hasArg("sf")) {
        long sf = 0;
        if (!parseLongStrict(server.arg("sf"), sf) || sf < 7 || sf > 12) {
            sendBadRequest("sf fuera de rango [7..12]");
            return;
        }
        newCfg.lora_sf = (uint8_t)sf;
    }

    if (server.hasArg("bw")) {
        long bw = 0;
        if (!parseLongStrict(server.arg("bw"), bw) || (bw != 125 && bw != 250 && bw != 500)) {
            sendBadRequest("bw invalido (125, 250 o 500)");
            return;
        }
        newCfg.lora_bw = (float)bw;
    }

    if (server.hasArg("cr")) {
        long cr = 0;
        if (!parseLongStrict(server.arg("cr"), cr) || cr < 5 || cr > 8) {
            sendBadRequest("cr fuera de rango [5..8]");
            return;
        }
        newCfg.lora_cr = (uint8_t)cr;
    }

    if (server.hasArg("pwr")) {
        long pwr = 0;
        if (!parseLongStrict(server.arg("pwr"), pwr) || pwr < -9 || pwr > 22) {
            sendBadRequest("pwr fuera de rango [-9..22] dBm");
            return;
        }
        newCfg.lora_power = (int8_t)pwr;
    }

    if (server.hasArg("sync")) {
        String syncStr = server.arg("sync");
        syncStr.trim();
        // Acepta "0x12" o "12"
        if (syncStr.startsWith("0x") || syncStr.startsWith("0X")) {
            syncStr.remove(0, 2);
        }

        long syncVal = 0;
        if (!parseLongStrict(syncStr, syncVal, 16) || syncVal < 0x00 || syncVal > 0xFF) {
            sendBadRequest("sync invalido (00..FF)");
            return;
        }
        newCfg.lora_sync = (uint8_t)syncVal;
    }

    if (server.hasArg("auto_ping")) {
        long autoPing = 0;
        if (!parseLongStrict(server.arg("auto_ping"), autoPing) || autoPing < 0 || autoPing > 60000) {
            sendBadRequest("auto_ping fuera de rango [0..60000] ms");
            return;
        }
        newCfg.auto_ping_ms = (uint32_t)autoPing;
    }

    if (server.hasArg("ping_to")) {
        long pingTo = 0;
        if (!parseLongStrict(server.arg("ping_to"), pingTo) || pingTo < 500 || pingTo > 30000) {
            sendBadRequest("ping_to fuera de rango [500..30000] ms");
            return;
        }
        newCfg.ping_timeout_ms = (uint32_t)pingTo;
    }

    ConfigManager::sanitize(newCfg);

    // Guardar en NVS
    if (!ConfigManager::save(newCfg)) {
        server.send(500, "text/plain; charset=utf-8", "[ERR] No se pudo guardar configuracion en NVS");
        return;
    }

    cfg = newCfg;

    // Respuesta de confirmación antes de reiniciar
    server.send(200, "text/html; charset=utf-8",
        "<html><head><meta charset='utf-8'>"
        "<style>body{font-family:sans-serif;text-align:center;padding:40px;background:#f0f4f0}"
        "h2{color:#1a5c1a}p{color:#666}</style></head>"
        "<body><h2>Configuracion guardada!</h2>"
        "<p>El nodo se reiniciara en 2 segundos como <b>" + String(roleToStr(newCfg.role)) + "</b></p>"
        "<p>Podes desconectarte del WiFi de configuracion.</p>"
        "</body></html>");

    delay(2000);
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────────────────────
void WebConfig::_handleNotFound() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

// ─────────────────────────────────────────────────────────────────────────────
void WebConfig::_setupRoutes(NodeConfig& cfg) {
    server.on("/", HTTP_GET,  [&cfg]() { _handleRoot(cfg); });
    server.on("/save", HTTP_POST, [&cfg]() { _handleSave(cfg); });
    server.onNotFound(_handleNotFound);
}

// ─────────────────────────────────────────────────────────────────────────────
void WebConfig::start(NodeConfig& cfg) {
    // Si el nodo_id no fue generado todavía, generarlo ahora
    if (cfg.node_id == 0) {
        cfg.node_id = ConfigManager::generateNodeId();
    }

    // Construir SSID con los últimos 4 hex del node_id
    char ssid[24];
    snprintf(ssid, sizeof(ssid), "AgroXiao-%04lX", (unsigned long)(cfg.node_id & 0xFFFF));

    Serial.println("\n═══════════════════════════════════════════");
    Serial.println("  CONFIG MODE");
    Serial.printf ("  SSID : %s\n", ssid);
    Serial.println("  URL  : http://192.168.4.1");
    Serial.println("  Para salir: BOOT button al encender");
    Serial.println("═══════════════════════════════════════════\n");

    // Levantar AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid);  // Sin contraseña — red abierta local

    delay(500);  // Esperar que el AP esté listo

    Serial.printf("[WiFi] AP levantado: %s — IP: %s\n",
                  ssid, WiFi.softAPIP().toString().c_str());

    // Registrar rutas e iniciar servidor
    _setupRoutes(cfg);
    server.begin();
    Serial.println("[HTTP] Servidor iniciado en puerto 80\n");

    // Loop bloqueante — el handler de /save llama a ESP.restart()
    while (true) {
        server.handleClient();
        delay(2);
    }
}
