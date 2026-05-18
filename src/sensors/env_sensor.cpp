#include "env_sensor.h"
#include "core/config.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// Instancias estáticas — solo existe un EnvSensor en el firmware
static OneWire s_oneWire(PIN_ONE_WIRE);
static DallasTemperature s_ds18b20(&s_oneWire);

bool EnvSensor::begin() {
    // DS18B20 en One-Wire (GPIO2)
    s_ds18b20.begin();
    int numDS18B20 = s_ds18b20.getDeviceCount();
    if (numDS18B20 > 0) {
        _ds18b20Ok = true;
        Serial.printf("[ENV] DS18B20 (GPIO%d): OK (%d dispositivo%s)\n", PIN_ONE_WIRE, numDS18B20, numDS18B20 == 1 ? "" : "s");
    } else {
        Serial.printf("[ENV] DS18B20 (GPIO%d): no detectado — verificar cableado\n", PIN_ONE_WIRE);
    }

    return _ds18b20Ok;
}

void EnvSensor::scanI2C() {
    Serial.println("[I2C] Bus I2C no disponible (GPIO4/GPIO5 reasignados)");
}

void EnvSensor::scanOneWire() {
    Serial.printf("[One-Wire] Scan (GPIO%d):\n", PIN_ONE_WIRE);
    int deviceCount = s_ds18b20.getDeviceCount();
    if (deviceCount == 0) {
        Serial.println("[One-Wire]   ningún dispositivo encontrado");
        return;
    }
    
    DeviceAddress address;
    for (int i = 0; i < deviceCount; i++) {
        if (s_ds18b20.getAddress(address, i)) {
            Serial.printf("[One-Wire]   Dispositivo %d: ", i);
            for (int j = 0; j < 8; j++) {
                Serial.printf("%02X", address[j]);
                if (j < 7) Serial.print(":");
            }
            Serial.println();
        }
    }
    Serial.printf("[One-Wire] DS18B20: %s (%d dispositivo%s)\n", _ds18b20Ok ? "OK" : "no init", deviceCount, deviceCount == 1 ? "" : "s");
}

EnvReading EnvSensor::read() {
    EnvReading r { NAN, NAN, NAN };

    if (_ds18b20Ok) {
        // Solicitar lectura de temperatura de todos los sensores
        s_ds18b20.requestTemperatures();
        // Leer del primer dispositivo DS18B20 encontrado (índice 0)
        float t = s_ds18b20.getTempCByIndex(0);
        // DallasTemperature retorna -127 en caso de error, checar con isnan no aplica
        if (t != -127.0f && t != 85.0f) {  // 85.0 es el default powerup value
            r.tempProbe = t;
        }
    }

    return r;
}
