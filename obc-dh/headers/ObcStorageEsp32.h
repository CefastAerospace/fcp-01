/**
 * @file ObcStorageEsp32.h
 * @brief Montagem SD/HSPI e acesso exclusivo ao armazenamento do OBC.
 */

#pragma once

#include "ObcStorage.h" // Storage, RESULT, STATUS e limites do núcleo portátil.
#include "Pinout.h" // GPIOs sd_cs, sd_sck, sd_miso e sd_mosi existentes.
#include <SD.h> // SD.begin/end; não usa File::flush sem resultado.
#include <SPI.h> // SPIClass e HSPI, sem usar SPI global do rádio.
#include <freertos/FreeRTOS.h> // StaticSemaphore_t e contexto FreeRTOS.
#include <freertos/semphr.h> // Mutex estático e acesso sem espera.

namespace obc_storage {
namespace esp32 {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    /** @brief Frequência inicial do SD; ajustar conforme medição da montagem. */
    constexpr uint32_t sd_clock_hz = 4000000;

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Monta FAT em /sd e prepara /sd/obc sem formatação automática.
     * @details Primeira inicialização deve preceder consumidores e não pode
     * ocorrer concorrentemente. Depois disso, todas as operações usam mutex
     * estático, sem espera: disputa retorna BUSY. Repetir Init não recupera
     * escritor falho; usar End + Init e um nome novo. Uso exclusivo do SD
     * por este adaptador; não acessar SD/HSPI ou o núcleo por fora dele.
     * API apenas para tarefas, sem chamadas em ISR. I/O é síncrono; não usar
     * no atendimento FOC. Montagem elétrica e queda de energia exigem ensaio.
     */
    RESULT Init(
    );

    /** @brief Cria arquivo exclusivo; regras e resultados em Storage::OpenNew. */
    RESULT OpenNew(
        const char* name
    );

    /** @brief Grava com até três tentativas; buffer estável até o retorno. */
    RESULT Append(
        const uint8_t* data,
        size_t size
    );

    /** @brief Lê blocos de arquivo fechado; contrato de Storage::Read. */
    RESULT Read(
        const char* name,
        uint32_t offset,
        uint8_t* buffer,
        size_t capacity,
        uint32_t& total_size
    );

    /** @brief Sincroniza e fecha o escritor; preserva qualquer falha retornada. */
    RESULT Close(
    );

    /** @brief Lista arquivos com o mutex do serviço, sem manter iterador externo. */
    RESULT List(
        const char* after,
        FILE_INFO& out
    );

    /**
     * @brief Fecha, desmonta e libera HSPI; não apaga arquivos ou o mutex.
     * @details Retorna erro de fechamento, se houver. SD.end não fornece um
     * resultado verificável de desmontagem; OK não comprova persistência física.
     */
    RESULT End(
    );

} // namespace esp32
} // namespace obc_storage
