#include "command_parser.h"
#include <string.h>
#include <stdio.h>

bool CommandParser::feed(char c) {
    // Ignorar \r (para terminadores Windows \r\n)
    if (c == '\r') return false;

    // Fin de línea → intentar parsear
    if (c == '\n') {
        _buf[_pos] = '\0';
        if (_pos > 0) {
            _ready = _parse();
        }
        _pos = 0;
        return _ready;
    }

    // Buffer lleno → descartar y resetear
    if (_pos >= BUF_SIZE - 1) {
        Serial.println("[WARN] Comando demasiado largo, descartado.");
        reset();
        return false;
    }

    _buf[_pos++] = c;
    return false;
}

Command CommandParser::getCommand() {
    Command cmd = _pending;
    reset();
    return cmd;
}

void CommandParser::reset() {
    _pos   = 0;
    _ready = false;
    _pending = Command{};
    _buf[0] = '\0';
}

bool CommandParser::_parse() {
    _pending = Command{};

    // Extraer primer token (nombre del comando)
    char name[32] = {};
    int matched = sscanf(_buf, "%31s", name);
    if (matched < 1) return false;

    if (strcmp(name, "ping") == 0) {
        _pending.type = CommandType::PING;
        return true;
    }

    if (strcmp(name, "status") == 0) {
        _pending.type = CommandType::STATUS;
        return true;
    }

    if (strcmp(name, "range_test") == 0) {
        uint16_t n = 0;
        if (sscanf(_buf, "range_test %hu", &n) == 1 && n > 0) {
            _pending.type    = CommandType::RANGE_TEST;
            _pending.range_n = n;
            return true;
        }
        Serial.println("[ERR] Uso: range_test <N>  (N > 0)");
        return false;
    }

    if (strcmp(name, "config") == 0) {
        int sf, cr, pwr;
        float bw;
        if (sscanf(_buf, "config %d %f %d %d", &sf, &bw, &cr, &pwr) == 4) {
            if (sf < 7 || sf > 12) {
                Serial.println("[ERR] SF debe estar entre 7 y 12");
                return false;
            }
            if (bw != 125.0f && bw != 250.0f && bw != 500.0f) {
                Serial.println("[ERR] BW debe ser 125.0, 250.0 o 500.0");
                return false;
            }
            if (cr < 5 || cr > 8) {
                Serial.println("[ERR] CR debe estar entre 5 y 8");
                return false;
            }
            if (pwr < -9 || pwr > 22) {
                Serial.println("[ERR] Power debe estar entre -9 y 22 dBm");
                return false;
            }
            _pending.type    = CommandType::CONFIG;
            _pending.cfg_sf  = (uint8_t)sf;
            _pending.cfg_bw  = bw;
            _pending.cfg_cr  = (uint8_t)cr;
            _pending.cfg_pwr = (int8_t)pwr;
            return true;
        }
        Serial.println("[ERR] Uso: config <sf> <bw> <cr> <power>");
        Serial.println("      Ejemplo: config 10 125.0 5 22");
        return false;
    }

    Serial.printf("[ERR] Comando desconocido: \"%s\"\n", name);
    Serial.println("      Comandos: ping | range_test <N> | config <sf> <bw> <cr> <pwr> | status");
    return false;
}
