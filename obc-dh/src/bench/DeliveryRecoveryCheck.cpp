#include "DeliveryRecoveryCheck.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "DeliveryRecoveryCheck.h".
 */

namespace obc_delivery {
namespace check {
namespace {

    // Reproduz perda do leitor, 32 admissões e falha de persistência de todas.
    bool FillFailedCache(
        Queue& queue,
        obc_data::PAYLOAD_SNAPSHOT& snapshot
    ) {
        queue.Init(7, true);
        queue.ReaderFailed();
        snapshot = {};
        snapshot.stamp = {7, 1};
        snapshot.aircraft_count = 1;
        for (uint32_t index = 1; index <= cache_slots; ++index) {
            snapshot.aircraft[0].icao = index;
            queue.ObserveValidatedSnapshot(snapshot);
        }
        RECORD writing = {};
        size_t count = 0;
        while (queue.BeginStorage(writing)) {
            queue.FinishStorage(writing, false);
            ++count;
        }
        const auto statistics = queue.GetStatistics();
        return count == cache_slots && statistics.occupied == cache_slots
            && statistics.high_water == cache_slots && statistics.accepted == cache_slots
            && statistics.lost == 0 && !statistics.recovery_occupied;
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

bool CheckRecoveryReservation(
) {
    std::unique_ptr<Queue> queue(new (std::nothrow) Queue);
    std::unique_ptr<obc_data::PAYLOAD_SNAPSHOT> snapshot(new (std::nothrow) obc_data::PAYLOAD_SNAPSHOT);
    if (!queue || !snapshot || !FillFailedCache(*queue, *snapshot)) return false;
    const DELIVERY older = {{6, 1}, 456, {0xabcdef, obc_data::SNAPSHOT_ALTITUDE, 0, 0, 99}};
    queue->RecheckStorage();
    if (!queue->Restore(queue->GetFrontier(), &older)) return false;
    if (!queue->Restore(queue->GetFrontier(), &older)) return false;
    auto statistics = queue->GetStatistics();
    if (statistics.occupied != cache_slots || !statistics.recovery_occupied
            || statistics.evicted || statistics.lost || queue->NeedsRecovery()) return false;

    // A reserva durável não vira uma 33ª admissão e não modifica as 32 cópias.
    snapshot->aircraft[0].icao = 33;
    queue->ObserveValidatedSnapshot(*snapshot);
    if (queue->GetStatistics().accepted != 32 || queue->GetStatistics().lost != 1) return false;
    DELIVERY sending = {};
    if (!queue->BeginTransmit(0, 10000, sending) || Compare(sending.id, older.id)
            || sending.monotonic_ms != older.monotonic_ms || sending.aircraft.altitude != 99) return false;
    if (!queue->FinishTransmit(sending.id, true) || !queue->Confirm(sending.id)) return false;
    RECORD confirmation = {};
    if (!queue->BeginStorage(confirmation) || !confirmation.confirmation
            || Compare(confirmation.delivery.id, older.id)) return false;

    // CONFIRM em gravação conserva a reserva; sua conclusão permite o próximo ID.
    DELIVERY next = older;
    next.id.sequence = 2;
    queue->RecheckStorage();
    if (queue->Restore(queue->GetFrontier(), &next)) return false;
    if (!queue->GetStatistics().recovery_occupied) return false;
    queue->FinishStorage(confirmation, false);
    if (queue->GetStatistics().recovery_occupied || queue->GetStatistics().occupied != 32) return false;
    if (!queue->Restore(queue->GetFrontier(), &next)
            || !queue->BeginTransmit(1, 10000, sending) || Compare(sending.id, next.id)) return false;
    if (!queue->FinishTransmit(sending.id, true) || !queue->Confirm(sending.id)) return false;
    if (!queue->BeginStorage(confirmation) || !confirmation.confirmation) return false;
    queue->FinishStorage(confirmation, true);
    if (queue->GetStatistics().recovery_occupied) return false;

    // As 32 admissões originais continuam com os mesmos IDs e conteúdo, em ordem.
    for (uint64_t index = 1; index <= cache_slots; ++index) {
        if (!queue->Restore(queue->GetFrontier(), nullptr)
                || !queue->BeginTransmit(index + 1, 10000, sending)) return false;
        if (sending.id.boot != 7 || sending.id.sequence != index || sending.aircraft.icao != index) return false;
        if (!queue->FinishTransmit(sending.id, true) || !queue->Confirm(sending.id)) return false;
    }
    statistics = queue->GetStatistics();
    if (statistics.occupied || statistics.recovery_occupied || statistics.evicted
            || statistics.accepted != 32 || statistics.confirmed != 34 || statistics.high_water != 32) return false;

    // Remontagem durante TX não altera a reserva ou o ID do consumidor existente.
    if (!FillFailedCache(*queue, *snapshot)) return false;
    if (!queue->BeginTransmit(0, 10000, sending)) return false;
    const ID inflight = sending.id;
    queue->RecheckStorage();
    if (!queue->Restore(queue->GetFrontier(), &older)) return false;
    if (queue->BeginTransmit(10000, 10000, sending)) return false;
    if (!queue->FinishTransmit(inflight, true) || !queue->Confirm(inflight)) return false;
    if (!queue->GetStatistics().recovery_occupied || queue->GetStatistics().occupied != 31) return false;
    if (!queue->Restore(queue->GetFrontier(), nullptr)
            || !queue->BeginTransmit(10001, 10000, sending) || Compare(sending.id, older.id)) return false;
    if (!queue->FinishTransmit(sending.id, true) || !queue->Confirm(sending.id)) return false;
    if (!queue->BeginStorage(confirmation) || !confirmation.confirmation) return false;
    queue->FinishStorage(confirmation, true);
    if (queue->GetStatistics().recovery_occupied || queue->GetStatistics().occupied != 31) return false;
    queue->Init(8, false);
    return queue->GetStatistics().occupied == 0 && !queue->GetStatistics().recovery_occupied;
}

} // namespace check
} // namespace obc_delivery
