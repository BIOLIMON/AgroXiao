#include "lora_manager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Definición del miembro estático
// ─────────────────────────────────────────────────────────────────────────────
volatile bool LoRaManager::_rxFlag = false;

// ─────────────────────────────────────────────────────────────────────────────
// Constructor — inicializa objetos SPI y SX1262 con los pines de config.h
// ─────────────────────────────────────────────────────────────────────────────
LoRaManager::LoRaManager()
    : _spi(FSPI),
      _radio(new Module(PIN_NSS, PIN_DIO1, PIN_RST, PIN_BUSY, _spi,
                        SPISettings(2000000, MSBFIRST, SPI_MODE0)))
{}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::begin() {
    // Switch de antena (solo si se usa conector B2B)
#if defined(PIN_ANT_SW)
    pinMode(PIN_ANT_SW, OUTPUT);
    digitalWrite(PIN_ANT_SW, HIGH);
#endif

    // Inicializar bus SPI
    _spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NSS);

    // Inicializar radio — TCXO pasado aquí (NO llamar setTCXO() después)
    int16_t state = _radio.begin(
        LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
        LORA_SYNC, LORA_POWER, LORA_PREAMBLE,
        LORA_TCXO_V
    );
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[ERR] radio.begin() falló: %d\n", state);
        return false;
    }
    Serial.println("[OK]  radio.begin()");

    // Habilitar CRC en paquetes
    state = _radio.setCRC(true);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[WARN] setCRC() falló: %d\n", state);
    }

    // Registrar callback ISR para recepción
    _radio.setPacketReceivedAction(onRxDone);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::reconfigure(uint8_t sf, float bw, uint8_t cr, int8_t power) {
    int16_t s1 = _radio.setSpreadingFactor(sf);
    int16_t s2 = _radio.setBandwidth(bw);
    int16_t s3 = _radio.setCodingRate(cr);
    int16_t s4 = _radio.setOutputPower(power);

    if (s1 != RADIOLIB_ERR_NONE || s2 != RADIOLIB_ERR_NONE ||
        s3 != RADIOLIB_ERR_NONE || s4 != RADIOLIB_ERR_NONE) {
        Serial.printf("[ERR] reconfigure() falló: SF=%d BW=%d CR=%d PWR=%d\n",
                      s1, s2, s3, s4);
        return false;
    }

    _sf    = sf;
    _bw    = bw;
    _cr    = cr;
    _power = power;

    return listen();
}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::sendPing(uint32_t packet_id) {
    TestPacket pkt;
    pkt.msg_type  = MSG_PING;
    pkt.packet_id = packet_id;
    pkt.timestamp = (uint32_t)millis();
    pkt.gps_lat   = 0.0f;
    pkt.gps_lon   = 0.0f;
    return _transmit(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::sendPong(uint32_t packet_id, uint32_t original_timestamp) {
    TestPacket pkt;
    pkt.msg_type  = MSG_PONG;
    pkt.packet_id = packet_id;
    pkt.timestamp = original_timestamp;  // Preservar timestamp del PING
    pkt.gps_lat   = 0.0f;
    pkt.gps_lon   = 0.0f;
    return _transmit(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::poll(TestPacket& pkt) {
    if (!_rxFlag) return false;

    // Limpiar flag ANTES de leer para no perder interrupciones concurrentes
    _rxFlag = false;

    uint8_t buf[sizeof(TestPacket)];
    int16_t state = _radio.readData(buf, sizeof(buf));

    // Re-armar recepción inmediatamente (fuera del contexto ISR, es seguro)
    listen();

    if (state != RADIOLIB_ERR_NONE) {
        if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println("[RX] Error de CRC — paquete descartado");
        } else {
            Serial.printf("[RX] Error al leer: %d\n", state);
        }
        return false;
    }

    // Validar tamaño
    if (_radio.getPacketLength() < sizeof(TestPacket)) {
        Serial.println("[RX] Paquete demasiado corto — descartado");
        return false;
    }

    memcpy(&pkt, buf, sizeof(TestPacket));
    _lastRSSI = _radio.getRSSI();
    _lastSNR  = _radio.getSNR();

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::listen() {
    int16_t state = _radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[ERR] startReceive() falló: %d\n", state);
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool LoRaManager::_transmit(const uint8_t* buf, size_t len) {
    int16_t state = _radio.transmit(buf, len);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[ERR] transmit() falló: %d\n", state);
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ISR — solo setea el flag, sin operaciones SPI ni llamadas a RadioLib
// ─────────────────────────────────────────────────────────────────────────────
void IRAM_ATTR LoRaManager::onRxDone() {
    _rxFlag = true;
}
