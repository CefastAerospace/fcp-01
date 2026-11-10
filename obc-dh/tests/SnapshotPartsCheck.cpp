/** @file SnapshotPartsCheck.cpp
 * @brief Vetores independentes partidos e falha na segunda escrita sem confirmar registro.
 */
#include "ObcDataFlow.h" // Codec e diário reais, incluindo escrita em duas partes.
#include <cassert>
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
    obc_data::PAYLOAD_SNAPSHOT snapshot;
    struct TEST_IO {
        obc_storage::Storage storage;
        unsigned append_calls = 0;
        bool fail_second = false;
    };

    // Backend POSIX real; somente o terceiro Append (segunda parte) pode falhar.
    obc_storage::RESULT Open(
        void* context,
        const char* name
    ) { return static_cast<TEST_IO*>(context)->storage.OpenNew(name); }

    obc_storage::RESULT Append(
        void* context,
        const uint8_t* bytes,
        size_t size
    ) {
        TEST_IO& io = *static_cast<TEST_IO*>(context);
        if (++io.append_calls == 3 && io.fail_second) return {obc_storage::STATUS::WRITE_FAILED, 0, EIO, 1};
        return io.storage.Append(bytes, size);
    }

    obc_storage::RESULT Close(
        void* context
    ) { return static_cast<TEST_IO*>(context)->storage.Close(); }

    // Compara assinatura, cabeçalho, corpo e CRC em fronteiras de campos/textos.
    void CheckEncoding(
        const std::vector<uint8_t>& golden
    ) {
        size_t consumed = 0;
        assert(obc_record::DecodeSnapshot(golden.data(), golden.size(), snapshot, consumed) == obc_record::STATUS::OK);
        const size_t cuts[] = {0, 1, 3, 4, 5, 23, 24, 32, 33, golden.size() - 4,
            golden.size() - 3, golden.size() - 1, golden.size(), 32768};
        for (size_t cut : cuts) {
            if (cut > golden.size()) continue;
            const size_t rest = golden.size() - cut;
            std::vector<uint8_t> first(cut + 2, 0xA5), second(rest + 2, 0xA5);
            size_t written = 0;
            assert(obc_record::EncodeSnapshotParts(snapshot, first.data() + 1, cut,
                second.data() + 1, rest, written) == obc_record::STATUS::OK);
            assert(written == golden.size());
            assert(std::memcmp(first.data() + 1, golden.data(), cut) == 0);
            assert(std::memcmp(second.data() + 1, golden.data() + cut, rest) == 0);
            assert(first.front() == 0xA5 && first.back() == 0xA5 && second.front() == 0xA5 && second.back() == 0xA5);
            if (rest != 0) {
                const auto old_first = first, old_second = second;
                assert(obc_record::EncodeSnapshotParts(snapshot, first.data() + 1, cut,
                    second.data() + 1, rest - 1, written) == obc_record::STATUS::BUFFER_TOO_SMALL);
                assert(written == 0 && first == old_first && second == old_second);
            }
        }
    }
}

int main(
    int argc,
    char** argv
) {
    assert(argc == 4);
    for (unsigned index = 0; index < 2; ++index) {
        std::ifstream input(std::string(argv[1]) + "/" + std::to_string(index) + ".bin", std::ios::binary);
        const std::vector<uint8_t> golden((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        assert(!golden.empty());
        CheckEncoding(golden);
    }
    // O último vetor é máximo: 64497 bytes, partido nos tamanhos do ESP.
    std::vector<uint8_t> first(obc_flow::snapshot_first_bytes), second(obc_flow::snapshot_second_bytes);
    size_t written = 0;
    assert(obc_record::EncodeSnapshotParts(snapshot, first.data(), first.size(), second.data(), second.size(), written) == obc_record::STATUS::OK);
    assert(written == 64497);
    for (unsigned fail = 0; fail < 2; ++fail) {
        TEST_IO test;
        test.fail_second = fail != 0;
        assert(test.storage.Init(argv[2 + fail]).status == obc_storage::STATUS::OK);
        const obc_flow::JOURNAL_IO io = {Open, Append, Close, &test};
        obc_flow::Journal journal;
        assert(journal.Init(io, snapshot.stamp, 0));
        const bool success = journal.WriteParts(first.data(), first.size(), second.data(), second.size());
        assert(success == (fail == 0));
        assert(journal.GetWritten() == (fail == 0 ? 2 : 1));
        assert(journal.GetFailures() == fail);
        assert(journal.IsReady() == (fail == 0));
        assert(test.storage.End().status == obc_storage::STATUS::OK);
    }
    std::puts("Snapshot partido: vetores, fronteiras, escrita completa e falha parcial aprovados");
}
