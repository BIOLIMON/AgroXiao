#pragma once

#include "core/node_config.h"

// ─────────────────────────────────────────────────────────────────────────────
// WebConfig — Modo de configuración via WiFi AP + servidor HTTP
//
// Flujo:
//   1. WebConfig::start(cfg)  → levanta AP "AgroXiao-XXXX", inicia servidor
//   2. Usuario conecta al AP y abre http://192.168.4.1
//   3. Completa el formulario y hace POST a /save
//   4. El nodo guarda en NVS y hace ESP.restart()
//
// Nota: start() es BLOQUEANTE — llama a handle() en loop interno.
//       Para salir sin guardar, el usuario debe resetear el hardware.
// ─────────────────────────────────────────────────────────────────────────────
class WebConfig {
public:
    // Inicia el modo de configuración. Bloquea hasta que el usuario guarda
    // (tras lo cual el nodo se reinicia).
    static void start(NodeConfig& cfg);

private:
    static void _setupRoutes(NodeConfig& cfg);
    static void _handleRoot(const NodeConfig& cfg);
    static void _handleSave(NodeConfig& cfg);
    static void _handleNotFound();

    // Reemplaza placeholders %KEY% en el template HTML con valores reales
    static String _buildPage(const NodeConfig& cfg);
};
