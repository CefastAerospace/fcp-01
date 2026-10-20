/**
 * @file I2cSchedulingCheck.cpp
 * @brief Verificação nativa da política de atendimento I2C, sem hardware.
 *
 * Executar dentro de obc-dh/:
 * g++ -std=c++11 -Wall -Wextra -Werror tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-check
 * /tmp/fcp01-i2c-check
 */

#include "../headers/I2cScheduling.h" // Metadados e política usados pelo executor.
#include <assert.h> // assert.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "I2cScheduling.h".
 */

// ─── HELPERS ──────────────────────────────────────────────────────

// Verifica promoção por conclusão, preservação da base e saturação no teto.
static void CheckPriority(
) {
    using namespace i2c::scheduling;
    REQUEST_ORDER low = {0, 0, 1000, 0};
    const REQUEST_ORDER high = {4, 250, 1000, 0};
    const REQUEST_ORDER* requests[] = {&low, &high};

    assert(GetEffectivePriority(low) == 0);
    assert(SelectRequest(requests, 2, 250) == 1);
    assert(SelectRequest(requests, 2, 900) == 1);

    RegisterCompletion(low);
    assert(GetEffectivePriority(low) == 1);
    RegisterCompletion(low);
    assert(GetEffectivePriority(low) == 2);
    RegisterCompletion(low);
    assert(GetEffectivePriority(low) == 3);
    assert(SelectRequest(requests, 2, 900) == 1);
    RegisterCompletion(low);
    assert(GetEffectivePriority(low) == 4);
    assert(low.base_priority == 0);
    assert(SelectRequest(requests, 2, 900) == 0);

    for (size_t index = 0; index < 1000; ++index) {
        RegisterCompletion(low);
    }
    assert(GetEffectivePriority(low) == 4);
    assert(low.completed_while_waiting == 4 * completions_per_level);

    REQUEST_ORDER medium = {2, 0, 1000, 0};
    RegisterCompletion(medium);
    assert(GetEffectivePriority(medium) == 3);
    RegisterCompletion(medium);
    assert(GetEffectivePriority(medium) == 4);
    assert(medium.base_priority == 2);
}

// Uma solicitação antiga alcança o teto e precede novas chegadas prioritárias.
static void CheckArrivalOrder(
) {
    using namespace i2c::scheduling;
    REQUEST_ORDER low = {0, 0, 1000, 0};
    for (size_t index = 0; index < 4 * completions_per_level; ++index) {
        RegisterCompletion(low);
    }
    for (uint32_t now_ms = 300; now_ms < 1000; now_ms += 25) {
        const REQUEST_ORDER high = {4, now_ms, 1000, 0};
        const REQUEST_ORDER* requests[] = {&low, &high};
        assert(SelectRequest(requests, 2, now_ms) == 0);
    }

    const REQUEST_ORDER first = {1, 500, 1000, 0};
    const REQUEST_ORDER second = {1, 500, 1000, 0};
    const REQUEST_ORDER* tied[] = {&first, &second};
    assert(SelectRequest(tied, 2, 500) == 0);
    assert(SelectRequest(nullptr, 0, 500) == 0);
}

// Confere vencimento exato, limpeza prioritária e estouro do contador em ms.
static void CheckExpirationAndWrap(
) {
    using namespace i2c::scheduling;
    const REQUEST_ORDER expired = {0, 100, 10, 0};
    const REQUEST_ORDER high = {3, 105, 1000, 0};
    const REQUEST_ORDER* requests[] = {&high, &expired};
    assert(!IsExpired(expired, 109));
    assert(IsExpired(expired, 110));
    assert(SelectRequest(requests, 2, 110) == 1);

    const REQUEST_ORDER wrapped = {0, UINT32_MAX - 49, 200, 0};
    assert(!IsExpired(wrapped, 149));
    assert(IsExpired(wrapped, 150));
}

int main(
) {
    CheckPriority();
    CheckArrivalOrder();
    CheckExpirationAndWrap();
    return 0;
}
