/**
 * @file ObcTtc.h
 * @brief Integração do OBC com o transporte TT&C existente.
 */

#pragma once

#include <Arduino.h> // Serial, delay e temporização do framework.
#include "Pinout.h" // GPIOs do rádio, distintos do SD/HSPI.
#include "Ttc.h" // Ttc, TC_FRAME e contrato de TC/TM.
#include "UmbilicalLink.h" // Transporte pela UART0/USB.
#include "ObcDataFlowEsp32.h" // Estado integrado, referência UTC e registro da TM.
#include "ObcProtection.h" // Resultado opcional da política EPS/Térmico.
#include "ObcWatchdogEsp32.h" // Agendamento do reset antes da tentativa de ACK.

namespace obc_ttc {

    /** @brief Entradas de uma configuração operacional explícita, chamadas pelo loop OBC. */
    struct MODE_SERVICES {
        uint64_t max_snapshot_age_ms;
        obc_modes::SUBSYSTEM_STATUS (*read_status)();
        bool (*start_adcs)();
        bool (*stop_adcs)();
        /** @brief Quando fornecido, aplica proteção e pausa de descidas da INT-010. */
        obc_protection::RESULT (*read_protection)();
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Inicializa uma vez o enlace após os serviços do boot.
     * @details Usa a tarefa TT&C no Core 0, prioridade 10. Rádio ausente
     * não desabilita o umbilical.
     * @param modes Configuração operacional. Ausente ou incompleta inicia em BENCH.
     * @return true se a tarefa iniciou com pelo menos um transporte pronto.
     */
    bool Init(
        const MODE_SERVICES* modes = nullptr
    );

    /**
     * @brief Atende um TC por chamada, esperando até 20 ms pela recepção.
     * @details ping recebe ACK; get_status/request_tm recebem ACK e TM
     * integrada, com null para dados indisponíveis. set_time aplica UTC
     * em RAM e tenta persistir a referência no fluxo. request_log encaminha
     * I/O ao escritor com prazo de 1 s e responde lg antes de confirmar
     * enfileiramento. Consome eventos TX e cf, e envia entregas com timeout
     * provisório de 10 s. reset_obc agenda reset em 1500 ms, com ACK best
     * effort. Com configuração operacional, avalia modos antes de despachar
     * o TC. Quando inativo, ainda avalia o modo e cede 20 ms.
     */
    void Service();

    /** @brief Encaminha cópia protegida do proprietário, sem acessar o rádio. */
    LINK_DIAGNOSTICS GetDiagnostics();

} // namespace obc_ttc
