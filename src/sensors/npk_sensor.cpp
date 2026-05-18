#include "npk_sensor.h"

namespace {
constexpr uint8_t  kNpkReadRetries  = 3;
constexpr uint8_t  kNpkRetryDelayMs = 20;
constexpr uint8_t  kModbusReadHoldingRegisters = 0x03;
constexpr size_t   kNpkRequestSize  = 8;
constexpr size_t   kNpkResponseSize = 7;
}

bool NpkSensor::begin() {
    pinMode(PIN_NPK_PWR, OUTPUT);
    digitalWrite(PIN_NPK_PWR, NPK_PWR_OFF);
    pinMode(PIN_RS485_EN, OUTPUT);
    _setTransmitMode(false);
    // UART se inicializa UNA SOLA VEZ aquí — no reinicializar en read()
    _uart.begin(NPK_SENSOR_BAUD, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
    _uart.setTimeout(NPK_SENSOR_TIMEOUT_MS);

    Serial.printf("[NPK] UART1 TX=GPIO%d RX=GPIO%d EN=GPIO%d PWR=GPIO%d baud=%d\n",
                  PIN_RS485_TX, PIN_RS485_RX, PIN_RS485_EN, PIN_NPK_PWR, NPK_SENSOR_BAUD);
    return true;
}

bool NpkSensor::read(uint16_t& nitrogen, uint16_t& phosphorus, uint16_t& potassium) {
    nitrogen   = NPK_VALUE_UNAVAILABLE;
    phosphorus = NPK_VALUE_UNAVAILABLE;
    potassium  = NPK_VALUE_UNAVAILABLE;

    Serial.printf("[NPK] Encendiendo modulo (GPIO%d -> %s)...\n",
                  PIN_NPK_PWR, NPK_PWR_ON == LOW ? "LOW" : "HIGH");
    digitalWrite(PIN_NPK_PWR, NPK_PWR_ON);
    delay(NPK_PWR_WARMUP_MS);

    Serial.printf("[NPK] Bus listo. Leyendo addr=0x%02X...\n", NPK_SENSOR_ADDR);

    bool nOk = _readRegister(0x001E, nitrogen);
    delay(kNpkRetryDelayMs);
    bool pOk = _readRegister(0x001F, phosphorus);
    delay(kNpkRetryDelayMs);
    bool kOk = _readRegister(0x0020, potassium);

    Serial.printf("[NPK] Resultado: N=%s P=%s K=%s\n",
                  nOk ? "OK" : "FAIL", pOk ? "OK" : "FAIL", kOk ? "OK" : "FAIL");

    _setTransmitMode(false);
    digitalWrite(PIN_NPK_PWR, NPK_PWR_OFF);

    return nOk && pOk && kOk;
}

void NpkSensor::scanBus() {
    Serial.println("[NPK] === SCAN DE BUS RS485 ===");
    Serial.printf("[NPK] GPIO%d (PWR) estado: %s\n", PIN_NPK_PWR,
                  digitalRead(PIN_NPK_PWR) == LOW ? "LOW (ON)" : "HIGH (OFF)");

    digitalWrite(PIN_NPK_PWR, LOW);
    Serial.printf("[NPK] GPIO%d readback: %s\n", PIN_NPK_PWR,
                  digitalRead(PIN_NPK_PWR) == LOW ? "LOW ✓" : "HIGH (no obedece)");
    delay(2000);

    for (uint8_t addr = 0x01; addr <= 0x0F; addr++) {
        uint8_t req[8] = { addr, 0x03, 0x00, 0x1E, 0x00, 0x01, 0x00, 0x00 };
        uint16_t crc = _crc16(req, 6);
        req[6] = crc & 0xFF; req[7] = crc >> 8;

        _clearInputBuffer();
        _setTransmitMode(true);
        delay(1);
        _uart.write(req, 8);
        _uart.flush();
        delay(4);
        _setTransmitMode(false);

        uint8_t buf[16] = {};
        size_t n = _uart.readBytes(buf, 16);
        if (n > 0) {
            Serial.printf("[NPK]   addr=0x%02X -> %u bytes: ", addr, (unsigned)n);
            for (size_t i = 0; i < n; i++) Serial.printf("%02X ", buf[i]);
            Serial.println();
        } else {
            Serial.printf("[NPK]   addr=0x%02X -> sin respuesta\n", addr);
        }
        delay(50);
    }

    digitalWrite(PIN_NPK_PWR, NPK_PWR_OFF);
    Serial.println("[NPK] === FIN SCAN ===");
}

void NpkSensor::_setTransmitMode(bool enabled) {
    digitalWrite(PIN_RS485_EN, enabled ? HIGH : LOW);
}

void NpkSensor::_clearInputBuffer() {
    uint8_t n = 0;
    while (_uart.available() > 0) {
        _uart.read();
        n++;
    }
    if (n > 0) {
        Serial.printf("[NPK]   buffer flush: %d bytes descartados\n", n);
    }
}

uint16_t NpkSensor::_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x0001) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
        }
    }
    return crc;
}

bool NpkSensor::_readRegister(uint16_t reg, uint16_t& value) {
    uint8_t request[8] = {
        NPK_SENSOR_ADDR,
        kModbusReadHoldingRegisters,
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
        0x00, 0x01,
        0x00, 0x00
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
        delay(4);
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

        if (response[0] != NPK_SENSOR_ADDR ||
            response[1] != kModbusReadHoldingRegisters ||
            response[2] != 0x02) {
            delay(kNpkRetryDelayMs);
            continue;
        }

        uint16_t gotCrc  = (uint16_t)response[5] | ((uint16_t)response[6] << 8);
        uint16_t calcCrc = _crc16(response, 5);
        if (calcCrc != gotCrc) {
            delay(kNpkRetryDelayMs);
            continue;
        }

        value = ((uint16_t)response[3] << 8) | response[4];
        return true;
    }

    return false;
}
