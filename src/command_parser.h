#pragma once

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// Tipos de comandos soportados
// ─────────────────────────────────────────────────────────────────────────────
enum class CommandType : uint8_t {
    NONE = 0,
    PING,         // ping
    RANGE_TEST,   // range_test <N>
    CONFIG,       // config <sf> <bw> <cr> <power>
    STATUS        // status
};

// ─────────────────────────────────────────────────────────────────────────────
// Resultado del parseo — solo los campos válidos para cada CommandType
// ─────────────────────────────────────────────────────────────────────────────
struct Command {
    CommandType type     = CommandType::NONE;
    uint16_t    range_n  = 0;       // range_test: número de paquetes
    uint8_t     cfg_sf   = 0;       // config: spreading factor
    float       cfg_bw   = 0.0f;   // config: bandwidth kHz
    uint8_t     cfg_cr   = 0;       // config: coding rate (5-8)
    int8_t      cfg_pwr  = 0;       // config: potencia dBm
};

// ─────────────────────────────────────────────────────────────────────────────
// CommandParser — parser non-blocking de comandos ASCII por Serial
// Uso: llamar feed(c) con cada byte leído de Serial.read().
//      Cuando feed() retorna true, llamar getCommand() para obtener el comando.
// ─────────────────────────────────────────────────────────────────────────────
class CommandParser {
public:
    // Alimenta un carácter. Retorna true cuando hay un comando completo listo.
    bool feed(char c);

    // Retorna el último comando parseado y resetea el estado interno.
    // Solo válido después de que feed() retornó true.
    Command getCommand();

    // Limpia el buffer y estado interno.
    void reset();

private:
    static constexpr uint8_t BUF_SIZE = 64;
    char    _buf[BUF_SIZE] = {};
    uint8_t _pos   = 0;
    bool    _ready = false;
    Command _pending;

    // Parsea _buf y llena _pending. Retorna true si el comando es válido.
    bool _parse();
};
