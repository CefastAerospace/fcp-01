#include "TtcProtocol.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "TtcProtocol.h".
 */

namespace ttc {

    // ─── HELPERS ──────────────────────────────────────────────────────

    uint16_t Crc16(
        const uint8_t* data,
        size_t len
    ) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= (uint16_t)data[i] << 8;
            for (uint8_t bit = 0; bit < 8; bit++) {
                crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
            }
        }
        return crc;
    }

    bool CheckFrameCrc(
        const uint8_t* frame,
        size_t len
    ) {
        if (frame == nullptr || len <= sizeof(uint16_t)) return false;

        size_t body_len = len - sizeof(uint16_t);
        uint16_t stored = (uint16_t)frame[body_len] | ((uint16_t)frame[body_len + 1] << 8);
        return Crc16(frame, body_len) == stored;
    }

} // namespace ttc
