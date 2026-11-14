/** @file ObcTaskHealthCheck.cpp
 * @brief Verifica limites, transições e eventos de saúde com tempo explícito.
 */
#include "ObcTaskHealth.h" // Núcleo portátil e eventos reais, sem Arduino/FreeRTOS.
#include <cassert>
#include <cstring>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTaskHealth.h".
 */

int main(
) {
    using namespace obc_health;
    Monitor monitor;
    REPORT report = {};
    PROGRESS progress = {};
    assert(!monitor.Evaluate(TASK::WRITER, progress, 999999, report));
    assert(report.state == STATE::INACTIVE && report.activity == ACTIVITY::INACTIVE);
    Mark(progress, ACTIVITY::WRITE, 0, true);
    assert(!progress.enabled && progress.completions == 0);

    const TASK tasks[] = {TASK::UART, TASK::PROCESS, TASK::WRITER};
    const ACTIVITY activities[] = {ACTIVITY::TRANSPORT, ACTIVITY::PROCESSING, ACTIVITY::WRITE};
    const uint64_t limits[] = {100, 2000, 5000};
    for (size_t index = 0; index < task_count; ++index) {
        Monitor local;
        Start(progress, 0);
        Mark(progress, activities[index], 0);
        assert(GetLimitMs(tasks[index]) == limits[index]);
        assert(!local.Evaluate(tasks[index], progress, 0, report));
        assert(report.state == STATE::OK && report.age_ms == 0);
        assert(!local.Evaluate(tasks[index], progress, limits[index], report));
        assert(report.state == STATE::OK);
        assert(local.Evaluate(tasks[index], progress, limits[index] + 1, report));
        assert(report.state == STATE::SUSPECT && report.activity == activities[index]);
        assert(!local.Evaluate(tasks[index], progress, limits[index] + 2, report));

        obc_record::RECORD record = {};
        assert(MakeEvent(report, {7, limits[index] + 2}, record));
        assert(record.header.type == obc_data::RECORD_TYPE::FAULT);
        assert(record.body.event.code == 256 + index && record.body.event.source == obc_data::SOURCE::OBC);
        assert(std::strstr(record.body.event.description, "suspect") != nullptr);
        uint8_t bytes[96];
        size_t size = 0;
        assert(obc_record::Encode(record, bytes, sizeof(bytes), size) == obc_record::STATUS::OK);
        obc_record::RECORD decoded = {};
        size_t consumed = 0;
        assert(obc_record::Decode(bytes, size, decoded, consumed) == obc_record::STATUS::OK);
        assert(consumed == 96 && decoded.header.stamp.boot_id == 7);

        Mark(progress, ACTIVITY::IDLE, limits[index] + 3, true);
        assert(local.Evaluate(tasks[index], progress, limits[index] + 3, report));
        assert(report.state == STATE::OK && report.completions == 1);
        assert(MakeEvent(report, {7, limits[index] + 3}, record));
        assert(record.header.type == obc_data::RECORD_TYPE::EVENT);
        assert(obc_record::Encode(record, bytes, sizeof(bytes), size) == obc_record::STATUS::OK);
        assert(!local.Evaluate(tasks[index], progress, limits[index] + 4, report));
        // IDLE só permanece saudável se a tarefa continuar retornando/avançando.
        assert(local.Evaluate(tasks[index], progress, 2 * limits[index] + 4, report));
        assert(report.state == STATE::SUSPECT);
    }

    // Sem aeronaves, sem quadro completo ou com fila vazia, rodadas concluídas
    // continuam sendo progresso. Espera de recuperação também não mascara I/O preso.
    Monitor waiting;
    Start(progress, 0);
    for (uint64_t now = 0; now <= 8000; now += 20) {
        Mark(progress, ACTIVITY::RECOVERY_WAIT, now, true);
        assert(!waiting.Evaluate(TASK::WRITER, progress, now, report));
        assert(report.state == STATE::OK);
    }
    Mark(progress, ACTIVITY::MOUNT, 8000);
    assert(waiting.Evaluate(TASK::WRITER, progress, 13001, report));
    assert(report.state == STATE::SUSPECT && report.activity == ACTIVITY::MOUNT);

    // Uma busca de 8 s permanece saudável enquanto cada leitura progride.
    Monitor scan;
    Start(progress, 0);
    for (uint64_t now = 0; now < 8000; now += 200) {
        Mark(progress, ACTIVITY::READ, now);
        assert(!scan.Evaluate(TASK::WRITER, progress, now + 199, report));
        Mark(progress, ACTIVITY::SCAN, now + 200, true);
    }
    assert(progress.completions == 40);
    assert(scan.Evaluate(TASK::WRITER, progress, 13001, report));

    // Primeira amostra atrasada e tarefa criada que nunca chega à primeira rodada.
    Monitor startup;
    Start(progress, 0);
    assert(startup.Evaluate(TASK::UART, progress, 101, report));
    assert(report.activity == ACTIVITY::STARTING);
    progress = {};
    assert(!startup.Evaluate(TASK::UART, progress, 100000, report));
    assert(report.state == STATE::INACTIVE);

    // Amostra temporal anterior à publicação não causa underflow; instante zero é válido.
    Start(progress, 10);
    Monitor future;
    assert(!future.Evaluate(TASK::UART, progress, 9, report));
    assert(report.age_ms == 0);
    Start(progress, 0);
    Mark(progress, ACTIVITY::IDLE, 0, true);
    assert(progress.last_completed_ms == 0 && progress.completions == 1);
    progress.completions = UINT64_MAX;
    Mark(progress, ACTIVITY::IDLE, 1, true);
    assert(progress.completions == UINT64_MAX);
    const REPORT previous = report;
    assert(!future.Evaluate(static_cast<TASK>(task_count), progress, 99, report));
    assert(report.task == previous.task && report.sampled_ms == previous.sampled_ms);
    obc_record::RECORD untouched = {};
    untouched.header.total_bytes = 123;
    assert(!MakeEvent(report, {0, 0}, untouched) && untouched.header.total_bytes == 123);
    report.state = STATE::INACTIVE;
    assert(!MakeEvent(report, {7, 0}, untouched) && untouched.header.total_bytes == 123);
    std::puts("Saude OBC: limites, inicio/espera, falta de progresso, retorno e eventos aprovados");
}
