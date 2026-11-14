/** @file ObcStorageRecoveryCheck.cpp
 * @brief Retomada real do diário em arquivos temporários, com falhas limitadas ao teste.
 */
#include "ObcDataFlow.h" // Diário, relógio da política e ciclo de vida portáteis.
#include "ObcDelivery.h" // Reconsulta sem reiniciar a fila de entregas.
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlow.h".
 */

namespace {

    struct TEST_STORAGE {
        obc_storage::Storage storage;
        char directory[64] = "/tmp/fcp01-recovery-XXXXXX";
        bool fail_next = false;
        bool partial_next = false;
        unsigned fail_append_call = 0;
        unsigned partial_append_call = 0;
        unsigned append_calls = 0;
        unsigned mount_failures = 0;
        unsigned end_calls = 0;
        unsigned init_calls = 0;
        obc_storage::STATUS end_result = obc_storage::STATUS::OK;

        TEST_STORAGE() {
            assert(mkdtemp(directory));
            assert(storage.Init(directory).status == obc_storage::STATUS::OK);
        }
        ~TEST_STORAGE() {
            storage.End();
            DIR* files = opendir(directory);
            assert(files);
            while (const dirent* entry = readdir(files)) {
                if (entry->d_name[0] == '.') continue;
                const std::string path = std::string(directory) + "/" + entry->d_name;
                assert(unlink(path.c_str()) == 0);
            }
            closedir(files);
            assert(rmdir(directory) == 0);
        }
    };

    // Usa criação exclusiva e escrita POSIX reais; injeção nunca altera arquivos anteriores.
    obc_storage::RESULT Open(
        void* context,
        const char* name
    ) { return static_cast<TEST_STORAGE*>(context)->storage.OpenNew(name); }

    obc_storage::RESULT Append(
        void* context,
        const uint8_t* bytes,
        size_t size
    ) {
        auto& test = *static_cast<TEST_STORAGE*>(context);
        ++test.append_calls;
        if (test.fail_next || test.partial_next || test.append_calls == test.fail_append_call
                || test.append_calls == test.partial_append_call) {
            const bool partial = test.partial_next || test.append_calls == test.partial_append_call;
            test.fail_next = false;
            test.partial_next = false;
            if (partial) assert(test.storage.Append(bytes, 5).status == obc_storage::STATUS::OK);
            return {obc_storage::STATUS::WRITE_FAILED, partial ? 5U : 0U, EIO, 3};
        }
        return test.storage.Append(bytes, size);
    }

    obc_storage::RESULT Close(
        void* context
    ) { return static_cast<TEST_STORAGE*>(context)->storage.Close(); }

    // BUSY não desmonta. Erros de fechamento são devolvidos após liberar recursos.
    obc_storage::RESULT End(
        void* context
    ) {
        auto& test = *static_cast<TEST_STORAGE*>(context);
        ++test.end_calls;
        if (test.end_result == obc_storage::STATUS::BUSY) return {test.end_result, 0, 0, 0};
        const auto result = test.storage.End();
        return test.end_result == obc_storage::STATUS::OK ? result
            : obc_storage::RESULT{test.end_result, 0, EIO, 0};
    }

    obc_storage::RESULT Init(
        void* context
    ) {
        auto& test = *static_cast<TEST_STORAGE*>(context);
        ++test.init_calls;
        if (test.mount_failures) {
            --test.mount_failures;
            return {obc_storage::STATUS::MOUNT_FAILED, 0, EIO, 0};
        }
        return test.storage.Init(test.directory);
    }

    // Mesmo boot e nome canônico usados pelo firmware.
    std::string Path(
        const TEST_STORAGE& test,
        unsigned segment
    ) {
        char name[80];
        std::snprintf(name, sizeof(name), "/boot_0000000000000007_%08x.bin", segment);
        return std::string(test.directory) + name;
    }

