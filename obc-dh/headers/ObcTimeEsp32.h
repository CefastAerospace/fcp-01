/**
 * @file ObcTimeEsp32.h
 * @brief Timer monotônico e contador de boots na NVS do ESP32.
 */

#pragma once

#include "ObcTime.h" // STATUS, BOOT_STORAGE e reserva portátil do contador.
#include <esp_timer.h> // esp_timer_get_time.
#include <nvs.h> // nvs_open, nvs_get_u64, nvs_set_u64, nvs_commit e nvs_close.
#include <nvs_flash.h> // nvs_flash_init, sem apagar a partição em caso de falha.

namespace obc_time {
namespace esp32 {

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Lê o timer desde a inicialização do ESP32, em ms de 64 bits.
     * @return Tempo monotônico local; reinicia em cada boot, sem referência UTC.
     */
    uint64_t GetMonotonicMs(
    );

    /**
     * @brief Reserva e conserva em RAM o identificador deste boot.
     * @details NVS padrão, namespace obc_time, chave boot_count. Chamar antes
     * das tarefas consumidoras, sem concorrência. Após sucesso, novas chamadas
     * devolvem o mesmo identificador sem escrever novamente. Erros não apagam
     * a NVS nem publicam um ID. Sem integração automática com setup().
     * A perda/apagamento/restauração da NVS invalida a continuidade dos IDs;
     * ensaios físicos de queda durante escrita permanecem necessários.
     * @param boot_id Identificador confirmado; zero em caso de erro.
     * @return Resultado da reserva ou STORAGE_ERROR na inicialização da NVS.
     */
    STATUS StartBoot(
        uint64_t& boot_id
    );

} // namespace esp32
} // namespace obc_time
