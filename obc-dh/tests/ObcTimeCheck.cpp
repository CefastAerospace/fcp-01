/**
 * @file ObcTimeCheck.cpp
 * @brief Verificação nativa de referências, limites e persistência dos boots.
 *
 * Executar dentro de obc-dh/:
 * g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcTime.cpp tests/ObcTimeCheck.cpp -o /tmp/fcp01-time-check
 * /tmp/fcp01-time-check
 */

#include "ObcTime.h" // Contrato temporal portátil e callbacks da persistência.
#include <assert.h> // assert, sem dependência de framework de testes.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTime.h".
 */

// ─── STRUCTS ──────────────────────────────────────────────────────

// Substitui somente o armazenamento, mantendo a lógica real de reserva.
struct TEST_STORAGE {
    uint64_t count;
    bool fail_read;
    bool fail_write;
    bool persist_then_fail;
    unsigned writes;
    uint64_t* published_id;
};

// ─── HELPERS ──────────────────────────────────────────────────────

// Simula leitura existente/ausente e erro sem transformar falha em zero.
static bool ReadCount(
    void* context,
    uint64_t& count
) {
    TEST_STORAGE& storage = *static_cast<TEST_STORAGE*>(context);
    if (storage.fail_read) {
        return false;
    }
    count = storage.count;
    return true;
}

// Confere que nenhum ID é publicado antes da confirmação e simula uma
// escrita de resultado incerto: persistiu, mas o chamador recebeu falha.
static bool WriteCount(
    void* context,
    uint64_t count
) {
    TEST_STORAGE& storage = *static_cast<TEST_STORAGE*>(context);
    assert(*storage.published_id == 0);
    ++storage.writes;
    if (storage.fail_write) {
        return false;
    }
    storage.count = count;
    return !storage.persist_then_fail;
}

// Verifica referências independentes, horários anteriores e ausência de UTC.
static void CheckReferences(
) {
    using namespace obc_time;
    const EVENT_STAMP reception = {1, 5000};
    TIME_REFERENCE captured = {};
    assert(CreateReference(reception, 1000, captured) == STATUS::OK);
    assert(captured.reception.boot_id == 1);
    assert(captured.reception.monotonic_ms == 5000);
    assert(captured.unix_s == 1000);
    assert(captured.delay_ms == reference_delay_ms);

    // Valores fixos de ensaio independem da calibração global de voo.
    const TIME_REFERENCE first = {{1, 5000}, 1000, 200};
    const EVENT_STAMP before = {1, 0};
    const EVENT_STAMP after = {1, 5015};
    int64_t unix_ms = -1;
    assert(Reconstruct(before, &first, unix_ms) == STATUS::OK);
    assert(unix_ms == 995200);
    assert(Reconstruct(reception, &first, unix_ms) == STATUS::OK);
    assert(unix_ms == 1000200);
    assert(Reconstruct(after, &first, unix_ms) == STATUS::OK);
    assert(unix_ms == 1000215);

    // Correções sucessivas são explícitas; não alteram a referência antiga,
    // o evento original ou a compensação usada na reconstrução anterior.
    const TIME_REFERENCE second = {{1, 10000}, 1006, 500};
    assert(Reconstruct(before, &second, unix_ms) == STATUS::OK);
    assert(unix_ms == 996500);
    assert(Reconstruct(before, &first, unix_ms) == STATUS::OK);
    assert(unix_ms == 995200);
    assert(first.delay_ms == 200 && before.monotonic_ms == 0);

    assert(Reconstruct(before, nullptr, unix_ms) == STATUS::NO_REFERENCE);
    assert(unix_ms == 0);
    const EVENT_STAMP next_boot = {2, 15};
    assert(Reconstruct(next_boot, &first, unix_ms) == STATUS::BOOT_MISMATCH);
    assert(unix_ms == 0);
    assert(Reconstruct(next_boot, nullptr, unix_ms) == STATUS::NO_REFERENCE);
    TIME_REFERENCE next_reference = {};
    assert(CreateReference({2, 5000}, 2000, next_reference) == STATUS::OK);
    assert(Reconstruct(next_boot, &next_reference, unix_ms) == STATUS::OK);

    const EVENT_STAMP invalid = {0, 0};
    assert(Reconstruct(invalid, &first, unix_ms) == STATUS::INVALID_ARGUMENT);
    assert(unix_ms == 0);
    assert(CreateReference(invalid, 1000, captured) == STATUS::INVALID_ARGUMENT);
    assert(captured.reception.boot_id == 0);
    assert(Reconstruct(before, &captured, unix_ms) == STATUS::INVALID_ARGUMENT);
}

