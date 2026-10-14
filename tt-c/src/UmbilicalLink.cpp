#include "UmbilicalLink.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "UmbilicalLink.h".
 */

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── CONSTRUTORES ─────────────────────────────────────────────────

// Público:
UmbilicalLink::UmbilicalLink(
    HardwareSerial& port,
    uint32_t baud
) : port(port), baud(baud) {
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
bool UmbilicalLink::IsReady(
) {
    return ready;
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool UmbilicalLink::Init(
) {
    port.begin(baud);
    ResetLine();
    ready = true;
    return true;
}

// Privado:
void UmbilicalLink::ResetLine(
) {
    line_len = 0;
    overflow = false;
}

// Público:
bool UmbilicalLink::Send(
    const uint8_t* data,
    size_t len
) {
    if (!ready || data == nullptr || len == 0 || len > ttc::max_frame_len) return false;

    uint8_t out[ttc::max_frame_len + 1];
    memcpy(out, data, len);
    out[len] = '\n';
    return port.write(out, len + 1) == len + 1;
}

// Público:
int UmbilicalLink::Receive(
    uint8_t* buffer,
    size_t max_len
) {
    if (!ready || buffer == nullptr || max_len == 0) return 0;

    // Lê até completar uma linha; o restante fica na porta para a próxima chamada.
    while (port.available() > 0) {
        char c = (char)port.read();
        if (c == '\r') continue;

        if (c != '\n') {
            if (line_len < sizeof(line) - 1) line[line_len++] = c;
            else overflow = true;
            continue;
        }

        // Linha completa.
        // - Excedeu o limite: descartada como erro.
        // - Não começa com '{': eco ou texto livre, ignorada.
        // - JSON: entregue.
        if (overflow) {
            ResetLine();
            return -1;
        }
        if (line_len == 0 || line[0] != '{') {
            ResetLine();
            continue;
        }
        if (line_len > max_len) {
            ResetLine();
            return -1;
        }

        size_t len = line_len;
        memcpy(buffer, line, len);
        ResetLine();
        return (int)len;
    }
    return 0;
}
