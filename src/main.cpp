/**
 * @file    main.cpp
 * @brief   Prueba básica LoRa con Seeed XIAO ESP32S3 + WIO-SX1262 (RadioLib)
 *
 * Conexiones internas (conector B2B del shield WIO-SX1262):
 *   NSS   → GPIO41
 *   DIO1  → GPIO39
 *   RST   → GPIO42
 *   BUSY  → GPIO40
 *   SCK   → GPIO7
 *   MOSI  → GPIO9
 *   MISO  → GPIO8
 *   ANT_SW→ GPIO38  (control del switch de antena)
 *
 * Para probar punto a punto:
 *   - Flashear este firmware en un XIAO con LORA_MODE = TX
 *   - Flashear el mismo firmware en otro XIAO con LORA_MODE = RX
 *
 * Parámetros LoRa ajustables en la sección de configuración.
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

// ─────────────────────────────────────────────
// CONFIGURACIÓN  ← AJUSTAR AQUÍ
// ─────────────────────────────────────────────

// Modo: true = Transmisor  |  false = Receptor
#define LORA_MODE_TX true

// Frecuencia en MHz (elegir según región/antena)
// 915.0 para América  |  868.0 para Europa  |  433.0 genérico
#define LORA_FREQ 915.0

// Parámetros LoRa
#define LORA_BW 125.0   // Ancho de banda [kHz]: 125, 250, 500
#define LORA_SF 9       // Spreading Factor: 7–12 (mayor = más rango, más lento)
#define LORA_CR 5       // Coding Rate: 5–8  (5 = 4/5)
#define LORA_SYNC 0x12  // Sync word: 0x12=privado, 0x34=LoRaWAN público
#define LORA_POWER 10   // Potencia TX [dBm]: -9 a +22
#define LORA_PREAMBLE 8 // Longitud del preámbulo (símbolos)

// Intervalo entre transmisiones (solo en modo TX)
#define TX_INTERVAL_MS 3000

// ─────────────────────────────────────────────
// PINES  (WIO-SX1262 en XIAO ESP32S3)
// ─────────────────────────────────────────────
#define PIN_NSS 41
#define PIN_DIO1 39
#define PIN_RST 42
#define PIN_BUSY 40
#define PIN_SCK 7
#define PIN_MOSI 9
#define PIN_MISO 8
#define PIN_ANT_SW 38

// ─────────────────────────────────────────────
// Objeto RadioLib
// ─────────────────────────────────────────────
SPIClass spi(FSPI);
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);

SX1262 radio =
    new Module(PIN_NSS, PIN_DIO1, PIN_RST, PIN_BUSY, spi, spiSettings);

// ─────────────────────────────────────────────
// Variables globales
// ─────────────────────────────────────────────
static uint32_t txCount = 0;
static uint32_t rxCount = 0;
static uint32_t txErrors = 0;
static bool rxFlag = false; // seteada por la ISR de RadioLib

// Callback de recepción (llamado desde ISR)
void onRxDone(void) { rxFlag = true; }

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────
void printLoRaConfig() {
  Serial.println("═══════════════════════════════════════");
  Serial.println("  XIAO ESP32S3 + WIO-SX1262  |  RadioLib");
  Serial.println("═══════════════════════════════════════");
  Serial.printf("  Modo      : %s\n",
                LORA_MODE_TX ? "TRANSMISOR (TX)" : "RECEPTOR (RX)");
  Serial.printf("  Frecuencia: %.1f MHz\n", LORA_FREQ);
  Serial.printf("  BW        : %.1f kHz\n", LORA_BW);
  Serial.printf("  SF        : %d\n", LORA_SF);
  Serial.printf("  CR        : 4/%d\n", LORA_CR);
  Serial.printf("  Sync Word : 0x%02X\n", LORA_SYNC);
  Serial.printf("  Potencia  : %d dBm\n", LORA_POWER);
  Serial.println("═══════════════════════════════════════");
}

void printStatus(int16_t state, const char *action) {
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("[OK]  %s\n", action);
  } else {
    Serial.printf("[ERR] %s  → código: %d\n", action, state);
  }
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1500); // esperar al monitor serial

  // Switch de antena del WIO-SX1262
  pinMode(PIN_ANT_SW, OUTPUT);
  digitalWrite(PIN_ANT_SW, HIGH); // HIGH = habilitar antena TX/RX

  // Inicializar SPI en los pines del B2B
  spi.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NSS);

  printLoRaConfig();
  Serial.println("\nInicializando SX1262...");

  // Inicializar radio con los parámetros configurados
  int16_t state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC,
                              LORA_POWER, LORA_PREAMBLE);
  printStatus(state, "radio.begin()");

  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("¡Fallo de inicialización! Comprobá conexiones y reiniciá.");
    while (true) {
      delay(1000);
    }
  }

  // Corrección de corriente para módulos con TCXO (WIO-SX1262 la tiene)
  state = radio.setTCXO(1.8); // 1.8 V → tensión del TCXO del WIO-SX1262
  printStatus(state, "setTCXO(1.8V)");

  // Activar CRC en paquetes
  state = radio.setCRC(true);
  printStatus(state, "setCRC(true)");

  if (LORA_MODE_TX) {
    // ── Modo TRANSMISOR ──────────────────────────────
    Serial.println("\n[TX] Listo. Transmitiendo cada " +
                   String(TX_INTERVAL_MS) + " ms...\n");
  } else {
    // ── Modo RECEPTOR ────────────────────────────────
    // Configurar callback y comenzar escucha continua
    radio.setPacketReceivedAction(onRxDone);
    state = radio.startReceive();
    printStatus(state, "startReceive()");
    Serial.println("\n[RX] Escuchando...\n");
  }
}

// ─────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────
void loop() {

  // ════════════════════════════════
  //  MODO TX
  // ════════════════════════════════
  if (LORA_MODE_TX) {
    txCount++;
    String payload = "AgroXiao #" + String(txCount) +
                     " | uptime=" + String(millis() / 1000) + "s";

    Serial.printf("[TX] Enviando paquete #%lu: \"%s\" ... ", txCount,
                  payload.c_str());

    int16_t state = radio.transmit(payload);

    if (state == RADIOLIB_ERR_NONE) {
      float snr = radio.getSNR();
      float rssi = radio.getRSSI();
      Serial.printf("OK  |  RSSI=%.1f dBm  SNR=%.1f dB\n", rssi, snr);
    } else {
      txErrors++;
      Serial.printf("ERROR %d  (errores totales: %lu)\n", state, txErrors);
    }

    delay(TX_INTERVAL_MS);

    // ════════════════════════════════
    //  MODO RX
    // ════════════════════════════════
  } else {
    if (rxFlag) {
      rxFlag = false;

      String payload;
      int16_t state = radio.readData(payload);

      if (state == RADIOLIB_ERR_NONE) {
        rxCount++;
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();
        float freq =
            radio.getFrequencyError(); // error de frecuencia estimado

        Serial.printf("[RX] Paquete #%lu recibido:\n", rxCount);
        Serial.printf("     Datos  : \"%s\"\n", payload.c_str());
        Serial.printf("     RSSI   : %.1f dBm\n", rssi);
        Serial.printf("     SNR    : %.1f dB\n", snr);
        Serial.printf("     ΔFreq  : %.1f Hz\n", freq);
        Serial.printf("     Longitud: %d bytes\n", payload.length());
        Serial.println();
      } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[RX] ¡Error de CRC! Paquete corrupto.");
      } else {
        Serial.printf("[RX] Error al leer paquete: %d\n", state);
      }

      // Volver a escuchar
      radio.startReceive();
    }
  }
}