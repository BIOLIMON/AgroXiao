#pragma once

#include <Arduino.h>

struct EnvReading {
    float tempAmbient;  // Reservado / NAN = no disponible
    float humidity;     // Reservado / NAN = no disponible
    float tempProbe;    // DS18B20 [°C], NAN = no disponible
};

class EnvSensor {
public:
    bool begin();
    EnvReading read();
    void scanI2C();      // Imprime por Serial todos los dispositivos en el bus I2C
    void scanOneWire();  // Imprime por Serial todos los dispositivos en el bus One-Wire

    bool ds18b20Detected() const { return _ds18b20Ok; }

private:
    bool _ds18b20Ok = false;
};
