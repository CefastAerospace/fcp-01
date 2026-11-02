#include "ObcTime.h" // Referências, resultados e reserva dos boots.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTime.h".
 */

namespace obc_time {

    // ─── HELPERS ──────────────────────────────────────────────────────

    STATUS CreateReference(
        const EVENT_STAMP& reception,
        uint32_t unix_s,
        TIME_REFERENCE& reference
    ) {
        // Não publica uma referência sem identificação do boot.
        if (reception.boot_id == 0) {
            reference = {};
            return STATUS::INVALID_ARGUMENT;
        }
        reference = {reception, unix_s, reference_delay_ms};
        return STATUS::OK;
    }

    STATUS Reconstruct(
        const EVENT_STAMP& event,
        const TIME_REFERENCE* reference,
        int64_t& unix_ms
    ) {
        // Um número isolado nunca substitui a indicação de validade.
        unix_ms = 0;
        if (event.boot_id == 0) {
            return STATUS::INVALID_ARGUMENT;
        }
        if (reference == nullptr) {
            return STATUS::NO_REFERENCE;
        }
        if (reference->reception.boot_id == 0) {
            return STATUS::INVALID_ARGUMENT;
        }
        if (event.boot_id != reference->reception.boot_id) {
            return STATUS::BOOT_MISMATCH;
        }

        // Amplia antes da multiplicação; a base sempre cabe em int64_t.
        const uint64_t base_ms = static_cast<uint64_t>(reference->unix_s) * 1000
            + reference->delay_ms;
        if (event.monotonic_ms >= reference->reception.monotonic_ms) {
            const uint64_t elapsed_ms = event.monotonic_ms
                - reference->reception.monotonic_ms;
            if (elapsed_ms > static_cast<uint64_t>(INT64_MAX) - base_ms) {
                return STATUS::OUT_OF_RANGE;
            }
            unix_ms = static_cast<int64_t>(base_ms + elapsed_ms);
        } else {
            // Subtrai magnitudes positivas para não converter uint64_t fora
            // da faixa com sinal nem fazer underflow em eventos anteriores.
            const uint64_t elapsed_ms = reference->reception.monotonic_ms
                - event.monotonic_ms;
            if (elapsed_ms > base_ms) {
                return STATUS::OUT_OF_RANGE;
            }
            unix_ms = static_cast<int64_t>(base_ms - elapsed_ms);
        }
        return STATUS::OK;
    }

    STATUS StartBoot(
        const BOOT_STORAGE& storage,
        uint64_t& boot_id
    ) {
        boot_id = 0;
        if (storage.read == nullptr || storage.write == nullptr) {
            return STATUS::INVALID_ARGUMENT;
        }

        // Zero é reservado ao estado indisponível; nunca volta após overflow.
        uint64_t previous_count = 0;
        if (!storage.read(storage.context, previous_count)) {
            return STATUS::STORAGE_ERROR;
        }
        if (previous_count == UINT64_MAX) {
            return STATUS::COUNTER_EXHAUSTED;
        }

        // Uma falha de gravação pode consumir um número, mas não publica
        // um identificador que poderia colidir após a próxima reinicialização.
        const uint64_t next_count = previous_count + 1;
        if (!storage.write(storage.context, next_count)) {
            return STATUS::STORAGE_ERROR;
        }
        boot_id = next_count;
        return STATUS::OK;
    }

} // namespace obc_time
