#pragma once

#include "core/node_config.h"
#include "mesh/lora_manager.h"
#include "utils/packet_dedupe.h"

class NodeRouter {
public:
    void init(const NodeConfig& cfg, LoRaManager& lora);
    void loop();

private:
    static constexpr uint8_t  SEEN_CACHE_SIZE = 48;
    static constexpr uint32_t SEEN_TTL_MS = 60000;

    const NodeConfig* _cfg  = nullptr;
    LoRaManager*      _lora = nullptr;

    PacketDedupe::SeenEntry _seen[SEEN_CACHE_SIZE] = {};

    char    _lineBuf[32] = {};
    uint8_t _linePos     = 0;

    bool _isDuplicateAndMark(const TestPacket& pkt);
    void _handleSerialConfigMode();
};