/** @file ObcI2cHealthCheck.cpp
 * @brief Verifica fronteiras, estados e registros I2C sem Arduino ou FreeRTOS.
 */
#include "ObcI2cHealth.h" // Avaliação portátil e codec reais.
#include <cassert>
#include <cstring>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcI2cHealth.h".
 */

namespace {

    // Confere layout real, código e instante de avaliação, sem persistência simulada.
    void CheckEvent(
        const obc_i2c_health::REPORT& report,
        bool fault
    ) {
        obc_record::RECORD record = {};
        assert(obc_i2c_health::MakeEvent(report, {7, 0x100000123ULL}, record));
        assert(record.header.type == (fault ? obc_data::RECORD_TYPE::FAULT : obc_data::RECORD_TYPE::EVENT));
        assert(record.body.event.source == obc_data::SOURCE::OBC && record.body.event.code == 259);
        assert(record.body.event.severity == (fault ? obc_data::SEVERITY::ERROR : obc_data::SEVERITY::INFO));
        assert(std::strstr(record.body.event.description, "i2c ") == record.body.event.description);
        uint8_t bytes[96];
        size_t size = 0;
        assert(obc_record::Encode(record, bytes, sizeof(bytes), size) == obc_record::STATUS::OK && size == 96);
        obc_record::RECORD decoded = {};
        size_t consumed = 0;
        assert(obc_record::Decode(bytes, size, decoded, consumed) == obc_record::STATUS::OK && consumed == 96);
        assert(decoded.header.stamp.boot_id == 7 && decoded.header.stamp.monotonic_ms == 0x100000123ULL);
        assert(decoded.body.event.code == 259);
    }

} // namespace

int main(
) {
    using namespace obc_i2c_health;
    using i2c::SERVICE_STATE;
    Monitor monitor;
    SAMPLE sample = {};
    REPORT report = {};
    assert(!monitor.Evaluate(sample, report) && report.state == STATE::INACTIVE);
    sample.service = SERVICE_STATE::READY;
    assert(monitor.Evaluate(sample, report));
    assert(report.state == STATE::OK && report.activity == ACTIVITY::IDLE);
    CheckEvent(report, false);
    sample.sampled_ms = UINT32_MAX;
    assert(!monitor.Evaluate(sample, report) && report.age_ms == 0);
    assert(report.limit_ms == 0 && report.state == STATE::OK);

    // Callback em zero: igualdade não dispara, excesso dispara uma única vez.
    Monitor callback;
    sample = {SERVICE_STATE::READY, true, 0, 0, 0};
    assert(!callback.Evaluate(sample, report));
    sample.sampled_ms = 100;
    assert(!callback.Evaluate(sample, report) && report.state == STATE::OK);
    sample.sampled_ms = 101;
    assert(callback.Evaluate(sample, report) && report.state == STATE::SUSPECT);
    assert(report.activity == ACTIVITY::CALLBACK && report.limit_ms == 100);
    CheckEvent(report, true);
    sample.sampled_ms = 102;
    assert(!callback.Evaluate(sample, report));
    // Depois de detectada, nem um wrap completo faz a suspeita desaparecer.
    sample.sampled_ms = 0;
    assert(!callback.Evaluate(sample, report) && report.state == STATE::SUSPECT);
    sample.callback_running = false;
    assert(callback.Evaluate(sample, report) && report.state == STATE::OK);
    CheckEvent(report, false);
    assert(!callback.Evaluate(sample, report));

    // Outro callback começa sem herdar a suspeita anterior.
    sample = {SERVICE_STATE::READY, true, 50, 0, 50};
    assert(!callback.Evaluate(sample, report) && report.state == STATE::OK);
    sample.sampled_ms = 151;
    assert(callback.Evaluate(sample, report));
    sample.callback_started_ms = 152;
    sample.sampled_ms = 152;
    assert(callback.Evaluate(sample, report) && report.state == STATE::OK);

    // Recuperação tem fronteira própria e não é confundida com disponibilidade.
    sample = {SERVICE_STATE::RECOVERING, false, 0, 200, 200};
    assert(callback.Evaluate(sample, report));
    assert(report.activity == ACTIVITY::RECOVERY && report.service == SERVICE_STATE::RECOVERING);
    CheckEvent(report, false);
    sample.sampled_ms = 400;
    assert(!callback.Evaluate(sample, report) && report.state == STATE::OK);
    sample.sampled_ms = 401;
    assert(callback.Evaluate(sample, report) && report.state == STATE::SUSPECT && report.limit_ms == 200);
    CheckEvent(report, true);
    sample.service = SERVICE_STATE::UNAVAILABLE;
    assert(callback.Evaluate(sample, report) && report.state == STATE::UNAVAILABLE);
    CheckEvent(report, true);
    sample.sampled_ms = 100000;
    assert(!callback.Evaluate(sample, report) && report.state == STATE::UNAVAILABLE);
    sample.service = SERVICE_STATE::RECOVERING;
    sample.recovery_started_ms = sample.sampled_ms;
    assert(callback.Evaluate(sample, report) && report.state == STATE::OK);
    sample.service = SERVICE_STATE::READY;
    assert(callback.Evaluate(sample, report) && report.activity == ACTIVITY::IDLE);
    CheckEvent(report, false);

    // Recuperação normal atravessa wrap; prazos independem do relógio de boot u64.
    Monitor wrap;
    sample = {SERVICE_STATE::RECOVERING, false, 0, UINT32_MAX - 99, 100};
    assert(!wrap.Evaluate(sample, report) && report.age_ms == 200 && report.state == STATE::OK);
    sample.sampled_ms = 101;
    assert(wrap.Evaluate(sample, report) && report.age_ms == 201);
    Monitor first_suspect;
    assert(first_suspect.Evaluate(sample, report) && report.state == STATE::SUSPECT);
    Monitor first_unavailable;
    sample.service = SERVICE_STATE::UNAVAILABLE;
    assert(first_unavailable.Evaluate(sample, report));
    Monitor normal;
    sample = {SERVICE_STATE::READY, false, 0, 0, 0};
    assert(!normal.Evaluate(sample, report));
    sample.callback_running = true;
    assert(!normal.Evaluate(sample, report));
    sample.callback_running = false;
    assert(!normal.Evaluate(sample, report));

    // Dados inválidos e boot ausente não publicam registros parcialmente construídos.
    const REPORT previous = report;
    sample.service = static_cast<SERVICE_STATE>(99);
    assert(!normal.Evaluate(sample, report));
    assert(report.service == previous.service && report.sampled_ms == previous.sampled_ms);
    obc_record::RECORD untouched = {};
    untouched.header.total_bytes = 123;
    assert(!MakeEvent(report, {0, 0}, untouched) && untouched.header.total_bytes == 123);
    report.state = STATE::INACTIVE;
    assert(!MakeEvent(report, {7, 0}, untouched) && untouched.header.total_bytes == 123);
    std::puts("Saude I2C: limites, ociosidade, recuperacao, wrap, transicoes e registros aprovados");
}
