# AgroXiao

Firmware para **Seeed Studio XIAO ESP32S3** + **Wio-SX1262 LoRa shield** orientado a red mesh agrícola. Un único binario soporta tres roles configurables en runtime: **Gateway**, **Remote** y **Router**.

## Hardware por nodo

### Todos los nodos
| Componente | Conexión |
|---|---|
| Wio-SX1262 LoRa shield | Conector B2B (fijo, sin cableado externo) |

### Nodo Remote (sensor de campo)
| Componente | Pin XIAO | GPIO | Notas |
|---|---|---|---|
| Sensor NPK RS485 — TX | D6 | GPIO43 | Via módulo MAX485 |
| Sensor NPK RS485 — RX | D7 | GPIO44 | Via módulo MAX485 |
| MAX485 DE/RE | D5 | GPIO6 | Control de dirección |
| MAX485 VCC (switch) | D3 | GPIO4 | Base BC557 PNP via 1 kΩ |
| DS18B20 (sonda Tº suelo) | D1 | GPIO2 | OneWire, resistencia 4.7 kΩ a 3.3V |
| Watermark 200SS — excitación A | D0 | GPIO1 | Salida digital |
| Watermark 200SS — ADC | D2 | GPIO3 | ADC1_CH2 |
| Watermark 200SS — excitación B | D4 | GPIO5 | Salida digital |
| Resistencia divisor WM | — | — | 10 kΩ entre GPIO3 y GPIO5 |

### Circuito Watermark 200SS (pseudo-AC)
```
GPIO1 (D0) ──[Sensor WM]── GPIO3 (D2/ADC) ──[R1 10kΩ]── GPIO5 (D4)
```
Se alternan dos polaridades para evitar acumulación de carga en los electrodos.

### Circuito control de alimentación NPK (BC557 PNP)
```
3.3V ────────────────── Emisor BC557
GPIO4 (D3) ──[1kΩ]──► Base BC557
                        Colector ──► VCC MAX485 + VCC sensor NPK
GND ────────────────── GND MAX485 + GND sensor NPK
```
GPIO4 LOW = módulo encendido · GPIO4 HIGH = módulo apagado (lógica activa-LOW).

---

## Roles del firmware

### Gateway
- Envía PINGs periódicos y recibe PONGs y `NODE_STATUS`
- Muestra RSSI / SNR / RTT / batería / NPK / temperatura / Watermark por Serial
- Emite beacons `HELLO` para descubrimiento mesh

### Remote (nodo de campo)
- **Modo deep sleep autónomo**: despierta → lee sensores → envía `NODE_STATUS` al broadcast → duerme
- Intervalo configurable con `DEEP_SLEEP_DURATION_S` en `node_remote.cpp`
- Durante el sleep los GPIO van a alta impedancia automáticamente (sin backfeed al MAX485)
- Comandos serial disponibles durante la ventana de 2 s tras el boot: `config_mode`, `diag`, `npk`

### Router
- Reenvía tráfico mesh e incluye estado mínimo en `NODE_STATUS`

---

## Estructura del código

```
src/
├── app/
│   └── main.cpp              # Entry point: carga config NVS, inicia LoRa y despacha al nodo
├── core/
│   ├── config.h              # Pines, constantes de protocolo y timing
│   ├── node_config.h         # Struct NodeConfig con parámetros LoRa y rol
│   ├── config_manager.*      # Persistencia en NVS (rol, parámetros LoRa, nombre)
│   ├── command_parser.*      # Parser de comandos serial para Gateway
│   └── metrics.*             # Estadísticas de red (RTT, pérdida de paquetes)
├── mesh/
│   └── lora_manager.*        # RadioLib SX1262: send/receive, TestPacket, poll()
├── nodes/
│   ├── node_gateway.*        # Lógica Gateway: ping, timeout, mesh display
│   ├── node_remote.*         # Lógica Remote: deep sleep, lectura sensores, TX autónomo
│   └── node_router.*         # Lógica Router: relay mesh
├── sensors/
│   ├── npk_sensor.*          # Modbus RTU sobre RS485 — N, P, K en mg/kg
│   ├── env_sensor.*          # DS18B20 OneWire — temperatura de sonda
│   └── watermark_sensor.*    # Watermark 200SS — centibares (kPa), calibración Shock 1998
├── utils/
│   └── packet_dedupe.h       # Deduplicación de paquetes por ID + TTL
└── web/
    └── web_config.*          # Portal WiFi AP para Config Mode (rol + parámetros LoRa)
```

