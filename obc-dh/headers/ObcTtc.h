/**
 * @file ObcTtc.h
 * @brief Integração de bancada do OBC com o transporte TT&C existente.
 */

#pragma once

#include <Arduino.h> // Serial, delay e temporização do framework.
#include "Pinout.h" // GPIOs do rádio, distintos do SD/HSPI.
#include "Ttc.h" // Ttc, TC_FRAME e contrato de TC/TM.
#include "UmbilicalLink.h" // Transporte pela UART0/USB.

namespace obc_ttc {

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Inicializa uma vez o enlace após os serviços do boot.
     * @details Usa a tarefa TT&C no Core 0, prioridade 10. Rádio ausente
     * não desabilita o umbilical. Não altera políticas de tempo, SD ou I2C.
     * @return true se a tarefa iniciou com pelo menos um transporte pronto.
     */
    bool Init();

    /**
     * @brief Atende um TC por chamada, esperando até 20 ms pela recepção.
     * @details ping recebe ACK; get_status/request_tm recebem ACK e TM
     * de bancada, com null para dados indisponíveis. Outros comandos
     * conhecidos recebem invalid_cmd. Não executa Update() nem ações de
     * missão. Quando inativo, cede execução por 20 ms.
     */
    void Service();

} // namespace obc_ttc
