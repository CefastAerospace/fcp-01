/**
 * @file PayloadReceiverCheck.cpp
 * @brief Exercita o receptor real com quadros Python struct/zlib independentes.
 * @details Compilar e executar conforme examples/README.md. Sem hardware.
 */

#include "PayloadReceiver.h" // Receiver e contrato binário do transporte.
#include <cassert> // Asserts de invariantes e resultados independentes.
#include <cstdio> // Relatório de conclusão e orçamento em memória.
#include <fstream> // Leitura binária dos vetores temporários.
#include <iterator> // istreambuf_iterator.
#include <string>
#include <vector>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "PayloadReceiver.h".
 */

namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    obc_payload::Receiver receiver;
    obc_data::PAYLOAD_SNAPSHOT saved;
    std::string vector_directory;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Lê vetores independentes, sem gerar CRC com o código sob teste.
    std::vector<uint8_t> ReadVector(
        const char* name
    ) {
        std::ifstream file(vector_directory + "/" + name, std::ios::binary);
        assert(file.good());
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    }

    // Alimenta o mesmo fluxo com fragmentação arbitrária e respeita consumed.
    obc_payload::RESULT FeedVector(
        const std::vector<uint8_t>& bytes,
        size_t fragment = 256,
        uint64_t now_ms = 5000
    ) {
        using namespace obc_payload;
        RESULT last = {};
        last.status = STATUS::WAITING;
        size_t offset = 0;
        while (offset < bytes.size()) {
            const size_t remaining = bytes.size() - offset;
            const size_t count = remaining < fragment ? remaining : fragment;
            const RESULT result = receiver.Feed(bytes.data() + offset, count, now_ms);
            assert(result.consumed <= count);
            assert(result.consumed != 0 || result.status != STATUS::WAITING);
            offset += result.consumed;
            if (result.status != STATUS::WAITING) last = result;
        }
        return last;
    }

    // Busca aeronaves por identidade, sem depender da ordem do JSON.
    const obc_data::AIRCRAFT_STATE& FindAircraft(
        uint32_t icao
    ) {
        const auto* snapshot = receiver.GetSnapshot();
        assert(snapshot != nullptr);
        for (size_t index = 0; index < snapshot->aircraft_count; ++index) {
            if (snapshot->aircraft[index].icao == icao) return snapshot->aircraft[index];
        }
        assert(false);
        return snapshot->aircraft[0];
    }

    // Confere respostas completas, inclusive cabeçalho e CRC externos.
    void CheckReply(
        const obc_payload::RESULT& result,
        const char* expected
    ) {
        const auto bytes = ReadVector(expected);
        assert(result.reply_size == bytes.size());
        assert(std::memcmp(result.reply, bytes.data(), bytes.size()) == 0);
    }

    // Confere amostra, id de 64 bits, campos opcionais e preservação de extras.
    void CheckContent(
    ) {
        using namespace obc_payload;
        assert(receiver.Init(7));
        const auto sample = ReadVector("sample");
        const RESULT result = FeedVector(sample, 1);
        assert(result.status == STATUS::READY);
        CheckReply(result, "expected_ack");
        const auto* snapshot = receiver.GetSnapshot();
        assert(snapshot->id == 480 && snapshot->aircraft_count == 7);
        assert(snapshot->stamp.boot_id == 7 && snapshot->stamp.monotonic_ms == 5000);
        assert(obc_data::Validate(*snapshot) == obc_data::STATUS::OK);
        const auto& decoded = FindAircraft(0xE49A48);
        assert(std::strcmp(decoded.identification, "GLO1669") == 0);
        assert(std::fabs(decoded.latitude + 20.109775) < 1e-9);
        assert(decoded.altitude == 38000);
        const auto& undecoded = FindAircraft(0xE495B2);
        assert((undecoded.available_fields & (obc_data::SNAPSHOT_LATITUDE | obc_data::SNAPSHOT_LONGITUDE)) == 0);

        for (const char* name : {"valid_fields", "undecoded", "one_coordinate", "null_fields", "bad_numbers", "max_aircraft", "max_fields", "max_wire", "max_id"}) {
            assert(receiver.Init(7));
            assert(FeedVector(ReadVector(name)).status == STATUS::READY);
            assert(obc_data::Validate(*receiver.GetSnapshot()) == obc_data::STATUS::OK);
            if (std::strcmp(name, "valid_fields") == 0) {
                const auto& aircraft = FindAircraft(0xABCDEF);
                assert(aircraft.latitude == 0 && aircraft.longitude == -180 && aircraft.altitude == -10);
                assert(aircraft.identification[0] == '\0' && aircraft.extra_count == 2);
                assert(std::strcmp(aircraft.extras[1].value, "line\nquote\"slash\\") == 0);
            } else if (std::strcmp(name, "one_coordinate") == 0) {
                const auto& aircraft = FindAircraft(1);
                assert(aircraft.available_fields == 0 && aircraft.extra_count == 1);
                assert(std::strcmp(aircraft.extras[0].value, "12.5") == 0);
            } else if (std::strcmp(name, "bad_numbers") == 0) {
                assert(FindAircraft(1).available_fields == 0 && FindAircraft(1).extra_count == 3);
            } else if (std::strcmp(name, "null_fields") == 0) {
                assert(FindAircraft(1).available_fields == 0 && FindAircraft(1).extra_count == 0);
            } else if (std::strcmp(name, "max_id") == 0) {
                assert(receiver.GetSnapshot()->id == UINT64_MAX);
            } else if (std::strcmp(name, "max_fields") == 0) {
                assert(FindAircraft(1).extra_count == 32);
            }
            assert(receiver.GetStatistics().parser_peak_bytes <= parser_memory_bytes);
        }

        assert(receiver.Init(7));
        assert(FeedVector(sample).status == STATUS::READY);
        assert(FeedVector(ReadVector("empty2")).status == STATUS::READY);
        assert(receiver.GetSnapshot()->aircraft_count == 0 && receiver.GetSnapshot()->id == 0);
    }

    // Cada entrada inválida encontra uma coleção prévia real de sete aeronaves.
    // Nenhum erro pode publicar parcialmente ou alterar sequer os bytes antigos.
    void CheckRejections(
    ) {
        using namespace obc_payload;
        struct CASE {
            const char* name;
            STATUS status;
        };
        const CASE cases[] = {
            {"overflow_id", STATUS::INVALID_VALUE}, {"negative_id", STATUS::INVALID_VALUE},
            {"float_id", STATUS::INVALID_VALUE}, {"missing_id", STATUS::INVALID_VALUE},
            {"wrong_messages", STATUS::INVALID_VALUE}, {"extra_root", STATUS::INVALID_VALUE},
            {"bad_icao", STATUS::INVALID_VALUE}, {"bad_icao_attribute", STATUS::INVALID_VALUE},
            {"duplicate_icao", STATUS::INVALID_VALUE}, {"wrong_extra", STATUS::INVALID_VALUE},
            {"null_extra", STATUS::INVALID_VALUE}, {"object_extra", STATUS::INVALID_VALUE},
            {"nul_extra", STATUS::INVALID_VALUE}, {"nul_key", STATUS::INVALID_VALUE},
            {"empty_key", STATUS::INVALID_VALUE}, {"field_type", STATUS::INVALID_VALUE},
            {"key_overflow", STATUS::LIMIT_EXCEEDED}, {"value_overflow", STATUS::LIMIT_EXCEEDED},
            {"identification_overflow", STATUS::LIMIT_EXCEEDED}, {"tick_overflow", STATUS::LIMIT_EXCEEDED},
            {"extras_overflow", STATUS::LIMIT_EXCEEDED}, {"aircraft_overflow", STATUS::LIMIT_EXCEEDED},
            {"no_memory", STATUS::NO_MEMORY}, {"trailing", STATUS::INVALID_JSON},
            {"single_quotes", STATUS::INVALID_JSON}, {"bare_key", STATUS::INVALID_JSON},
            {"leading_zero", STATUS::INVALID_JSON}, {"trailing_comma", STATUS::INVALID_JSON},
            {"broken_json", STATUS::INVALID_JSON}, {"raw_control", STATUS::INVALID_JSON},
            {"two_objects", STATUS::INVALID_JSON}, {"deep", STATUS::INVALID_JSON},
        };
        for (const auto& test : cases) {
            assert(receiver.Init(7));
            assert(FeedVector(ReadVector("prior")).status == STATUS::READY);
            std::memcpy(&saved, receiver.GetSnapshot(), sizeof(saved));
            const RESULT result = FeedVector(ReadVector(test.name));
            if (result.status != test.status) {
                std::fprintf(stderr, "%s: status=%u; expected=%u\n", test.name,
                    static_cast<unsigned>(result.status), static_cast<unsigned>(test.status));
            }
            assert(result.status == test.status && result.reply_size == 31);
            assert(result.reply[5] == static_cast<uint8_t>(FRAME_TYPE::NACK));
            assert(std::memcmp(&saved, receiver.GetSnapshot(), sizeof(saved)) == 0);
            assert(receiver.GetStatistics().accepted == 1);
            assert(receiver.GetStatistics().parser_peak_bytes <= parser_memory_bytes);
        }
    }

    // ACK perdido repete o quadro, não o snapshot; gaps não avançam a sequência.
    void CheckSequence(
    ) {
        using namespace obc_payload;
        assert(receiver.Init(7));
        const auto sample = ReadVector("sample");
        assert(FeedVector(sample).status == STATUS::READY);
        const RESULT duplicate = FeedVector(sample, 17, 6000);
        assert(duplicate.status == STATUS::DUPLICATE);
        CheckReply(duplicate, "expected_ack");
        assert(receiver.GetSnapshot()->stamp.monotonic_ms == 5000);
        assert(receiver.GetStatistics().accepted == 1 && receiver.GetStatistics().duplicates == 1);
        assert(FeedVector(ReadVector("same_tuple_changed"), 256, 6000).status == STATUS::SEQUENCE_ERROR);
        const RESULT gap = FeedVector(ReadVector("gap"), 256, 6000);
        assert(gap.status == STATUS::SEQUENCE_ERROR);
        CheckReply(gap, "expected_nack");
        assert(receiver.GetStatistics().gaps == 1);
        assert(FeedVector(ReadVector("empty2"), 256, 6000).status == STATUS::READY);
        assert(FeedVector(ReadVector("empty3"), 256, 6000).status == STATUS::READY);
        assert(FeedVector(ReadVector("restart_bad"), 256, 6000).status == STATUS::SEQUENCE_ERROR);
        assert(FeedVector(ReadVector("restart"), 256, 6000).status == STATUS::READY);
        assert(receiver.GetStatistics().session_changes == 1);

        assert(receiver.Init(8));
        assert(receiver.GetSnapshot() == nullptr);
        assert(FeedVector(ReadVector("baseline")).status == STATUS::READY);
        assert(receiver.GetSnapshot()->stamp.boot_id == 8);
        assert(receiver.Init(8));
        assert(FeedVector(ReadVector("max_sequence")).status == STATUS::READY);
        assert(FeedVector(ReadVector("max_sequence")).status == STATUS::DUPLICATE);
        assert(FeedVector(ReadVector("empty2")).status == STATUS::SEQUENCE_ERROR);
        assert(FeedVector(ReadVector("restart")).status == STATUS::READY);
    }

    // Tempo simulado verifica as fronteiras exatas sem sleeps ou hardware.
    void CheckTimeouts(
    ) {
        using namespace obc_payload;
        assert(receiver.Init(7));
        const auto sample = ReadVector("sample");
        assert(receiver.Feed(sample.data(), 17, 1000).status == STATUS::WAITING);
        assert(receiver.CheckTimeout(1249) == STATUS::WAITING);
        assert(receiver.CheckTimeout(1250) == STATUS::TIMEOUT);
        assert(FeedVector(sample, 256, 1250).status == STATUS::READY);
        std::memcpy(&saved, receiver.GetSnapshot(), sizeof(saved));
        assert(receiver.Feed(sample.data(), 10, 1300).status == STATUS::WAITING);
        assert(receiver.CheckTimeout(1550) == STATUS::TIMEOUT);
        assert(std::memcmp(&saved, receiver.GetSnapshot(), sizeof(saved)) == 0);
        assert(receiver.Feed(nullptr, 1, 1550).status == STATUS::INVALID_ARGUMENT);
        assert(receiver.CheckTimeout(1549) == STATUS::INVALID_ARGUMENT);
        assert(!receiver.Init(0));
        assert(std::memcmp(&saved, receiver.GetSnapshot(), sizeof(saved)) == 0);

        assert(receiver.Init(7));
        for (size_t index = 0; index < 40; ++index) {
            assert(receiver.Feed(sample.data() + index, 1, index * 200).status == STATUS::WAITING);
        }
        assert(receiver.CheckTimeout(7999) == STATUS::WAITING);
        assert(receiver.CheckTimeout(8000) == STATUS::TIMEOUT);
        assert(FeedVector(sample, 256, 8000).status == STATUS::READY);
    }

    // Recupera assinatura após ruído/CRC inválido e candidatos já armazenados.
    void CheckFraming(
    ) {
        using namespace obc_payload;
        for (const char* name : {"wrong_version", "wrong_type", "zero_sequence", "zero_session", "bad_crc", "oversize_header"}) {
            assert(receiver.Init(7));
            const RESULT invalid = FeedVector(ReadVector(name));
            assert(invalid.status != STATUS::READY && invalid.reply_size == 0);
            assert(receiver.GetSnapshot() == nullptr);
            assert(FeedVector(ReadVector("sample")).status == STATUS::READY);
        }
        assert(receiver.Init(7));
        std::vector<uint8_t> stream = {'n', 'o', 'i', 's', 'e', 'F', 'C'};
        const auto first = ReadVector("sample");
        const auto second = ReadVector("empty2");
        stream.insert(stream.end(), first.begin(), first.end());
        stream.insert(stream.end(), second.begin(), second.end());
        assert(FeedVector(stream, stream.size()).status == STATUS::READY);
        assert(receiver.GetStatistics().accepted == 2 && receiver.GetSnapshot()->aircraft_count == 0);

        assert(receiver.Init(7));
        assert(FeedVector(ReadVector("nested_bad_crc")).status == STATUS::INVALID_FRAME);
        const RESULT retained = receiver.Feed(nullptr, 0, 5000);
        assert(retained.status == STATUS::READY && retained.consumed == 0);
        assert(receiver.GetSnapshot()->aircraft_count == 7);
    }

} // namespace

int main(
    int argc,
    char** argv
) {
    assert(argc == 2);
    vector_directory = argv[1];
    assert(receiver.Feed(nullptr, 0, 0).status == obc_payload::STATUS::NOT_INITIALIZED);
    CheckContent();
    CheckRejections();
    CheckSequence();
    CheckTimeouts();
    CheckFraming();
    std::printf("PayloadReceiver: PASS; sizeof(Receiver)=%zu; parser_budget=%zu\n",
        sizeof(receiver), obc_payload::parser_memory_bytes);
}
