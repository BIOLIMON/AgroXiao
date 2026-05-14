#pragma once

#include "core/node_config.h"
#include "mesh/lora_manager.h"
#include "core/metrics.h"
#include "core/command_parser.h"

// ─────────────────────────────────────────────────────────────────────────────
// NodeGateway — lógica del nodo Gateway
//
// Uso desde main.cpp:
//   NodeGateway gw;
//   gw.init(cfg, lora, metrics);
//   // en loop():
//   gw.loop();
// ─────────────────────────────────────────────────────────────────────────────
class NodeGateway {
public:
    void init(const NodeConfig& cfg, LoRaManager& lora, MetricsCollector& metrics);
    void loop();

private:
    struct NeighborInfo {
        bool     used = false;
        uint32_t node_id = 0;
        float    last_rssi = 0.0f;
        float    last_snr = 0.0f;
        float    battery_v = 0.0f;
        uint8_t  battery_pct = 0;
        uint16_t nitrogen = NPK_VALUE_UNAVAILABLE;
        uint16_t phosphorus = NPK_VALUE_UNAVAILABLE;
        uint16_t potassium = NPK_VALUE_UNAVAILABLE;
        float    temp_ambient = NAN;
        float    humidity = NAN;
        float    temp_probe = NAN;
        uint8_t  hops = 0;
        uint32_t last_seen_ms = 0;
    };

    static constexpr uint8_t MAX_NEIGHBORS = 24;

    const NodeConfig*  _cfg     = nullptr;
    LoRaManager*       _lora    = nullptr;
    MetricsCollector*  _metrics = nullptr;

    CommandParser _parser;

    uint32_t _pingId        = 0;
    bool     _waitingPong   = false;
    uint32_t _pingTimestamp = 0;
    uint32_t _lastAutoPing  = 0;

    bool     _rangeActive   = false;
    uint16_t _rangeTotal    = 0;
    uint16_t _rangeSent     = 0;

    NeighborInfo _neighbors[MAX_NEIGHBORS] = {};
    uint32_t _lastHelloMs = 0;
    uint32_t _helloSeq = 0;

    void _sendNextPing();
    void _sendHello();
    void _handleNodeStatus(const TestPacket& pkt);
    void _handleCommand(const Command& cmd);
    void _printRangeSummary();
    void _printStatus();
};
