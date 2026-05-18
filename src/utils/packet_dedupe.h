#pragma once

#include <Arduino.h>

namespace PacketDedupe {

struct SeenEntry {
    bool     used = false;
    uint8_t  msg_type = 0;
    uint32_t source_id = 0;
    uint32_t packet_id = 0;
    uint32_t seen_ms = 0;
};

template <size_t N>
inline bool isDuplicateAndMark(SeenEntry (&cache)[N], const TestPacket& pkt, uint32_t ttlMs) {
    const uint32_t now = millis();

    for (size_t i = 0; i < N; i++) {
        if (cache[i].used && (now - cache[i].seen_ms) > ttlMs) {
            cache[i].used = false;
        }
    }

    int freeIdx = -1;
    int oldestIdx = 0;
    uint32_t oldestTs = UINT32_MAX;

    for (size_t i = 0; i < N; i++) {
        if (cache[i].used) {
            if (cache[i].msg_type == pkt.msg_type &&
                cache[i].source_id == pkt.source_id &&
                cache[i].packet_id == pkt.packet_id) {
                return true;
            }
            if (cache[i].seen_ms < oldestTs) {
                oldestTs = cache[i].seen_ms;
                oldestIdx = (int)i;
            }
        } else if (freeIdx < 0) {
            freeIdx = (int)i;
        }
    }

    int idx = (freeIdx >= 0) ? freeIdx : oldestIdx;
    cache[idx].used = true;
    cache[idx].msg_type = pkt.msg_type;
    cache[idx].source_id = pkt.source_id;
    cache[idx].packet_id = pkt.packet_id;
    cache[idx].seen_ms = now;

    return false;
}

}  // namespace PacketDedupe