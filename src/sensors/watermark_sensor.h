#pragma once

#include <Arduino.h>
#include "core/config.h"

// Sensor de humedad de suelo Watermark 200SS — lectura por divisor resistivo pseudo-AC.
//
// Circuito requerido:
//
//   PIN_WM_A ──[WM Sensor]── PIN_WM_ADC ──[R1 10kΩ]── PIN_WM_B
//
//   PIN_WM_A   = GPIO1 (D0) — excitación polaridad A
//   PIN_WM_ADC = GPIO3 (D2) — ADC: punto de medición (ADC1_CH2)
//   PIN_WM_B   = GPIO5 (D4) — excitación polaridad B
//   R1 (10kΩ)  = resistencia externa entre PIN_WM_ADC y PIN_WM_B
//
// Se alternan dos polaridades para cancelar acumulación de carga en electrodos.
// Dir A: WM_A=HIGH, WM_B=LOW  → R = R1 × (Vs − V) / V
// Dir B: WM_A=LOW,  WM_B=HIGH → R = R1 × V / (Vs − V)
class WatermarkSensor {
public:
    void begin();

    // Lee la tensión de agua en el suelo.
    // tempC: temperatura del suelo en °C (usar sonda DS18B20 si disponible; 24°C por defecto)
    // Retorna centibares (kPa) en [0, 200], o WM_VALUE_UNAVAILABLE (-1) si hay falla.
    int16_t read(float tempC = 24.0f);

private:
    // Retorna resistencia promediada en Ohms, o -1.0f si el circuito detecta falla.
    float _readResistanceOhm();

    // Calibración Shock 1998 + corrección de temperatura.
    // Convierte resistencia (Ohm) a centibares positivos.
    int16_t _ohmsToCentibars(float ohms, float tempC);
};