---

## Protocolo de paquetes (`TestPacket`)

Struct packed serializado directamente sobre LoRa:

| Campo | Tipo | Descripción |
|---|---|---|
| `msg_type` | uint8 | PING / PONG / HELLO / NODE_STATUS |
| `source_id` / `dest_id` | uint32 | Node ID o `0xFFFFFFFF` (broadcast) |
| `hop_limit` / `hop_count` | uint8 | TTL mesh |
| `packet_id` | uint32 | Contador secuencial (persiste en RTC RAM en Remote) |
| `timestamp` | uint32 | millis() en el origen |
| `batteryVoltage` / `batteryPercent` | float / uint8 | Estado de batería |
| `nitrogen` / `phosphorus` / `potassium` | uint16 | mg/kg — `0xFFFF` = no disponible |
| `tempAmbient` / `humidity` / `tempProbe` | float | Temperatura DS18B20 — NAN = no disponible |
| `watermarkCb` | int16 | Centibares — `-1` = no disponible |

---

## Comandos

```bash
# Compilar
pio run

# Compilar y flashear (reemplazar puerto según corresponda)
pio run --target upload --upload-port /dev/ttyACM0   # Gateway
pio run --target upload --upload-port /dev/ttyACM1   # Remote

# Monitor serial
pio device monitor --port /dev/ttyACM0 --baud 115200
```

### Comandos serial — Gateway
| Comando | Acción |
|---|---|
| `ping` | Fuerza un ping inmediato |
| `status` | Muestra configuración LoRa actual y vecinos mesh |
| `range_test <N>` | Envía N pings seguidos y muestra estadísticas |
| `config <sf> <bw> <cr> <pwr>` | Cambia parámetros LoRa en runtime |
| `config_mode` | Reinicia en modo portal WiFi |

### Comandos serial — Remote (ventana de 2 s al boot)
| Comando | Acción |
|---|---|
| `config_mode` | Reinicia en modo portal WiFi |
| `diag` / `npk` | Imprime diagnóstico de todos los sensores |

---

## Config Mode (portal WiFi)

Mantén presionado **BOOT** al encender o envía `config_mode` por serial. El dispositivo levanta un AP WiFi. Desde el portal (`http://192.168.4.1`) se puede cambiar:
- Rol: Gateway / Remote / Router
- Nombre del nodo
- Frecuencia, SF, BW, CR, potencia, sync word LoRa

---

## Parámetros LoRa por defecto

| Parámetro | Valor |
|---|---|
| Frecuencia | 915.0 MHz |
| Spreading Factor | 10 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Potencia TX | 22 dBm |
| Sync Word | 0x12 (red privada) |
| TCXO | 1.8 V (requerido por Wio-SX1262) |

---

## Sensor NPK — detalles RS485

| Parámetro | Valor |
|---|---|
| Baud rate | 4800 |
| Dirección Modbus | 0x01 |
| Timeout | 1000 ms |
| Registro N | 0x001E |
| Registro P | 0x001F |
| Registro K | 0x0020 |

El módulo MAX485 se enciende solo durante la lectura y se apaga al terminar (`uart.end()` + `gpio_reset_pin()` previo al corte para evitar backfeed).

---

## Sensor Watermark 200SS — calibración

Implementa la calibración **Shock 1998** con corrección de temperatura:

| Resistencia | Tensión de agua | Condición |
|---|---|---|
| < 300 Ω | N/D | Cortocircuito |
| 300–550 Ω | 0 cb | Saturado |
| 550–1000 Ω | 0–10 cb | Suelo húmedo |
| 1–8 kΩ | 10–100 cb | Rango de campo |
| > 8 kΩ | > 100 cb | Suelo seco |
| > 100 kΩ | N/D | Circuito abierto / sensor ausente |

La temperatura de corrección proviene del DS18B20; si no está disponible se usa 24 °C por defecto.

---

## Referencias

- [Seeed XIAO ESP32S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Wio-SX1262 Kit](https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/)
- [RadioLib](https://jgromes.github.io/RadioLib/)
- [IRROMETER Watermark 200SS](https://www.irrometer.com/200ss.html)
- [PlatformIO](https://docs.platformio.org/en/latest/)
