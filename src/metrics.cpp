#include "metrics.h"

void MetricsCollector::reset() {
    _sent     = 0;
    _received = 0;
    _rssiSum  = 0.0f;
    _rssiMin  =  0.0f;
    _rssiMax  = -200.0f;
    _snrSum   = 0.0f;
    _snrMin   =  50.0f;
    _snrMax   = -50.0f;
    _rttSum   = 0;
    _rttMin   = UINT32_MAX;
    _rttMax   = 0;
    _lastRSSI = 0.0f;
    _lastSNR  = 0.0f;
    _lastRTT  = 0;
}

void MetricsCollector::recordSent() {
    _sent++;
}

void MetricsCollector::recordReceived(float rssi, float snr, uint32_t rtt_ms) {
    _received++;

    _rssiSum += rssi;
    if (rssi < _rssiMin) _rssiMin = rssi;
    if (rssi > _rssiMax) _rssiMax = rssi;

    _snrSum += snr;
    if (snr < _snrMin) _snrMin = snr;
    if (snr > _snrMax) _snrMax = snr;

    _rttSum += rtt_ms;
    if (rtt_ms < _rttMin) _rttMin = rtt_ms;
    if (rtt_ms > _rttMax) _rttMax = rtt_ms;

    _lastRSSI = rssi;
    _lastSNR  = snr;
    _lastRTT  = rtt_ms;
}

RangeSummary MetricsCollector::getSummary() const {
    RangeSummary s;
    s.sent     = _sent;
    s.received = _received;

    if (_sent > 0) {
        s.packet_loss_pct = 100.0f * (float)(_sent - _received) / (float)_sent;
    } else {
        s.packet_loss_pct = 0.0f;
    }

    if (_received > 0) {
        s.rssi_min = _rssiMin;
        s.rssi_max = _rssiMax;
        s.rssi_avg = _rssiSum / (float)_received;

        s.snr_min  = _snrMin;
        s.snr_max  = _snrMax;
        s.snr_avg  = _snrSum / (float)_received;

        s.rtt_min_ms = _rttMin;
        s.rtt_max_ms = _rttMax;
        s.rtt_avg_ms = _rttSum / _received;
    } else {
        s.rssi_min = s.rssi_max = s.rssi_avg = 0.0f;
        s.snr_min  = s.snr_max  = s.snr_avg  = 0.0f;
        s.rtt_min_ms = s.rtt_max_ms = s.rtt_avg_ms = 0;
    }

    return s;
}
