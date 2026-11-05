/**
 * @file ObcStartup.h
 * @brief Coordenação portátil dos serviços do boot de bancada.
 */

#pragma once

#include "ObcTime.h" // STATUS temporal e identificação de boot existentes.
#include "ObcStorage.h" // RESULT/STATUS completos do armazenamento, sem driver.

namespace obc_startup {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /**
     * @brief Funções existentes de inicialização, substituíveis no teste nativo.
     * @details Todas são obrigatórias e síncronas. Não há provedores, contexto
     * global ou inicialização de hardware neste tipo. O chamador fornece as
     * funções reais de tempo/NVS, I2C e armazenamento no firmware ESP32.
     */
    struct INITIALIZERS {
        obc_time::STATUS (*start_boot)(
            uint64_t& boot_id
        );
        bool (*init_i2c)(
        );
        obc_storage::RESULT (*init_storage)(
        );
    };

    /**
     * @brief Resultados independentes de uma sequência de inicialização.
     * @details boot_id zero significa indisponível, nunca um boot inventado.
     * i2c_ready confirma apenas controlador/executor, não sensores ou montagem.
     * storage_result conserva status, quantidade, errno e tentativas originais.
     * Não representa modo de voo, saúde contínua ou horário UTC válido.
     */
    struct STARTUP_REPORT {
        uint64_t boot_id;
        obc_time::STATUS time_status;
        bool i2c_ready;
        obc_storage::RESULT storage_result;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Inicializa uma vez por chamada, na ordem tempo/NVS, I2C e SD.
     * @details Tenta todos os serviços mesmo quando um anterior retorna falha.
     * Não repete, reinicia, grava registros ou cria tarefas próprias. Não copia
     * drivers nem altera suas políticas. Chamador deve invocar no setup, antes
     * de iniciar consumidores e sem concorrência. Função nula invalida a tabela:
     * nenhuma função é chamada, estados temporal/SD são INVALID_ARGUMENT,
     * boot_id zero e i2c_ready falso. Não há retenção do relatório neste módulo.
     * @param initializers Funções não nulas dos três serviços.
     * @return Resultados para diagnóstico, sem publicar estado global.
     */
    STARTUP_REPORT Initialize(
        const INITIALIZERS& initializers
    );

} // namespace obc_startup
