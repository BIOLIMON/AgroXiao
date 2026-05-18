#pragma once

#include <Arduino.h>

#include "core/config.h"

class NpkSensor {
public:
    bool begin();
    bool read(uint16_t& nitrogen, uint16_t& phosphorus, uint16_t& potassium);

private:
    HardwareSerial _uart{1};

    static uint16_t _crc16(const uint8_t* data, size_t len);
    bool _readRegister(uint16_t registerAddress, uint16_t& value);
    void _setTransmitMode(bool enabled);
    void _clearInputBuffer();
};