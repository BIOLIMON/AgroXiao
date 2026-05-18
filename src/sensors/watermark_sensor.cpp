#include "watermark_sensor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constantes del circuito / límites de detección de falla
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float WM_SUPPLY_MV      = 3300.0f;  // Tensión de excitación [mV]
static constexpr int   WM_ADC_SAMPLES    = 5;         // Muestras por dirección
static constexpr uint32_t WM_SETTLE_MS   = 10;        // Espera post-excitación [ms]
static constexpr uint32_t WM_DISCHARGE_MS = 50;       // Descarga al finalizar [ms]

// Circuito abierto: suelo muy seco puede superar 80kΩ — umbral elevado para no
// descartar lecturas válidas de sensor en aire.
static constexpr float WM_OPEN_OHM      = 100000.0f; // Ω — circuito abierto / sensor ausente
static constexpr float WM_SHORT_OHM     = 200.0f;    // Ω — cortocircuito de cables

// ─────────────────────────────────────────────────────────────────────────────
void WatermarkSensor::begin() {
    pinMode(PIN_WM_A, INPUT);   // alta impedancia hasta la lectura
    pinMode(PIN_WM_B, INPUT);   // alta impedancia hasta la lectura
}

// ─────────────────────────────────────────────────────────────────────────────
// Circuito pseudo-AC (polaridad alternada para minimizar acumulación de carga):
//
//   PIN_WM_A ──[WM Sensor]── PIN_WM_ADC ──[R1 10kΩ]── PIN_WM_B
//
// Dir A: PIN_WM_A=HIGH, PIN_WM_B=LOW
//   V_adc_A = Vs × R1 / (R_sensor + R1)
//   R = R1 × (Vs − V_adc) / V_adc
//
// Dir B: PIN_WM_A=LOW, PIN_WM_B=HIGH  (polaridad inversa)
//   V_adc_B = Vs × R_sensor / (R_sensor + R1)
//   R = R1 × V_adc / (Vs − V_adc)
//
// Se promedian ambas estimaciones para cancelar offset del divisor.
// ─────────────────────────────────────────────────────────────────────────────
float WatermarkSensor::_readResistanceOhm() {
    // ── Dirección A: WM_A=HIGH, WM_B=LOW ──
    pinMode(PIN_WM_A, OUTPUT);
    pinMode(PIN_WM_B, OUTPUT);
    digitalWrite(PIN_WM_A, HIGH);
    digitalWrite(PIN_WM_B, LOW);
    delay(WM_SETTLE_MS);

    float vA_mv = 0.0f;
    for (int i = 0; i < WM_ADC_SAMPLES; i++) {
        vA_mv += (float)analogReadMilliVolts(PIN_WM_ADC);
    }
    vA_mv /= WM_ADC_SAMPLES;

    // Descargar
    digitalWrite(PIN_WM_A, LOW);
    delay(WM_DISCHARGE_MS);

    // ── Dirección B: WM_A=LOW, WM_B=HIGH ──
    digitalWrite(PIN_WM_A, LOW);
    digitalWrite(PIN_WM_B, HIGH);
    delay(WM_SETTLE_MS);

    float vB_mv = 0.0f;
    for (int i = 0; i < WM_ADC_SAMPLES; i++) {
        vB_mv += (float)analogReadMilliVolts(PIN_WM_ADC);
    }
    vB_mv /= WM_ADC_SAMPLES;

    // Descargar y volver a alta impedancia
    digitalWrite(PIN_WM_B, LOW);
    delay(WM_DISCHARGE_MS);
    pinMode(PIN_WM_A, INPUT);
    pinMode(PIN_WM_B, INPUT);

    // Detectar saturación (circuito abierto o cortocircuito)
    const float margin = 50.0f;
    bool vA_bad = (vA_mv <= margin || vA_mv >= (WM_SUPPLY_MV - margin));
    bool vB_bad = (vB_mv <= margin || vB_mv >= (WM_SUPPLY_MV - margin));

    float rA = -1.0f, rB = -1.0f;
    if (!vA_bad) {
        rA = (float)WM_SERIES_R * (WM_SUPPLY_MV - vA_mv) / vA_mv;
    }
    if (!vB_bad) {
        rB = (float)WM_SERIES_R * vB_mv / (WM_SUPPLY_MV - vB_mv);
    }

    if (rA > 0.0f && rB > 0.0f) {
        return (rA + rB) / 2.0f;
    } else if (rA > 0.0f) {
        return rA;
    } else if (rB > 0.0f) {
        return rB;
    }
    return -1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Calibración Shock 1998 (IRROMETER) con corrección de temperatura.
// Rango principal: 10 a 100 kPa. Extrapolación lineal fuera de ese rango.
// Retorna centibares positivos (kPa), o WM_VALUE_UNAVAILABLE si hay falla.
// ─────────────────────────────────────────────────────────────────────────────
int16_t WatermarkSensor::_ohmsToCentibars(float ohms, float tempC) {
    if (ohms <= 0.0f || ohms >= WM_OPEN_OHM) {
        return WM_VALUE_UNAVAILABLE;  // circuito abierto / sensor ausente
    }
    if (ohms < WM_SHORT_OHM) {
        return WM_VALUE_UNAVAILABLE;  // cortocircuito de cables
    }

    float resK  = ohms / 1000.0f;
    float tempD = 1.00f + 0.018f * (tempC - 24.0f);  // factor corrección temperatura
    float cb    = 0.0f;

    if (ohms > 550.0f) {
        if (ohms > 8000.0f) {
            // Rango superior al calibrado (suelo muy seco)
            cb = -2.246f
                 - 5.239f * resK * (1.0f + 0.018f * (tempC - 24.0f))
                 - 0.06756f * resK * resK * (tempD * tempD);
        } else if (ohms > 1000.0f) {
            // Rango principal de calibración Shock 1998 (10-100 kPa)
            cb = (-3.213f * resK - 4.093f)
                 / (1.0f - 0.009733f * resK - 0.01205f * tempC);
        } else {
            // Rango bajo (550-1000 Ω, suelo húmedo ~0-10 kPa)
            cb = -(resK * 23.156f - 12.736f) * tempD;
        }
    } else if (ohms > 300.0f) {
        cb = 0.0f;  // sensor saturado / suelo muy húmedo
    } else {
        return WM_VALUE_UNAVAILABLE;  // zona de cortocircuito de sensor (300-200 Ω)
    }

    // La ecuación devuelve valores negativos (convención de tensión negativa);
    // retornamos el valor absoluto como centibares positivos.
    int16_t result = (int16_t)(-cb + 0.5f);
    return (result < 0) ? 0 : result;
}

// ─────────────────────────────────────────────────────────────────────────────
int16_t WatermarkSensor::read(float tempC) {
    float ohms = _readResistanceOhm();
    if (ohms < 0.0f) {
        return WM_VALUE_UNAVAILABLE;
    }
    return _ohmsToCentibars(ohms, tempC);
}
