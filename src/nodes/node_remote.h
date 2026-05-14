#pragma once

#include "core/node_config.h"
#include "mesh/lora_manager.h"
#include "sensors/npk_sensor.h"
#include "sensors/env_sensor.h"
#include "utils/packet_dedupe.h"

// ─────────────────────────────────────────────────────────────────────────────
// NodeRemote — lógica del nodo de campo
//
// Uso desde main.cpp:
//   NodeRemote rem;
//   rem.init(cfg, lora);
//   // en loop():
//   rem.loop();
// ─────────────────────────────────────────────────────────────────────────────
class NodeRemote {
public:
    void init(const NodeConfig& cfg, LoRaManager& lora);
    void loop();

private:
    static constexpr uint8_t  SEEN_CACHE_SIZE = 24;
    static constexpr uint32_t SEEN_TTL_MS = 45000;

    const NodeConfig* _cfg  = nullptr;
    LoRaManager*      _lora = nullptr;
    NpkSensor         _npk;
    EnvSensor         _env;

    PacketDedupe::SeenEntry _seen[SEEN_CACHE_SIZE] = {};

    // Buffer para parseo mínimo de comandos Serial
    char    _lineBuf[32] = {};
    uint8_t _linePos     = 0;
    uint32_t _lastDiagMs = 0;

    bool _isDuplicateAndMark(const TestPacket& pkt);
    float   _readBatteryVoltage();
    uint8_t _getBatteryPercent(float voltage);
    void _readNpk(uint16_t& nitrogen, uint16_t& phosphorus, uint16_t& potassium);
    EnvReading _readEnv();
    void _printSensorDiagnostic();
};
