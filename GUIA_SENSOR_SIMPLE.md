# Guía NPK para AgroXiao

Esta guía describe la conexión y los parámetros que usa el firmware actual para leer el sensor NPK desde el nodo Remote.

## Qué mide

- N: Nitrógeno
- P: Fósforo
- K: Potasio

Las lecturas se reportan en mg/kg.

## Dónde se usa en el firmware

- Solo en `REMOTE`.
- `GATEWAY` solo consume los valores recibidos por mesh.
- `ROUTER` solo reenvía el paquete sin modificar NPK.

## Cableado actual

### Sensor NPK

| Cable del sensor | Conexión |
|---|---|
| Marrón | +12V DC |
| Negro | GND común |
| Amarillo | A del bus RS485 |
| Azul | B del bus RS485 |

### UART/RS485 del XIAO ESP32S3

| Señal | GPIO |
|---|---|
| DE/RE | GPIO 6 |
| RX | GPIO 44 |
| TX | GPIO 43 |

El GND de la fuente de 12V debe compartir tierra con el ESP32.

## Parámetros Modbus

- Baud: `4800` — **el sensor opera a 4800, no 9600**
- Dirección: `0x01`
- Timeout: `1000 ms`
- Intentos por lectura: `3`

## Registros leídos

| Nutriente | Registro |
|---|---|
| N | `0x001E` |
| P | `0x001F` |
| K | `0x0020` |

El firmware hace una lectura por registro y valida CRC de respuesta antes de aceptar el valor.

## Qué deberías ver

Cuando el sensor responde bien, el Remote muestra algo como:

```text
[NPK] N=120 mg/kg | P=45 mg/kg | K=180 mg/kg
```

Y el Gateway lo refleja al recibir `PONG` o `NODE_STATUS`.

## Si no responde

- Verifica que el GND del sensor, la fuente de 12V y el ESP32 estén unidos.
- Si no aparece respuesta, invierte A/B del bus RS485.
- Comprueba que el sensor esté alimentado con 12V reales.
- Haz una prueba en aire y luego en suelo húmedo; en aire muchas sondas dan valores inestables.

## Acceso rápido en firmware

Desde el nodo Remote puedes pedir una lectura diagnóstica por Serial con:

```text
diag
```

o

```text
npk
```

Eso imprime la batería y la lectura NPK local.

## Errores comunes confirmados en hardware

### El sensor consume corriente pero el ESP32 no lee nada (timeout)

**Causa más probable: baud rate incorrecto.**
Este sensor opera a **4800 baud**. Si `NPK_SENSOR_BAUD` en `config.h` está en `9600`, el ESP32 envía tramas a velocidad incorrecta y el sensor las ignora silenciosamente — sigue consumiendo corriente pero nunca responde.

**Fix:** asegurarse de que en [src/core/config.h](src/core/config.h) esté:
```cpp
#define NPK_SENSOR_BAUD  4800
```

### El sensor responde a veces pero con errores de CRC o datos basura

**Causa probable: DE baja demasiado rápido después del flush.**
A 4800 baud cada byte tarda ~2 ms en salir físicamente del pin TX. Si DE/RE baja antes de que el último bit termine, el propio ESP32 interfiere con la línea. En [src/sensors/npk_sensor.cpp](src/sensors/npk_sensor.cpp) hay un `delay(4)` después de `flush()` para evitar esto — no reducirlo.

## Archivo relacionado

- `src/sensors/npk_sensor.cpp`
- `src/sensors/npk_sensor.h`
- `src/core/config.h` — aquí se define `NPK_SENSOR_BAUD`
- `src/nodes/node_remote.cpp`
