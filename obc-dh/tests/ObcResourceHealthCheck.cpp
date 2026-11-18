/** @file ObcResourceHealthCheck.cpp
 * @brief Fronteiras, histerese, progresso, wraps e eventos reais de recursos.
 */
#include "ObcResourceHealth.h" // Avaliadores portáteis e codec real.
#include <cassert>
#include <cstring>
#include <initializer_list> // Casos EVENT/FAULT com os mesmos códigos.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcResourceHealth.h".
 */

int main(
) {
    using namespace obc_resource_health;
    TtcMonitor ttc;
    TTC_SAMPLE sample = {};
    auto report = ttc.Evaluate(sample);
    assert(report.progress == STATE::INACTIVE && report.radio == STATE::INACTIVE);
    assert(!report.progress_changed && !report.radio_changed);
    sample = {true, true, true, true, 0, 2000, 0};
    report = ttc.Evaluate(sample);
    assert(report.progress == STATE::OK && report.radio == STATE::OK);
    sample.sampled_ms = 2001;
    report = ttc.Evaluate(sample);
    assert(report.progress == STATE::SUSPECT && report.progress_changed);
    sample.sampled_ms = 2002;
    assert(!ttc.Evaluate(sample).progress_changed);
    // Suspeita detectada persiste mesmo quando idade modular volta a zero.
    sample.sampled_ms = 0;
    assert(ttc.Evaluate(sample).progress == STATE::SUSPECT);
    sample.completions = 1;
    report = ttc.Evaluate(sample);
    assert(report.progress == STATE::OK && report.progress_changed);
    sample.progress_ms = UINT32_MAX - 999;
    sample.sampled_ms = 1000;
    assert(ttc.Evaluate(sample).progress == STATE::OK);
    sample.sampled_ms = 1001;
    assert(ttc.Evaluate(sample).progress == STATE::SUSPECT);
    sample.task_started = false;
    sample.radio_ready = false;
    report = ttc.Evaluate(sample);
    assert(report.progress == STATE::INACTIVE && report.radio == STATE::UNAVAILABLE);
    assert(report.radio_changed);
    assert(!ttc.Evaluate(sample).radio_changed);
    TtcMonitor absent;
    assert(absent.Evaluate({true, false, false, false, 0, 0, 0}).radio_changed);
    // Rodadas ociosas/sem TC são progresso real e mantêm o rádio independente.
    TtcMonitor idle;
    for (uint32_t now = 0; now < 10000; now += 20) {
        report = idle.Evaluate({true, true, true, false, now, now, now / 20});
        assert(report.progress == STATE::OK && report.radio == STATE::UNAVAILABLE);
    }

    MemoryMonitor memory;
    MEMORY_SAMPLE ram = {};
    ram.byte_heap = 8192;
    ram.byte_largest = 4096;
    auto health = memory.Evaluate(ram);
    assert(!health.pressure_changed && !health.stack_low_mask && !health.allocation_delta);
    ram.byte_heap = 8191;
    ram.byte_largest = 4095;
    health = memory.Evaluate(ram);
    assert(health.heap_low && health.block_low && health.pressure_changed);
    ram.byte_heap = 12287;
    ram.byte_largest = 6143;
    health = memory.Evaluate(ram);
    assert(health.heap_low && health.block_low && !health.pressure_changed);
    ram.byte_heap = 12288;
    health = memory.Evaluate(ram);
    assert(!health.heap_low && health.block_low && health.pressure_changed);
    ram.byte_largest = 6144;
    health = memory.Evaluate(ram);
    assert(!health.heap_low && !health.block_low && health.pressure_changed);
    // Independência das métricas; valor na banda não inicia episódio novo.
    ram.byte_heap = 10000;
    ram.byte_largest = 5000;
    assert(!memory.Evaluate(ram).pressure_changed);
    for (size_t index = 0; index < stack_count; ++index) {
        ram.stacks[index] = {true, 1024};
        assert(!memory.Evaluate(ram).stack_low_mask);
        ram.stacks[index].free_bytes = 1023;
        health = memory.Evaluate(ram);
        assert(health.stack_new_mask == (1U << index));
        ram.stacks[index].free_bytes = 4096;
        health = memory.Evaluate(ram);
        assert(health.stack_low_mask == (1U << index) && !health.stack_new_mask);
        ram.stacks[index].active = false;
        assert(!memory.Evaluate(ram).stack_low_mask);
        ram.stacks[index] = {true, 0};
        assert(memory.Evaluate(ram).stack_new_mask == (1U << index));
        ram.stacks[index] = {};
        memory.Evaluate(ram);
    }
    // Alocação recusada entre amostras aparece mesmo com heap recuperado.
    ram.allocation_failures = 3;
    assert(memory.Evaluate(ram).allocation_delta == 3);
    assert(!memory.Evaluate(ram).allocation_delta);
    ram.allocation_failures = UINT64_MAX;
    assert(memory.Evaluate(ram).allocation_delta == UINT64_MAX - 3);
    assert(!memory.Evaluate(ram).allocation_delta);

    const CODE codes[] = {CODE::TTC_PROGRESS, CODE::RADIO, CODE::HEAP, CODE::STACK_MARGIN, CODE::ALLOCATION};
    for (const auto code : codes) {
        for (const bool fault : {false, true}) {
            obc_record::RECORD event = {};
            assert(MakeEvent(code, fault, "resource transition", {8, 0}, event));
            uint8_t bytes[96];
            size_t written = 0;
            assert(obc_record::Encode(event, bytes, sizeof(bytes), written) == obc_record::STATUS::OK);
            obc_record::RECORD decoded = {};
            size_t consumed = 0;
            assert(obc_record::Decode(bytes, written, decoded, consumed) == obc_record::STATUS::OK);
            assert(consumed == 96 && decoded.header.stamp.boot_id == 8);
            assert(decoded.body.event.code == static_cast<uint16_t>(code));
            assert(decoded.body.event.source == obc_data::SOURCE::OBC);
            assert(decoded.header.type == (fault ? obc_data::RECORD_TYPE::FAULT : obc_data::RECORD_TYPE::EVENT));
        }
    }
    obc_record::RECORD untouched = {};
    untouched.header.total_bytes = 123;
    assert(!MakeEvent(CODE::HEAP, true, "no boot", {0, 1}, untouched));
    assert(untouched.header.total_bytes == 123);
    std::puts("Recursos OBC: progresso, wrap, histerese, pilhas, NO_MEMORY e eventos aprovados");
}
