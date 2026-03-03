# AgroXiao — XIAO ESP32S3 + WIO-SX1262 LoRa

> Plataforma de desarrollo IoT agrícola basada en el módulo **Seeed Studio XIAO ESP32S3** con el shield **WIO-SX1262 LoRa**, gestionada con **PlatformIO**.

---

## 📋 Tabla de contenidos

- [Hardware](#hardware)
- [Pinout del shield WIO-SX1262](#pinout-del-shield-wio-sx1262)
- [Dependencias y configuración del proyecto](#dependencias-y-configuración-del-proyecto)
- [Firmware de prueba LoRa](#firmware-de-prueba-lora)
- [Parámetros configurables](#parámetros-configurables)
- [Cómo compilar y flashear](#cómo-compilar-y-flashear)
- [Monitor serial](#monitor-serial)
- [Prueba punto a punto (TX/RX)](#prueba-punto-a-punto-txrx)
- [Salida esperada](#salida-esperada)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Referencias](#referencias)

---

## Hardware

| Componente | Modelo | Descripción |
|---|---|---|
| Microcontrolador | [Seeed XIAO ESP32S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) | ESP32-S3 dual-core, WiFi + BLE, 8 MB Flash |
| Shield LoRa | [Wio-SX1262 para XIAO](https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/) | Módulo LoRa SX1262 con TCXO, conector B2B |

El shield **WIO-SX1262** se conecta al XIAO ESP32S3 directamente mediante un conector **Board-to-Board (B2B)** sin necesidad de cables adicionales.

---

## Pinout del shield WIO-SX1262

Las siguientes GPIOs son utilizadas internamente por el shield a través del conector B2B. **No modificar** salvo cambio de hardware.

| Señal | GPIO | Descripción |
|---|---|---|
| `NSS` / `CS` | **41** | SPI Chip Select del SX1262 |
| `SCK` | **7** | SPI Clock |
| `MOSI` | **9** | SPI Master Out Slave In |
| `MISO` | **8** | SPI Master In Slave Out |
| `RST` | **42** | Reset del SX1262 |
| `BUSY` | **40** | Señal BUSY del SX1262 |
| `DIO1` | **39** | Interrupción IRQ del SX1262 |
| `ANT_SW` | **38** | Control del switch de antena |

> **Nota sobre el TCXO:** El WIO-SX1262 utiliza un oscilador de temperatura compensada (TCXO) a **1.8 V**. El firmware ya lo configura automáticamente con `radio.setTCXO(1.8)`.

---

## Dependencias y configuración del proyecto

El proyecto usa **PlatformIO** con el framework Arduino para ESP32.

### `platformio.ini`

```ini
[env:seeed_xiao_esp32s3]
platform   = espressif32
board      = seeed_xiao_esp32s3
framework  = arduino
monitor_speed   = 115200
monitor_filters = esp32_exception_decoder

lib_deps =
    jgromes/RadioLib @ ^7.1.2
```

### Librería principal: RadioLib

[RadioLib](https://github.com/jgromes/RadioLib) es una librería universal para módulos de radio que soporta SX126x (SX1261/SX1262/SX1268), LoRaWAN, FSK, RTTY, entre otros. Se descarga automáticamente con PlatformIO.

---

## Firmware de prueba LoRa

El archivo `src/main.cpp` contiene un sketch de prueba que soporta **modo TX** (transmisor) y **modo RX** (receptor) configurado con una sola constante en la parte superior del archivo.

### Inicialización del radio (SPI personalizado)

El WIO-SX1262 usa el bus **FSPI** del ESP32S3. El objeto `SX1262` se instancia así:

```cpp
SPIClass spi(FSPI);
SX1262 radio = new Module(PIN_NSS, PIN_DIO1, PIN_RST, PIN_BUSY, spi, spiSettings);
```

### Secuencia de setup

1. Configurar GPIO38 (`ANT_SW`) en `HIGH` para habilitar la antena
2. Inicializar SPI en los pines B2B
3. Llamar a `radio.begin(freq, bw, sf, cr, sync, power, preamble)`
4. Configurar TCXO a 1.8 V
5. Activar CRC en paquetes
6. Según el modo: comenzar TX por polling o iniciar `startReceive()` con callback ISR

---

## Parámetros configurables

Todos los parámetros se encuentran en la sección `CONFIGURACIÓN` al inicio de `main.cpp`:

```cpp
// Modo: true = Transmisor  |  false = Receptor
#define LORA_MODE_TX  true

// Frecuencia (elegir según región y antena)
#define LORA_FREQ     915.0   // MHz — América del Sur
                              // 868.0 → Europa | 433.0 → genérico

// Parámetros LoRa
#define LORA_BW       125.0   // Ancho de banda [kHz]: 125 | 250 | 500
#define LORA_SF       9       // Spreading Factor: 7–12 (mayor SF = más rango, menor velocidad)
#define LORA_CR       5       // Coding Rate: 5–8  (5 = 4/5, más eficiente)
#define LORA_SYNC     0x12    // Sync word: 0x12 = red privada | 0x34 = LoRaWAN público
#define LORA_POWER    10      // Potencia TX [dBm]: rango -9 a +22
#define LORA_PREAMBLE 8       // Longitud de preámbulo (símbolos)

// Intervalo entre transmisiones en modo TX
#define TX_INTERVAL_MS  3000
```

### Referencia rápida de Spreading Factor

| SF | Velocidad aprox. | Alcance relativo | Uso recomendado |
|---|---|---|---|
| 7 | ~5.5 kbps | Corto | Pruebas rápidas, áreas locales |
| 9 | ~1.4 kbps | Medio | **Default, buen balance** |
| 11 | ~0.37 kbps | Largo | Zonas rurales, gran distancia |
| 12 | ~0.25 kbps | Máximo | Links extremos, muy lento |

---

## Cómo compilar y flashear

### Compilar

```bash
cd AgroXiao
pio run
```

### Compilar y flashear (dispositivo conectado por USB)

```bash
pio run --target upload
```

### Especificar puerto manualmente

```bash
pio run --target upload --upload-port /dev/ttyUSB0
```

---

## Monitor serial

```bash
pio device monitor
```

O con puerto explícito:

```bash
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

> El `monitor_filters = esp32_exception_decoder` en `platformio.ini` decodifica automáticamente los stack traces de panic/crash del ESP32.

---

## Prueba punto a punto (TX/RX)

Para testear la comunicación LoRa entre dos dispositivos:

1. **Dispositivo A (Transmisor):** compilar y flashear con `LORA_MODE_TX = true`
2. **Dispositivo B (Receptor):** cambiar a `LORA_MODE_TX = false`, compilar y flashear
3. Ambos deben usar la **misma frecuencia** y el **mismo sync word**
4. Abrir el monitor serial en cada dispositivo

---

## Salida esperada

### Transmisor (TX)

```
═══════════════════════════════════════
  XIAO ESP32S3 + WIO-SX1262  |  RadioLib
═══════════════════════════════════════
  Modo      : TRANSMISOR (TX)
  Frecuencia: 915.0 MHz
  BW        : 125.0 kHz
  SF        : 9
  CR        : 4/5
  Sync Word : 0x12
  Potencia  : 10 dBm
═══════════════════════════════════════

Inicializando SX1262...
[OK]  radio.begin()
[OK]  setTCXO(1.8V)
[OK]  setCRC(true)

[TX] Listo. Transmitiendo cada 3000 ms...

[TX] Enviando paquete #1: "AgroXiao #1 | uptime=3s" ... OK  |  RSSI=-52.0 dBm  SNR=8.5 dB
[TX] Enviando paquete #2: "AgroXiao #2 | uptime=6s" ... OK  |  RSSI=-53.1 dBm  SNR=8.2 dB
```

### Receptor (RX)

```
  Modo      : RECEPTOR (RX)
...
[RX] Escuchando...

[RX] Paquete #1 recibido:
     Datos  : "AgroXiao #1 | uptime=3s"
     RSSI   : -67.3 dBm
     SNR    : 7.8 dB
     ΔFreq  : 124.5 Hz
     Longitud: 23 bytes
```

---

## Estructura del proyecto

```
AgroXiao/
├── platformio.ini          # Configuración de PlatformIO (board, libs, monitor)
├── src/
│   └── main.cpp            # Firmware principal (prueba TX/RX con WIO-SX1262)
├── include/
│   └── README              # Directorio para headers propios del proyecto
├── lib/
│   └── README              # Directorio para librerías locales del proyecto
├── test/
│   └── README              # Directorio para tests unitarios (PlatformIO Test)
└── .pio/                   # Cache de PlatformIO (no versionar)
```

---

## Referencias

- [Seeed Wiki — XIAO ESP32S3 Getting Started](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Seeed Wiki — Wio-SX1262 Kit Introduction](https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/)
- [RadioLib — Documentación oficial](https://jgromes.github.io/RadioLib/)
- [RadioLib — Ejemplos SX126x](https://github.com/jgromes/RadioLib/tree/master/examples/SX126x)
- [PlatformIO — XIAO ESP32S3](https://docs.platformio.org/en/latest/boards/espressif32/seeed_xiao_esp32s3.html)
- [Semtech SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)

---

*Proyecto: AgroXiao — Agrophy · 2026*
