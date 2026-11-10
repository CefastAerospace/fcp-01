/** @file ObcDataFlowCheck.cpp
 * @brief Tempo, fila, perdas, diário POSIX e observações do receptor real.
 */
#include "ObcDataFlow.h" // Estado, fila e diário reais.
#include "PayloadReceiver.h" // Receptor FCPU real.
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlow.h".
 */

namespace {

    struct TEST_STORAGE {
        obc_storage::Storage storage;
        uint32_t position = 0;
        uint32_t limit = 180;
        bool fail = false;
    };

    // Usa arquivos reais, com limite baixo e falha explícita para testar rotação.
    obc_storage::RESULT Open(
        void* context,
        const char* name
    ) {
        TEST_STORAGE& test = *static_cast<TEST_STORAGE*>(context);
        const obc_storage::RESULT result = test.storage.OpenNew(name);
        if (result.status == obc_storage::STATUS::OK) test.position = 0;
        return result;
    }

    obc_storage::RESULT Append(
        void* context,
        const uint8_t* bytes,
        size_t size
    ) {
        TEST_STORAGE& test = *static_cast<TEST_STORAGE*>(context);
        if (test.fail) return {obc_storage::STATUS::WRITE_FAILED, 0, EIO, 1};
        if (size > test.limit - test.position) return {obc_storage::STATUS::FILE_LIMIT, 0, 0, 0};
        const obc_storage::RESULT result = test.storage.Append(bytes, size);
        if (result.status == obc_storage::STATUS::OK) test.position += static_cast<uint32_t>(size);
        return result;
    }

    obc_storage::RESULT Close(
        void* context
    ) {
        return static_cast<TEST_STORAGE*>(context)->storage.Close();
    }

    // Vetor FCPU mínimo; CRC é exercitado independentemente na suíte anterior.
    std::vector<uint8_t> Frame(
        uint32_t sequence
    ) {
        const char json[] = "{\"id\":0,\"messages\":{}}";
        std::vector<uint8_t> frame(26 + sizeof(json) - 1, 0);
        std::memcpy(frame.data(), "FCPU", 4);
        frame[4] = 1;
        frame[5] = 1;
        frame[6] = 1;
        for (unsigned index = 0; index < 4; ++index) {
            frame[14 + index] = static_cast<uint8_t>(sequence >> (8 * index));
            frame[18 + index] = static_cast<uint8_t>((sizeof(json) - 1) >> (8 * index));
        }
        std::memcpy(frame.data() + 22, json, sizeof(json) - 1);
        const uint32_t crc = obc_record::ComputeCrc32(frame.data(), frame.size() - 4);
        for (unsigned index = 0; index < 4; ++index) frame[frame.size() - 4 + index] = static_cast<uint8_t>(crc >> (8 * index));
        return frame;
    }

} // namespace

