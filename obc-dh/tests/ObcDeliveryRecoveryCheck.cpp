/** @file ObcDeliveryRecoveryCheck.cpp
 * @brief Executa no PC a mesma verificação da reserva usada na bancada ESP32.
 */
#include "DeliveryRecoveryCheck.h" // Verificação sem hardware da fila real.
#include <cstdio>

int main(
) {
    if (!obc_delivery::check::CheckRecoveryReservation()) {
        std::puts("Reserva de recuperacao: FAIL");
        return 1;
    }
    std::printf("Reserva de recuperacao: 32 admissoes preservadas, FIFO e consumidores aprovados; queue_bytes=%zu\n",
        sizeof(obc_delivery::Queue));
}
