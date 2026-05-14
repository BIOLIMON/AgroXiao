#pragma once

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include "core/config.h"
#include "core/node_config.h"

// ─────────────────────────────────────────────────────────────────────────────
// Estructura de paquete — compartida entre Gateway y Remote
// __attribute__((packed)) elimina padding para serialización directa
// ─────────────────────────────────────────────────────────────────────────────
struct TestPacket {
    uint8_t  msg_type;         // MSG_PING (0x01) o MSG_PONG (0x02)
    uint32_t source_id;        // Node ID origen
    uint32_t dest_id;          // Node ID destino (o NODE_BROADCAST_ID)
    uint8_t  hop_limit;        // TTL máximo permitido
    uint8_t  hop_count;        // Saltos recorridos hasta este nodo
    uint32_t packet_id;        // ID secuencial
    uint32_t timestamp;        // millis() al momento de envío (del PING original)
    float    gps_lat;          // Latitud GPS — 0.0 si no se usa
    float    gps_lon;          // Longitud GPS — 0.0 si no se usa
    // Datos de batería — completados por el Remote en cada PONG
    float    batteryVoltage;   // Voltaje real de la batería [V], ej: 3.85
    uint8_t  batteryPercent;   // Porcentaje de carga [0–100]
    // Datos NPK — completados por el Remote si hay sensor RS485 disponible
    uint16_t nitrogen;         // mg/kg; 0xFFFF = no disponible
    uint16_t phosphorus;       // mg/kg; 0xFFFF = no disponible
    uint16_t potassium;        // mg/kg; 0xFFFF = no disponible
    // Datos ambientales — AHT10 (temp/hum) + SHT30 (sonda temp)
    float    tempAmbient;      // AHT10 [°C]; NAN = no disponible
    float    humidity;         // AHT10 [%];  NAN = no disponible
    float    tempProbe;        // SHT30 [°C]; NAN = no disponible
} __attribute__((packed));

// ─────────────────────────────────────────────────────────────────────────────
// LoRaManager — wrapper sobre RadioLib SX1262
// ─────────────────────────────────────────────────────────────────────────────
class LoRaManager {
public:
    LoRaManager();

    // Inicialización con parámetros runtime desde NodeConfig.
    // Retorna true si OK.
    bool begin(const NodeConfig& cfg);

    // Reconfiguración en vivo de parámetros LoRa (para comando config).
    // Retorna true si OK.
    bool reconfigure(uint8_t sf, float bw, uint8_t cr, int8_t power);

    // Transmite un paquete PING con metadata de red.
    bool sendPing(uint32_t sourceId, uint32_t destId, uint32_t packet_id,
                  uint8_t hopLimit = 1);

    // Transmite un paquete PONG, reenviando el timestamp original del PING.
    // Incluye datos de batería, NPK y sensores ambientales medidos en el Remote.
    bool sendPong(uint32_t sourceId, uint32_t destId,
                  uint32_t packet_id, uint32_t original_timestamp,
                  float batteryVoltage = 0.0f, uint8_t batteryPercent = 0,
                  uint16_t nitrogen = NPK_VALUE_UNAVAILABLE,
                  uint16_t phosphorus = NPK_VALUE_UNAVAILABLE,
                  uint16_t potassium = NPK_VALUE_UNAVAILABLE,
                  float tempAmbient = NAN, float humidity = NAN, float tempProbe = NAN,
                  uint8_t hopLimit = 1);

    // Beacon mesh para descubrimiento de vecinos.
    bool sendHello(uint32_t sourceId, uint32_t packet_id, uint8_t hopLimit);

    // Estado del nodo (primera carga útil mesh: batería + sensores).
    bool sendNodeStatus(uint32_t sourceId, uint32_t destId, uint32_t packet_id,
                        float batteryVoltage, uint8_t batteryPercent,
                        uint16_t nitrogen = NPK_VALUE_UNAVAILABLE,
                        uint16_t phosphorus = NPK_VALUE_UNAVAILABLE,
                        uint16_t potassium = NPK_VALUE_UNAVAILABLE,
                        float tempAmbient = NAN, float humidity = NAN, float tempProbe = NAN,
                        uint8_t hopCount = 0, uint8_t hopLimit = 1);

    // Reenvía un paquete mesh incrementando hop_count (si hay TTL disponible).
    bool forwardPacket(const TestPacket& pkt);

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
    float   _freq  = 915.0f;
    uint8_t _sf    = 10;
    float   _bw    = 125.0f;
    uint8_t _cr    = 5;
    int8_t  _power = 22;

    // Serializa y transmite un buffer crudo. Bloquea hasta completar.
    bool _transmit(const uint8_t* buf, size_t len);
};
