#include <Arduino.h>
#include "core/config_manager.h"
#include "core/node_config.h"
#include "core/config.h"
#include "core/metrics.h"
#include "mesh/lora_manager.h"
#include "nodes/node_gateway.h"
#include "nodes/node_remote.h"
#include "web/web_config.h"

// Único firmware para todos los nodos (Gateway, Remote, Router)
// El rol se configura en ConfigManager y persiste en NVS

LoRaManager lora;
NodeConfig nodeConfig;
MetricsCollector metrics;

// Instancias de nodos (solo una activa según el rol configurado)
NodeGateway *gateway = nullptr;
NodeRemote  *remote  = nullptr;

void setup() {
    Serial.begin(115200);

    // LED e inicialización temprana antes del delay USB
    pinMode(PIN_LED, OUTPUT);
    ledOff();
    pinMode(PIN_BOOT, INPUT_PULLUP);
    pinMode(PIN_USER_BTN, INPUT_PULLUP);

    delay(2000);  // Esperar estabilización del USB

    Serial.println("\n╔═══════════════════════════════════════════════╗");
    Serial.println("║   AgroXiao - Sistema de Monitoreo Agrícola   ║");
    Serial.println("║        Mesh LoRa ESP32S3 + Sensores          ║");
    Serial.println("╚═══════════════════════════════════════════════╝\n");

    // Cargar configuración desde NVS
    nodeConfig = ConfigManager::load();

    // Entrar a Config Mode si: botón BOOT presionado al encender, o NVS lo solicitó
    bool bootBtn = (digitalRead(PIN_BOOT) == LOW);
    bool nvsReq  = ConfigManager::consumeConfigModeOnce();
    bool uncfg   = !ConfigManager::isConfigured(nodeConfig);

    if (bootBtn || nvsReq || uncfg) {
        if (bootBtn) Serial.println("[INIT] Botón BOOT presionado → Config Mode");
        if (nvsReq)  Serial.println("[INIT] Config Mode solicitado por NVS");
        if (uncfg)   Serial.println("[INIT] Nodo sin configurar → Config Mode");
        ledBlink(3, 100);
        WebConfig::start(nodeConfig);
        ESP.restart();
    }

    Serial.printf("[INIT] Rol: %s | ID: 0x%08lX\n",
                  nodeConfig.role == NodeRole::GATEWAY  ? "GATEWAY" :
                  nodeConfig.role == NodeRole::REMOTE   ? "REMOTE" :
                  nodeConfig.role == NodeRole::ROUTER   ? "ROUTER" : "UNKNOWN",
                  (unsigned long)nodeConfig.node_id);

    // Inicializar LoRa
    if (!lora.begin(nodeConfig)) {
        Serial.println("[INIT] ✗ Error inicializando LoRa");
        delay(5000);
        ESP.restart();
    }
    Serial.println("[INIT] ✓ LoRa inicializado");

    // Crear instancia del nodo según el rol
    switch (nodeConfig.role) {
        case NodeRole::GATEWAY:
            Serial.println("[INIT] Iniciando modo GATEWAY...");
            gateway = new NodeGateway();
            gateway->init(nodeConfig, lora, metrics);  // nodeConfig no-const: Gateway puede modificar auto_ping_ms
            break;

        case NodeRole::REMOTE:
            Serial.println("[INIT] Iniciando modo REMOTE...");
            remote = new NodeRemote();
            remote->init(nodeConfig, lora);
            break;

        case NodeRole::ROUTER:
            Serial.println("[INIT] Iniciando modo ROUTER...");
            remote = new NodeRemote();  // Router usa la lógica de remote
            remote->init(nodeConfig, lora);
            break;

        default:
            Serial.println("[INIT] ✗ Rol desconocido");
            delay(5000);
            ESP.restart();
    }

    Serial.println("[INIT] ✓ Sistema inicializado\n");
    ledBlink(3, 80);  // 3 parpadeos = boot OK
}

// ─────────────────────────────────────────────────────────────────────────────
// Detección de botón WIO presionado ≥6s → Config Mode en runtime
// ─────────────────────────────────────────────────────────────────────────────
static void _checkConfigButton() {
    static uint32_t pressStart = 0;
    static bool     pressing   = false;
    static uint8_t  ledPhase   = 0;

    if (digitalRead(PIN_USER_BTN) == LOW) {
        if (!pressing) {
            pressing   = true;
            pressStart = millis();
            ledPhase   = 0;
        }
        uint32_t held = millis() - pressStart;

        // Feedback visual progresivo: parpadea cada segundo
        uint8_t secs = held / 1000;
        if (secs != ledPhase) {
            ledPhase = secs;
            ledBlink(1, 60);
        }

        if (held >= 6000) {
            Serial.println("[BTN] Botón WIO 6s → Config Mode");
            ledBlink(5, 80);
            WebConfig::start(nodeConfig);
            ESP.restart();
        }
    } else {
        pressing = false;
    }
}

void loop() {
    _checkConfigButton();

    if (gateway) {
        gateway->loop();
    } else if (remote) {
        remote->loop();
    }
}