// Exercita limites sem esperar o timer real nem converter intervalos fora da faixa.
static void CheckTimeLimits(
) {
    using namespace obc_time;
    int64_t unix_ms;
    const TIME_REFERENCE wrapped = {{1, static_cast<uint64_t>(UINT32_MAX) - 49}, 1000, 200};
    const EVENT_STAMP after_wrap = {1, static_cast<uint64_t>(UINT32_MAX) + 150};
    assert(Reconstruct(after_wrap, &wrapped, unix_ms) == STATUS::OK);
    assert(unix_ms == 1000399);

    const TIME_REFERENCE epoch = {{1, 200}, 0, 200};
    assert(Reconstruct({1, 0}, &epoch, unix_ms) == STATUS::OK);
    assert(unix_ms == 0);
    const TIME_REFERENCE before_epoch = {{1, 201}, 0, 200};
    assert(Reconstruct({1, 0}, &before_epoch, unix_ms) == STATUS::OUT_OF_RANGE);
    assert(unix_ms == 0);

    const TIME_REFERENCE large = {{1, 0}, UINT32_MAX, UINT32_MAX};
    assert(Reconstruct({1, 0}, &large, unix_ms) == STATUS::OK);
    assert(unix_ms == 4299262262295LL);

    const TIME_REFERENCE origin = {{1, 0}, 0, 200};
    const uint64_t last_ms = static_cast<uint64_t>(INT64_MAX) - 200;
    assert(Reconstruct({1, last_ms}, &origin, unix_ms) == STATUS::OK);
    assert(unix_ms == INT64_MAX);
    assert(Reconstruct({1, last_ms + 1}, &origin, unix_ms) == STATUS::OUT_OF_RANGE);
    assert(unix_ms == 0);
    assert(Reconstruct({1, UINT64_MAX}, &origin, unix_ms) == STATUS::OUT_OF_RANGE);
    const TIME_REFERENCE far_future = {{1, UINT64_MAX}, 1000, 200};
    assert(Reconstruct({1, UINT64_MAX}, &far_future, unix_ms) == STATUS::OK);
    assert(unix_ms == 1000200);
    assert(Reconstruct({1, 0}, &far_future, unix_ms) == STATUS::OUT_OF_RANGE);
}

// Simula boots e erros de persistência; ID só existe depois da escrita confirmada.
static void CheckBoots(
) {
    using namespace obc_time;
    uint64_t boot_id = 99;
    TEST_STORAGE memory = {0, false, false, false, 0, &boot_id};
    const BOOT_STORAGE storage = {ReadCount, WriteCount, &memory};
    assert(StartBoot(storage, boot_id) == STATUS::OK);
    assert(boot_id == 1 && memory.count == 1 && memory.writes == 1);
    assert(boot_id - 1 == 0);
    assert(StartBoot(storage, boot_id) == STATUS::OK);
    assert(boot_id == 2 && memory.count == 2 && boot_id - 1 == 1);

    memory.fail_read = true;
    assert(StartBoot(storage, boot_id) == STATUS::STORAGE_ERROR);
    assert(boot_id == 0 && memory.count == 2 && memory.writes == 2);
    memory.fail_read = false;
    memory.fail_write = true;
    assert(StartBoot(storage, boot_id) == STATUS::STORAGE_ERROR);
    assert(boot_id == 0 && memory.count == 2 && memory.writes == 3);
    memory.fail_write = false;
    assert(StartBoot(storage, boot_id) == STATUS::OK);
    assert(boot_id == 3);

    memory.persist_then_fail = true;
    assert(StartBoot(storage, boot_id) == STATUS::STORAGE_ERROR);
    assert(boot_id == 0 && memory.count == 4);
    memory.persist_then_fail = false;
    assert(StartBoot(storage, boot_id) == STATUS::OK);
    assert(boot_id == 5);

    memory.count = UINT64_MAX - 1;
    assert(StartBoot(storage, boot_id) == STATUS::OK);
    assert(boot_id == UINT64_MAX);
    const unsigned writes = memory.writes;
    assert(StartBoot(storage, boot_id) == STATUS::COUNTER_EXHAUSTED);
    assert(boot_id == 0 && memory.writes == writes);

    const BOOT_STORAGE missing_read = {nullptr, WriteCount, &memory};
    const BOOT_STORAGE missing_write = {ReadCount, nullptr, &memory};
    assert(StartBoot(missing_read, boot_id) == STATUS::INVALID_ARGUMENT);
    assert(boot_id == 0);
    assert(StartBoot(missing_write, boot_id) == STATUS::INVALID_ARGUMENT);
    assert(boot_id == 0 && memory.writes == writes);
}

int main(
) {
    CheckReferences();
    CheckTimeLimits();
    CheckBoots();
    return 0;
}
