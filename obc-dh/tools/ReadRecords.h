/**
 * @file ReadRecords.h
 * @brief Recursos do leitor nativo: janela limitada e referências por boot.
 */

#pragma once

#include "ObcRecordCodec.h" // Mesmo decoder usado pelo firmware.
#include <algorithm> // min, search, stable_sort e upper_bound.
#include <exception> // std::exception para erros de recurso no programa nativo.
#include <fstream> // Leitura binária de arquivo seekable, sem alteração da origem.
#include <iomanip> // Precisão decimal suficiente para round-trip dos reais.
#include <iostream> // stdout JSON Lines e stderr com diagnósticos.
#include <locale> // Formatação JSON independente da configuração regional.
#include <vector> // Apenas referências temporais, não os registros de missão.

namespace obc_record_tool {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Referência validada e sua posição original para rastreabilidade. */
    struct REFERENCE {
        obc_time::TIME_REFERENCE value;
        uint64_t file_offset;
    };

    /** @brief Estatísticas de uma passagem; trechos descartados são [início,fim). */
    struct SCAN_SUMMARY {
        uint64_t records;
        uint64_t discarded_ranges;
        uint64_t discarded_bytes;
        bool io_error;
    };

} // namespace obc_record_tool
