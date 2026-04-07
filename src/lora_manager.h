#pragma once

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
// Estructura de paquete — compartida entre Gateway y Remote
// __attribute__((packed)) elimina padding para serialización directa
// ─────────────────────────────────────────────────────────────────────────────
struct TestPacket {
    uint8_t  msg_type;    // MSG_PING (0x01) o MSG_PONG (0x02)
    uint32_t packet_id;   // ID secuencial
    uint32_t timestamp;   // millis() al momento de envío (del PING original)
    float    gps_lat;     // Latitud GPS — 0.0 si no se usa
    float    gps_lon;     // Longitud GPS — 0.0 si no se usa
} __attribute__((packed));

// ─────────────────────────────────────────────────────────────────────────────
// LoRaManager — wrapper sobre RadioLib SX1262
// ─────────────────────────────────────────────────────────────────────────────
class LoRaManager {
public:
    LoRaManager();

    // Inicialización: configura SPI, radio, TCXO, CRC y acción ISR.
    // Retorna true si OK.
    bool begin();

    // Reconfiguración en vivo de parámetros LoRa (para comando config).
    // Retorna true si OK.
    bool reconfigure(uint8_t sf, float bw, uint8_t cr, int8_t power);

    // Transmite un paquete PING con el ID dado.
    bool sendPing(uint32_t packet_id);

    // Transmite un paquete PONG, reenviando el timestamp original del PING.
    bool sendPong(uint32_t packet_id, uint32_t original_timestamp);

    // Sondeo non-blocking: llamar desde loop().
    // Si hay un paquete nuevo completo, lo decodifica en `pkt` y retorna true.
    // También llama a listen() internamente para re-armar la recepción.
    bool poll(TestPacket& pkt);

    // Pone el radio en modo recepción continua.
    // Debe llamarse después de cualquier TX.
    bool listen();

    // Métricas del último paquete recibido
    float getLastRSSI() const { return _lastRSSI; }
    float getLastSNR()  const { return _lastSNR;  }

    // Getters de configuración actual (para comando status)
    float   getFreq()  const { return _freq;  }
    uint8_t getSF()    const { return _sf;    }
    float   getBW()    const { return _bw;    }
    uint8_t getCR()    const { return _cr;    }
    int8_t  getPower() const { return _power; }

    // Callback ISR — debe ser static (RadioLib lo llama sin instancia)
    static void IRAM_ATTR onRxDone();

private:
    SPIClass  _spi;
    SX1262    _radio;

    static volatile bool _rxFlag;

    float   _lastRSSI = 0.0f;
    float   _lastSNR  = 0.0f;

    // Cache de configuración actual
    float   _freq  = LORA_FREQ;
    uint8_t _sf    = LORA_SF;
    float   _bw    = LORA_BW;
    uint8_t _cr    = LORA_CR;
    int8_t  _power = LORA_POWER;

    // Serializa y transmite un buffer crudo. Bloquea hasta completar.
    bool _transmit(const uint8_t* buf, size_t len);
};