int main(
) {
    obc_flow::State state;
    state.Init(7);
    assert(state.ComposeTelemetry(0).available_fields == 0);
    obc_time::TIME_REFERENCE reference = {};
    assert(state.SetTime(0, 100, 90, 0, reference));
    assert(reference.reception.monotonic_ms == 90 && reference.delay_ms == 0);
    assert(state.ComposeTelemetry(100).available_fields & obc_data::TM_TIMESTAMP);
    assert(state.ComposeTelemetry(1090).timestamp == 1);
    const uint64_t wrapped = (1ULL << 32) + 10;
    assert(state.SetTime(1000, wrapped, UINT32_MAX - 4, 162, reference));
    assert(reference.reception.monotonic_ms == (1ULL << 32) - 5);
    assert(state.ComposeTelemetry(wrapped).timestamp == 1000);
    assert(!state.SetTime(100, 10, 11, 0, reference));
    assert(state.SetTime(UINT32_MAX, 20, 20, 0, reference));
    assert(!(state.ComposeTelemetry(1020).available_fields & obc_data::TM_TIMESTAMP));
    state.DropRecords(UINT64_MAX);
    state.DropRecords();
    assert(state.GetStatistics().records_dropped == UINT64_MAX);
    state.Init(8);
    assert(state.ComposeTelemetry(5000).available_fields == 0);

    obc_payload::Receiver receiver;
    assert(receiver.Init(8));
    const uint32_t sequences[] = {1, 4, 4, 2, 4};
    for (size_t index = 0; index < 5; ++index) {
        const std::vector<uint8_t> frame = Frame(sequences[index]);
        const auto result = receiver.Feed(frame.data(), frame.size(), index);
        assert(result.status == (index == 0 || index == 3 ? obc_payload::STATUS::READY : obc_payload::STATUS::SEQUENCE_ERROR));
    }
    assert(receiver.GetStatistics().gaps == 3);
    assert(receiver.GetStatistics().missing_positions == 2);
    assert(receiver.GetStatistics().complete_candidates == 5);
    assert(state.ObserveSnapshot(*receiver.GetSnapshot()));
    auto tm = state.ComposeTelemetry(100);
    assert(tm.available_fields == obc_data::TM_AIRCRAFT && tm.aircraft_count == 0);
    state.SetPayloadStatistics(5, 3, 2, 0);
    assert(state.GetStatistics().received == 5 && state.GetStatistics().gaps == 2);
    std::vector<uint8_t> bad = Frame(3);
    bad.back() ^= 1;
    assert(receiver.Feed(bad.data(), bad.size(), 5).status == obc_payload::STATUS::INVALID_FRAME);
    assert(receiver.GetStatistics().complete_candidates == 6);
    const auto partial = Frame(3);
    assert(receiver.Feed(partial.data(), 10, 6).status == obc_payload::STATUS::WAITING);
    assert(receiver.CheckTimeout(1000) == obc_payload::STATUS::TIMEOUT);
    assert(receiver.GetStatistics().complete_candidates == 6);

    uint8_t bytes[99];
    size_t size = 0;
    const obc_time::EVENT_STAMP stamp = {8, 100};
    assert(obc_record::EncodeTelemetryV3(stamp, tm, bytes, sizeof(bytes), size) == obc_record::STATUS::OK);
    obc_flow::RecordQueue queue;
    for (unsigned index = 0; index < 8; ++index) assert(queue.Push(bytes, size));
    assert(!queue.Push(bytes, size));
    obc_flow::RECORD_BLOCK block = {};
    for (unsigned index = 0; index < 8; ++index) {
        assert(queue.Pop(block));
        assert(block.size == size && std::memcmp(block.bytes, bytes, size) == 0);
    }
    assert(!queue.Pop(block));

    char directory[] = "/tmp/fcp01-flow-XXXXXX";
    assert(mkdtemp(directory));
    TEST_STORAGE test;
    assert(test.storage.Init(directory).status == obc_storage::STATUS::OK);
    const obc_flow::JOURNAL_IO io = {Open, Append, Close, &test};
    obc_flow::Journal journal;
    assert(journal.Init(io, stamp, 0));
    obc_record::RECORD time_record = {};
    time_record.header = {1, obc_data::RECORD_TYPE::TIME_REFERENCE, 36, stamp};
    time_record.body.time_reference = {stamp, 1000, 0};
    uint8_t time_bytes[36];
    size_t time_size = 0;
    assert(obc_record::Encode(time_record, time_bytes, sizeof(time_bytes), time_size) == obc_record::STATUS::OK);
    assert(journal.Write(time_bytes, time_size));
    assert(journal.Write(bytes, size));
    assert(journal.Write(bytes, size)); // Segundo segmento: BOOT + referência + TM.
    assert(journal.GetWritten() == 6 && journal.GetFailures() == 1);
    const std::string second = std::string(directory) + "/boot_0000000000000008_00000001.bin";
    std::ifstream file(second, std::ios::binary);
    const std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    assert(content.size() == 175);
    assert(std::memcmp(content.data() + 40, time_bytes, 36) == 0);
    assert(std::memcmp(content.data() + 76, bytes, size) == 0);
    test.fail = true;
    assert(!journal.Write(time_bytes, time_size));
    assert(!journal.IsReady() && journal.GetFailures() >= 2);
    assert(!journal.Write(bytes, size));
    assert(test.storage.End().status == obc_storage::STATUS::OK);
    for (unsigned index = 0; index < 3; ++index) {
        char path[256];
        std::snprintf(path, sizeof(path), "%s/boot_0000000000000008_%08x.bin", directory, index);
        std::remove(path);
    }
    assert(rmdir(directory) == 0);
    std::puts("INT-006 portátil: tempo, fila, rotação, falhas e observações aprovados");
}
