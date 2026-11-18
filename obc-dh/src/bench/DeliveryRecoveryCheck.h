/**
 * @file DeliveryRecoveryCheck.h
 * @brief Verificação compartilhada PC/ESP32 da reserva de recuperação durável.
 */
#pragma once

#include "ObcDelivery.h" // Fila real, snapshots e estados de confirmação.
#include <memory> // unique_ptr para liberar a memória antes da bancada integrada.
#include <new> // nothrow, com falha explícita de alocação na verificação.

namespace obc_delivery {
namespace check {

    /**
     * @brief Confere cache cheio, reserva ocupada, avanço FIFO e consumidores em uso.
     * @details Executar sem concorrência antes do fluxo integrado. A memória
     * temporária fica no heap e é liberada antes de inicializar o receptor.
     * @return true se todos os casos passam; false em falha ou falta de memória.
     */
    bool CheckRecoveryReservation(
    );

} // namespace check
} // namespace obc_delivery