    std::vector<uint8_t> Read(
        const std::string& path
    ) {
        std::ifstream input(path, std::ios::binary);
        assert(input);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    // Registro normal válido, independente dos metadados da retomada.
    std::vector<uint8_t> NormalRecord(
    ) {
        obc_record::RECORD record = {};
        record.header = {1, obc_data::RECORD_TYPE::STATISTICS, 84, {7, 100}};
        record.body.statistics = {};
        std::vector<uint8_t> bytes(84);
        size_t size = 0;
        assert(obc_record::Encode(record, bytes.data(), bytes.size(), size) == obc_record::STATUS::OK);
        assert(size == bytes.size());
        return bytes;
    }

    // Compara bytes preservados e valida BOOT/referência pelo codec real.
    void CheckSegment(
        const TEST_STORAGE& test,
        unsigned segment,
        uint32_t unix_s
    ) {
        const auto bytes = Read(Path(test, segment));
        assert(bytes.size() >= 76);
        obc_record::RECORD record = {};
        size_t consumed = 0;
        assert(obc_record::Decode(bytes.data(), bytes.size(), record, consumed) == obc_record::STATUS::OK);
        assert(consumed == 40 && record.header.type == obc_data::RECORD_TYPE::BOOT);
        assert(record.header.stamp.boot_id == 7 && record.body.boot.restart_count == 6);
        assert(obc_record::Decode(bytes.data() + 40, bytes.size() - 40, record, consumed) == obc_record::STATUS::OK);
        assert(consumed == 36 && record.header.type == obc_data::RECORD_TYPE::TIME_REFERENCE);
        assert(record.body.time_reference.unix_s == unix_s && record.header.stamp.boot_id == 7);
    }

    // Primeira falha de montagem não consome o intervalo da próxima tentativa.
    void CheckRecovery(
    ) {
        TEST_STORAGE test;
        const obc_flow::JOURNAL_IO journal_io = {Open, Append, Close, &test};
        const obc_flow::RECOVERY_IO recovery_io = {End, Init, &test};
        obc_flow::Journal journal;
        obc_flow::StorageRecovery recovery;
        obc_flow::State state;
        state.Init(7);
        obc_time::TIME_REFERENCE reference = {};
        assert(!state.GetTimeReference(reference));
        assert(journal.Init(journal_io, {7, 10}, 0));
        assert(state.SetTime(1000, 20, 20, 0, reference));
        assert(journal.WriteReference(reference));
        const auto normal = NormalRecord();
        assert(journal.Write(normal.data(), normal.size()));
        test.fail_next = true;
        assert(!journal.Write(normal.data(), normal.size()));
        const auto previous = Read(Path(test, 0));
        const auto written = journal.GetWritten();
        test.mount_failures = 1;
        test.end_result = obc_storage::STATUS::CLOSE_FAILED;
        assert(!recovery.Service(recovery_io, journal, 100, &reference));
        assert(!recovery.Service(recovery_io, journal, 5099, &reference));
        assert(test.end_calls == 0);
        assert(!recovery.Service(recovery_io, journal, 5100, &reference));
        assert(test.end_calls == 1 && test.init_calls == 1);
        assert(recovery.GetDiagnostics().last_end.status == obc_storage::STATUS::CLOSE_FAILED);
        assert(!recovery.Service(recovery_io, journal, 9000, &reference));
        assert(!recovery.Service(recovery_io, journal, 13999, &reference));
        assert(test.end_calls == 1);
        assert(state.SetTime(2000, 9000, 9000, 0, reference));
        assert(state.GetTimeReference(reference));
        assert(recovery.Service(recovery_io, journal, 14000, &reference));
        assert(recovery.GetDiagnostics().attempts == 2 && recovery.GetDiagnostics().recovered == 0);
        assert(journal.GetWritten() == written + 2);
        assert(Read(Path(test, 0)) == previous);
        CheckSegment(test, 1, 2000);
        // Outra referência recebida durante a montagem precede os novos dados.
        assert(state.SetTime(3000, 14001, 14001, 0, reference));
        assert(journal.WriteReference(reference));
        recovery.ObserveWrite(false);
        assert(recovery.GetDiagnostics().attempts == 2);
        assert(journal.Write(normal.data(), normal.size()));
        recovery.ObserveWrite(true);
        assert(recovery.GetDiagnostics().attempts == 0 && recovery.GetDiagnostics().recovered == 1);
        test.fail_next = true;
        assert(!journal.Write(normal.data(), normal.size()));
        assert(!recovery.Service(recovery_io, journal, 20000, &reference));
        assert(recovery.Service(recovery_io, journal, 25000, &reference));
        assert(recovery.GetDiagnostics().attempts == 1 && recovery.GetDiagnostics().episodes == 2);
        CheckSegment(test, 2, 3000);
        assert(journal.Write(normal.data(), normal.size()));
        recovery.ObserveWrite(true);
        assert(recovery.GetDiagnostics().recovered == 2 && recovery.GetDiagnostics().total_attempts == 3);
        assert(journal.GetFailures() >= 2);
    }

    // Uma montagem permanente falha exatamente três vezes, inclusive após wrap u64.
    void CheckExhaustion(
    ) {
        TEST_STORAGE test;
        obc_flow::Journal journal;
        obc_flow::StorageRecovery recovery;
        const obc_flow::RECOVERY_IO io = {End, Init, &test};
        assert(journal.Init({Open, Append, Close, &test}, {7, 1}, 0));
        const auto bytes = NormalRecord();
        test.fail_next = true;
        assert(!journal.Write(bytes.data(), bytes.size()));
        test.mount_failures = 20;
        uint64_t now = UINT64_MAX - 1000;
        for (unsigned attempt = 0; attempt < 3; ++attempt) {
            assert(!recovery.Service(io, journal, now, nullptr));
            assert(!recovery.Service(io, journal, now + 4999, nullptr));
            assert(test.init_calls == attempt);
            now += 5000;
            assert(!recovery.Service(io, journal, now, nullptr));
            assert(test.init_calls == attempt + 1);
            ++now;
        }
        assert(recovery.GetDiagnostics().state == obc_flow::RECOVERY_STATE::EXHAUSTED);
        assert(recovery.GetDiagnostics().exhausted == 1);
        assert(!recovery.Service(io, journal, now + 1000000, nullptr));
        recovery.ObserveWrite(true);
        assert(recovery.GetDiagnostics().attempts == 3 && test.end_calls == 3);
        assert(!journal.IsReady());
    }

    // Metadados confirmados não renovam o orçamento quando a escrita normal falha.
    void CheckMetadataOnly(
    ) {
        TEST_STORAGE test;
        obc_flow::Journal journal;
        obc_flow::StorageRecovery recovery;
        const obc_flow::RECOVERY_IO io = {End, Init, &test};
        assert(journal.Init({Open, Append, Close, &test}, {7, 1}, 0));
        const auto bytes = NormalRecord();
        uint64_t now = 0;
        for (unsigned attempt = 0; attempt < 3; ++attempt) {
            test.fail_next = true;
            assert(!journal.Write(bytes.data(), bytes.size()));
            recovery.ObserveWrite(false);
            assert(!recovery.Service(io, journal, now, nullptr));
            now += 5000;
            assert(recovery.Service(io, journal, now, nullptr));
            assert(recovery.GetDiagnostics().attempts == attempt + 1);
            ++now;
        }
        test.fail_next = true;
        assert(!journal.Write(bytes.data(), bytes.size()));
        recovery.ObserveWrite(false);
        assert(!recovery.Service(io, journal, now, nullptr));
        assert(recovery.GetDiagnostics().state == obc_flow::RECOVERY_STATE::EXHAUSTED);
        assert(recovery.GetDiagnostics().recovered == 0 && test.init_calls == 3);
    }

    // Colisões nunca alteram o arquivo existente; cada abertura usa índice novo.
    void CheckCollisionAndBusy(
    ) {
        TEST_STORAGE test;
        obc_flow::Journal journal;
        obc_flow::StorageRecovery recovery;
        const obc_flow::RECOVERY_IO io = {End, Init, &test};
        assert(journal.Init({Open, Append, Close, &test}, {7, 1}, 0));
        { std::ofstream collision(Path(test, 1)); collision << "KEEP"; }
        const auto original = Read(Path(test, 1));
        const auto bytes = NormalRecord();
        test.fail_next = true;
        assert(!journal.Write(bytes.data(), bytes.size()));
        assert(!recovery.Service(io, journal, 0, nullptr));
        test.end_result = obc_storage::STATUS::BUSY;
        assert(!recovery.Service(io, journal, 5000, nullptr));
        assert(test.init_calls == 0);
        assert(!recovery.Service(io, journal, 5001, nullptr));
        test.end_result = obc_storage::STATUS::OK;
        assert(!recovery.Service(io, journal, 10001, nullptr));
        assert(recovery.GetDiagnostics().last_result.status == obc_storage::STATUS::ALREADY_EXISTS);
        assert(Read(Path(test, 1)) == original);
        assert(!recovery.Service(io, journal, 10002, nullptr));
        assert(recovery.Service(io, journal, 15002, nullptr));
        assert(Read(Path(test, 1)) == original && Read(Path(test, 2)).size() == 40);
        assert(recovery.GetDiagnostics().attempts == 3);
    }

    // Falha de BOOT ou referência durante Resume permanece no mesmo episódio.
    void CheckMetadataFailure(
    ) {
        for (unsigned failed_metadata = 1; failed_metadata <= 2; ++failed_metadata) {
            TEST_STORAGE test;
            obc_flow::Journal journal;
            obc_flow::StorageRecovery recovery;
            const obc_flow::RECOVERY_IO io = {End, Init, &test};
            const obc_time::TIME_REFERENCE reference = {{7, 2}, 1234, 0};
            assert(journal.Init({Open, Append, Close, &test}, {7, 1}, 0));
            const auto bytes = NormalRecord();
            test.fail_next = true;
            assert(!journal.Write(bytes.data(), bytes.size()));
            assert(!recovery.Service(io, journal, 0, &reference));
            test.fail_append_call = test.append_calls + failed_metadata;
            assert(!recovery.Service(io, journal, 5000, &reference));
            assert(!journal.IsReady() && recovery.GetDiagnostics().recovered == 0);
            const auto partial = Read(Path(test, 1));
            assert(partial.size() == (failed_metadata == 1 ? 0U : 40U));
            assert(!recovery.Service(io, journal, 5001, &reference));
            assert(recovery.Service(io, journal, 10001, &reference));
            assert(Read(Path(test, 1)) == partial);
            CheckSegment(test, 2, 1234);
        }
    }

    // Uma cauda de snapshot fica intacta; não vira registro confirmado na retomada.
    void CheckPartialSnapshot(
    ) {
        TEST_STORAGE test;
        obc_flow::Journal journal;
        obc_flow::StorageRecovery recovery;
        const obc_flow::RECOVERY_IO io = {End, Init, &test};
        assert(journal.Init({Open, Append, Close, &test}, {7, 1}, 0));
        obc_data::PAYLOAD_SNAPSHOT snapshot = {};
        snapshot.stamp = {7, 2};
        std::vector<uint8_t> bytes(obc_record::max_snapshot_record_bytes);
        size_t size = 0;
        assert(obc_record::EncodeSnapshot(snapshot, bytes.data(), bytes.size(), size) == obc_record::STATUS::OK);
        test.partial_append_call = test.append_calls + 2;
        assert(!journal.WriteParts(bytes.data(), 24, bytes.data() + 24, size - 24));
        assert(journal.GetWritten() == 1);
        const auto partial = Read(Path(test, 0));
        assert(partial.size() == 69);
        assert(!recovery.Service(io, journal, 0, nullptr));
        assert(recovery.Service(io, journal, 5000, nullptr));
        assert(Read(Path(test, 0)) == partial);
        const auto normal = NormalRecord();
        assert(journal.Write(normal.data(), normal.size()));
        recovery.ObserveWrite(true);
        assert(journal.GetWritten() == 3);
        assert(journal.GetFailures() == 1 && recovery.GetDiagnostics().recovered == 1);
    }

    // Uma reconsulta não muda ID em trânsito, confirmação ou ocupação do cache.
    void CheckDeliveryRecheck(
    ) {
        static obc_data::PAYLOAD_SNAPSHOT snapshot = {};
        snapshot.stamp = {7, 2};
        snapshot.aircraft_count = 1;
        snapshot.aircraft[0].icao = 0x123456;
        obc_delivery::Queue queue;
        queue.Init(7, false);
        queue.ObserveValidatedSnapshot(snapshot);
        obc_delivery::DELIVERY delivery = {};
        assert(queue.BeginTransmit(0, 10000, delivery));
        const auto frontier = queue.GetFrontier();
        const auto before = queue.GetStatistics();
        queue.RecheckStorage();
        assert(queue.NeedsRecovery());
        assert(queue.Restore(frontier, nullptr));
        assert(queue.GetStatistics().occupied == before.occupied);
        assert(queue.GetStatistics().accepted == before.accepted);
        assert(queue.FinishTransmit(delivery.id, true));
        assert(queue.Confirm(delivery.id));
        assert(obc_delivery::Compare(queue.GetFrontier(), delivery.id) == 0);
    }

} // namespace

int main(
) {
    CheckRecovery();
    CheckExhaustion();
    CheckMetadataOnly();
    CheckCollisionAndBusy();
    CheckMetadataFailure();
    CheckPartialSnapshot();
    CheckDeliveryRecheck();
    std::puts("Retomada SD: prazos, episodios, esgotamento, metadados, arquivos e IDs aprovados");
}
