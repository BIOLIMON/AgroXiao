#include "env_sensor.h"
#include "core/config.h"

#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_SHT31.h>

// Instancias estáticas — solo existe un EnvSensor en el firmware
static Adafruit_AHTX0 s_aht;
static Adafruit_SHT31 s_sht;

bool EnvSensor::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    _ahtOk = s_aht.begin(&Wire);
    Serial.printf("[ENV] AHT10 (0x38): %s\n", _ahtOk ? "OK" : "no detectado");

    // SHT30 puede estar en 0x44 (ADDR=GND) o 0x45 (ADDR=VCC)
    _shtOk = s_sht.begin(0x44);
    if (!_shtOk) {
        _shtOk = s_sht.begin(0x45);
        if (_shtOk) Serial.println("[ENV] SHT30 (0x45): OK");
        else        Serial.println("[ENV] SHT30: no detectado");
    } else {
        Serial.println("[ENV] SHT30 (0x44): OK");
    }

    return _ahtOk || _shtOk;
}

EnvReading EnvSensor::read() {
    EnvReading r { NAN, NAN, NAN };

    if (_ahtOk) {
        sensors_event_t hum, temp;
        if (s_aht.getEvent(&hum, &temp)) {
            r.tempAmbient = temp.temperature;
            r.humidity    = hum.relative_humidity;
        }
    }

    if (_shtOk) {
        float t = s_sht.readTemperature();
        if (!isnan(t)) {
            r.tempProbe = t;
        }
    }

    return r;
}
