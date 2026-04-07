#pragma once

#include <Arduino.h>
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
// Resumen de una sesión de pruebas (range_test o acumulado)
// ─────────────────────────────────────────────────────────────────────────────
struct RangeSummary {
    uint32_t sent;
    uint32_t received;
    float    packet_loss_pct;   // 0.0–100.0

    float    rssi_min;
    float    rssi_max;
    float    rssi_avg;

    float    snr_min;
    float    snr_max;
    float    snr_avg;

    uint32_t rtt_min_ms;
    uint32_t rtt_max_ms;
    uint32_t rtt_avg_ms;
};

// ─────────────────────────────────────────────────────────────────────────────
// MetricsCollector — acumula muestras de RSSI, SNR y RTT
// ─────────────────────────────────────────────────────────────────────────────
class MetricsCollector {
public:
    void reset();

    // Llamar cuando se envía un PING
    void recordSent();

    // Llamar cuando se recibe el PONG correspondiente
    void recordReceived(float rssi, float snr, uint32_t rtt_ms);

    // Construye y retorna el resumen de la sesión actual
    RangeSummary getSummary() const;

    // Acceso rápido a contadores y última muestra
    uint32_t getPacketsSent()     const { return _sent;      }
    uint32_t getPacketsReceived() const { return _received;  }
    float    getLastRSSI()        const { return _lastRSSI;  }
    float    getLastSNR()         const { return _lastSNR;   }
    uint32_t getLastRTT()         const { return _lastRTT;   }

private:
    uint32_t _sent     = 0;
    uint32_t _received = 0;

    float    _rssiSum  = 0.0f;
    float    _rssiMin  =  0.0f;
    float    _rssiMax  = -200.0f;

    float    _snrSum   = 0.0f;
    float    _snrMin   =  50.0f;
    float    _snrMax   = -50.0f;

    uint32_t _rttSum   = 0;
    uint32_t _rttMin   = UINT32_MAX;
    uint32_t _rttMax   = 0;

    float    _lastRSSI = 0.0f;
    float    _lastSNR  = 0.0f;
    uint32_t _lastRTT  = 0;
};
