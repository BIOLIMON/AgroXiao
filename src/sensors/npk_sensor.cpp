#include "npk_sensor.h"

namespace {
constexpr uint8_t kNpkReadRetries = 3;
constexpr uint8_t kNpkRetryDelayMs = 20;
constexpr uint8_t kModbusReadHoldingRegisters = 0x03;
constexpr size_t   kNpkRequestSize = 8;
constexpr size_t   kNpkResponseSize = 7;
}

bool NpkSensor::begin() {
    pinMode(PIN_RS485_EN, OUTPUT);
    _setTransmitMode(false);
    _uart.begin(NPK_SENSOR_BAUD, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
    _uart.setTimeout(NPK_SENSOR_TIMEOUT_MS);
    return true;
}

bool NpkSensor::read(uint16_t& nitrogen, uint16_t& phosphorus, uint16_t& potassium) {
    nitrogen = NPK_VALUE_UNAVAILABLE;
    phosphorus = NPK_VALUE_UNAVAILABLE;
    potassium = NPK_VALUE_UNAVAILABLE;

    bool nOk = _readRegister(0x001E, nitrogen);
    delay(kNpkRetryDelayMs);
    bool pOk = _readRegister(0x001F, phosphorus);
    delay(kNpkRetryDelayMs);
    bool kOk = _readRegister(0x0020, potassium);

    return nOk && pOk && kOk;
}

void NpkSensor::_setTransmitMode(bool enabled) {
    digitalWrite(PIN_RS485_EN, enabled ? HIGH : LOW);
}

void NpkSensor::_clearInputBuffer() {
    while (_uart.available() > 0) {
        (void)_uart.read();
    }
}

uint16_t NpkSensor::_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool NpkSensor::_readRegister(uint16_t registerAddress, uint16_t& value) {
    uint8_t request[8] = {
        NPK_SENSOR_ADDR,
        kModbusReadHoldingRegisters,
        (uint8_t)(registerAddress >> 8),
        (uint8_t)(registerAddress & 0xFF),
        0x00,
        0x01,
        0x00,
        0x00
    };

    uint16_t crc = _crc16(request, 6);
    request[6] = (uint8_t)(crc & 0xFF);
    request[7] = (uint8_t)(crc >> 8);

    for (uint8_t attempt = 0; attempt < kNpkReadRetries; attempt++) {
        _clearInputBuffer();
        _setTransmitMode(true);
        delay(1);

        size_t sent = _uart.write(request, kNpkRequestSize);
        _uart.flush();
        delay(4);  // >2 bytes @ 4800 baud to clear the line before dropping DE
        _setTransmitMode(false);

        if (sent != kNpkRequestSize) {
            delay(kNpkRetryDelayMs);
            continue;
        }

        uint8_t response[kNpkResponseSize] = {};
        size_t received = _uart.readBytes(response, kNpkResponseSize);
        if (received != kNpkResponseSize) {
            delay(kNpkRetryDelayMs);
            continue;
        }

        if (response[0] != NPK_SENSOR_ADDR || response[1] != kModbusReadHoldingRegisters || response[2] != 0x02) {
            delay(kNpkRetryDelayMs);
            continue;
        }

        uint16_t responseCrc = (uint16_t)response[5] | ((uint16_t)response[6] << 8);
        if (_crc16(response, 5) != responseCrc) {
            delay(kNpkRetryDelayMs);
            continue;
        }

        value = ((uint16_t)response[3] << 8) | response[4];
        return true;
    }

    return false;
}