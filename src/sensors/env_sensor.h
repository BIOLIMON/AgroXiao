#pragma once

#include <Arduino.h>

struct EnvReading {
    float tempAmbient;  // AHT10 [°C], NAN = no disponible
    float humidity;     // AHT10 [%],  NAN = no disponible
    float tempProbe;    // SHT30 [°C], NAN = no disponible
};

class EnvSensor {
public:
    bool begin();
    EnvReading read();

    bool ahtDetected() const { return _ahtOk; }
    bool shtDetected() const { return _shtOk; }

private:
    bool _ahtOk = false;
    bool _shtOk = false;
};
