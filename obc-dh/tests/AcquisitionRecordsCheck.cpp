/**
 * @file AcquisitionRecordsCheck.cpp
 * @brief Verifica o codec FCP4 e sua admissão na fila existente.
 */
#include "ObcDataFlow.h" // Codec e fila usados pelo diário.
#include <array> // Buffers fixos e comparação dos bytes.
#include <cassert> // Verificações nativas.
#include <iostream> // Exporta os vetores do codificador para comparação independente.

namespace {
    // Um escritor indisponível deve recusar a operação sem contabilizar sucesso.
    void CheckJournal(
        const uint8_t* bytes,
        size_t size,
        const obc_time::EVENT_STAMP& completed
    ) {
        using obc_storage::RESULT;
        using obc_storage::STATUS;
        bool available = false;
        const obc_flow::JOURNAL_IO io = {
            [](void* context, const char*) -> RESULT {
                return {*static_cast<bool*>(context) ? STATUS::OK : STATUS::MOUNT_FAILED, 0, 0, 0};
            },
            [](void* context, const uint8_t*, size_t length) -> RESULT {
                if (!*static_cast<bool*>(context)) return {STATUS::WRITE_FAILED, 0, 0, 1};
                return {STATUS::OK, length, 0, 1};
            },
            [](void*) -> RESULT { return {STATUS::OK, 0, 0, 0}; },
            &available
        };
        obc_flow::Journal unavailable;
        assert(!unavailable.Init(io, completed, 0));
        assert(!unavailable.Write(bytes, size) && unavailable.GetWritten() == 0);

        available = true;
        obc_flow::Journal journal;
        assert(journal.Init(io, completed, 0));
        assert(journal.Write(bytes, size) && journal.GetWritten() == 2);
        available = false;
        assert(!journal.Write(bytes, size));
        assert(journal.GetWritten() == 2 && journal.GetFailures() == 1 && !journal.IsReady());
    }
}

int main(
) {
    using namespace obc_record;
    const obc_time::EVENT_STAMP completed = {7, 125};
    ACQUISITION_DATA data = {
        120, 2, 2, 0x44, ACQUISITION_OPERATION::READ, ACQUISITION_RESULT::OK,
        ACQUISITION_DRIVER::OK, ACQUISITION_PHASE::READ, 4, 0, 2, 0x3f,
        {3.25f, 3.5f, 10.0f, -0.125f, -0.40625f, -0.4375f}
    };
    std::array<uint8_t, eps_record_bytes> bytes = {};
    size_t size = 0;
    assert(EncodeAcquisition(ACQUISITION_KIND::ELECTRICAL, completed, data,
        bytes.data(), bytes.size(), size) == STATUS::OK);
    assert(size == bytes.size());
    std::cout.write(reinterpret_cast<const char*>(bytes.data()), size);
    CheckJournal(bytes.data(), size, completed);

    obc_flow::RecordQueue queue;
    for (size_t slot = 0; slot < obc_flow::small_record_slots; ++slot) {
        assert(queue.Push(bytes.data(), size));
    }
    assert(!queue.Push(bytes.data(), size));
    obc_flow::RECORD_BLOCK block = {};
    assert(queue.Pop(block) && block.size == size && std::memcmp(block.bytes, bytes.data(), size) == 0);
    assert(queue.Push(bytes.data(), size));

    RECORD record = {};
    size_t consumed = 0;
    assert(DecodeAcquisition(bytes.data(), size, record, consumed) == STATUS::OK);
    assert(consumed == size && record.header.stamp.boot_id == 7);
    assert(record.body.acquisition.values[3] == -0.125f);
    for (size_t count = 0; count < size; ++count) {
        assert(DecodeAcquisition(bytes.data(), count, record, consumed) == STATUS::INCOMPLETE);
        assert(consumed == 0 && record.header.stamp.boot_id == 7);
    }
    const auto intact = bytes;
    bytes[50] ^= 1;
    assert(DecodeAcquisition(bytes.data(), size, record, consumed) == STATUS::CRC_MISMATCH);
    assert(consumed == 0 && record.body.acquisition.values[3] == -0.125f);
    bytes = intact;
    data.values[0] = std::numeric_limits<float>::infinity();
    assert(EncodeAcquisition(ACQUISITION_KIND::ELECTRICAL, completed, data,
        bytes.data(), bytes.size(), size) == STATUS::INVALID_DATA);
    assert(size == 0 && bytes == intact);

    data.channel = 4;
    data.address = 0x4c;
    data.result = ACQUISITION_RESULT::QUEUE_TIMEOUT;
    data.driver = ACQUISITION_DRIVER::NOT_RUN;
    data.phase = ACQUISITION_PHASE::NONE;
    data.reg = data.received = data.available_fields = 0;
    assert(EncodeAcquisition(ACQUISITION_KIND::THERMAL, completed, data,
        bytes.data(), bytes.size(), size) == STATUS::OK);
    assert(size == thermal_record_bytes);
    std::cout.write(reinterpret_cast<const char*>(bytes.data()), size);
    assert(DecodeAcquisition(bytes.data(), size, record, consumed) == STATUS::OK);
    assert(record.body.acquisition.available_fields == 0 && record.body.acquisition.values[0] == 0);

    data.result = ACQUISITION_RESULT::OK;
    data.driver = ACQUISITION_DRIVER::OK;
    data.available_fields = 1;
    data.values[0] = 0.0f;
    assert(EncodeAcquisition(ACQUISITION_KIND::THERMAL, completed, data,
        bytes.data(), bytes.size(), size) == STATUS::OK);
    std::cout.write(reinterpret_cast<const char*>(bytes.data()), size);
    return std::cout ? 0 : 1;
}
