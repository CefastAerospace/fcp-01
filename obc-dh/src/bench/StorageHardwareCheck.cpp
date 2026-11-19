#include "StorageHardwareCheck.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "StorageHardwareCheck.h".
 */

namespace {
    using obc_storage::RESULT;
    using obc_storage::STATUS;
    namespace sd = obc_storage::esp32;

    constexpr uint8_t fixture[] = {'A', 'B', 'C', 'D', 'E', 'F'};
    constexpr size_t name_bytes = 12;
    constexpr size_t block_bytes = 4096;
    constexpr unsigned race_rounds = 20;
    constexpr TickType_t race_stagger_ticks = 2;
    constexpr unsigned full_blocks = 17;
    uint8_t block[block_bytes];

    struct MEASUREMENT {
        RESULT result;
        uint32_t elapsed_us;
    };

    struct RACE {
        TaskHandle_t owner;
        const char* path;
        MEASUREMENT reads[race_rounds];
        bool matches[race_rounds];
    };

    RACE race;

    // Mede o adaptador sem incluir o tempo de transmissão dos diagnósticos.
    template <typename Operation>
    MEASUREMENT Measure(
        Operation operation
    ) {
        const int64_t started_us = esp_timer_get_time();
        const RESULT result = operation();
        return {result, static_cast<uint32_t>(esp_timer_get_time() - started_us)};
    }

    // Publica o retorno bruto, o tempo e a comparação feita para aquele caso.
    bool Report(
        const char* name,
        const MEASUREMENT& sample,
        bool passed,
        unsigned round = 0
    ) {
        const auto& result = sample.result;
        Serial.printf("[TES006] op=%s round=%u status=%u bytes=%u errno=%d attempts=%u us=%lu pass=%u\n",
            name, round, static_cast<unsigned>(result.status), static_cast<unsigned>(result.transferred),
            result.system_error, static_cast<unsigned>(result.attempts),
            static_cast<unsigned long>(sample.elapsed_us), passed ? 1U : 0U);
        return passed;
    }

    // Só OK com o tamanho completo conta como bloco gravado ou lido.
    bool Check(
        const char* name,
        const MEASUREMENT& sample,
        STATUS expected,
        size_t wanted = 0
    ) {
        return Report(name, sample, sample.result.status == expected && sample.result.transferred == wanted);
    }

    // Nomes curtos para ocupar uma entrada FAT e identificar o cenário.
    void MakeName(
        char (&name)[name_bytes],
        const char* id,
        char suffix
    ) {
        std::snprintf(name, sizeof(name), "t6%s%c.bin", id, suffix);
        Serial.printf("[TES006] file=%s\n", name);
    }

    // Publica somente os bytes do vetor de teste, já comparados pelo adaptador.
    bool CheckRead(
        const char* label,
        const char* name,
        uint32_t offset,
        size_t capacity,
        STATUS status,
        size_t wanted
    ) {
        uint8_t bytes[sizeof(fixture)] = {};
        uint32_t total = 0;
        const auto sample = Measure([&] { return sd::Read(name, offset, bytes, capacity, total); });
        const bool matches = !wanted || std::memcmp(bytes, fixture + offset, wanted) == 0;
        Serial.printf("[TES006-DATA] op=%s offset=%lu total=%lu hex=", label,
            static_cast<unsigned long>(offset), static_cast<unsigned long>(total));
        if (sample.result.status == STATUS::OK) {
            for (size_t index = 0; index < sample.result.transferred && index < capacity; ++index) {
                Serial.printf("%02x", static_cast<unsigned>(bytes[index]));
            }
        }
        Serial.println();
        return Report(label, sample, sample.result.status == status && sample.result.transferred == wanted
            && matches && (status == STATUS::BUSY || total == sizeof(fixture)));
    }

    // Leitor concorrente em Core 0. O loop Arduino escreve pelo mesmo adaptador.
    void ReadContender(
        void*
    ) {
        for (unsigned index = 0; index < race_rounds; ++index) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (index % 2 != 0) vTaskDelay(race_stagger_ticks);

            uint8_t bytes[sizeof(fixture)] = {};
            uint32_t total = 0;
            auto& sample = race.reads[index];
            sample = Measure([&] { return sd::Read(race.path, 0, bytes, sizeof(bytes), total); });
            race.matches[index] = sample.result.status == STATUS::OK && sample.result.transferred == sizeof(bytes)
                && total == sizeof(bytes) && std::memcmp(bytes, fixture, sizeof(bytes)) == 0;
            xTaskNotifyGive(race.owner);
        }
        vTaskDelete(nullptr);
    }

    // Confere também o arquivo concorrente, contando só os appends confirmados.
    bool CheckRace(
        const char* nominal,
        const char* id
    ) {
        char name[name_bytes];
        MakeName(name, id, 'c');
        if (!Check("race_open", Measure([&] { return sd::OpenNew(name); }), STATUS::OK)) return false;
        std::memset(block, 0x5a, sizeof(block));
        race.owner = xTaskGetCurrentTaskHandle();
        race.path = nominal;
        TaskHandle_t reader = nullptr;
        if (xTaskCreatePinnedToCore(ReadContender, "sd_race_read", 4096, nullptr, 1, &reader, 0) != pdPASS) {
            Serial.println("[TES006] race=task_unavailable");
            return false;
        }

        unsigned busy = 0;
        unsigned writes = 0;
        unsigned reads = 0;
        bool passed = true;
        MEASUREMENT samples[race_rounds];
        // Alterna a precedência sem serializar as chamadas ao driver.
        for (unsigned index = 0; index < race_rounds; ++index) {
            xTaskNotifyGive(reader);
            if (index % 2 == 0) vTaskDelay(race_stagger_ticks);
            samples[index] = Measure([] { return sd::Append(block, sizeof(block)); });
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        for (unsigned index = 0; index < race_rounds; ++index) {
            const auto& write = samples[index];
            const auto& read = race.reads[index];
            const bool wrote = write.result.status == STATUS::OK && write.result.transferred == sizeof(block);
            const bool write_busy = write.result.status == STATUS::BUSY;
            const bool read_busy = read.result.status == STATUS::BUSY;
            writes += wrote;
            reads += race.matches[index];
            busy += write_busy + read_busy;
            passed &= Report("race_append", write, wrote || write_busy, index);
            passed &= Report("race_read", read, race.matches[index] || read_busy, index);
        }
        if (!Check("race_close", Measure(sd::Close), STATUS::OK)) return false;
        for (unsigned index = 0; index < writes; ++index) {
            uint32_t total = 0;
            const auto sample = Measure([&] { return sd::Read(name, index * block_bytes, block, block_bytes, total); });
            bool matches = sample.result.status == STATUS::OK && sample.result.transferred == block_bytes
                && total == writes * block_bytes;
            for (uint8_t byte : block) matches = matches && byte == 0x5a;
            passed &= Report("race_verify", sample, matches, index);
        }
        Serial.printf("[TES006] busy=%u writes=%u reads=%u\n", busy, writes, reads);
        return passed && busy && writes && reads;
    }

    // Exercita o vetor nominal usando nomes novos em cada rodada de bancada.
    bool RunNominal(
        const char* id,
        bool recovered
    ) {
        char name[name_bytes];
        MakeName(name, id, recovered ? 'p' : 'n');
        if (!Check("open", Measure([&] { return sd::OpenNew(name); }), STATUS::OK)
                || !Check("append_abc", Measure([] { return sd::Append(fixture, 3); }), STATUS::OK, 3)
                || !Check("append_def", Measure([] { return sd::Append(fixture + 3, 3); }), STATUS::OK, 3)
                || !CheckRead("read_open", name, 0, sizeof(fixture), STATUS::BUSY, 0)
                || !Check("close", Measure(sd::Close), STATUS::OK)
                || !Check("duplicate_name", Measure([&] { return sd::OpenNew(name); }), STATUS::ALREADY_EXISTS)
                || !CheckRead("read_ab", name, 0, 2, STATUS::OK, 2)
                || !CheckRead("read_cd", name, 2, 2, STATUS::OK, 2)
                || !CheckRead("read_ef", name, 4, 2, STATUS::OK, 2)
                || !CheckRead("read_eof", name, 6, sizeof(fixture), STATUS::OK, 0)
                || !CheckRead("read_after_eof", name, 7, sizeof(fixture), STATUS::INVALID_ARGUMENT, 0)) return false;
        return recovered || (CheckRace(name, id) && CheckRead("read_nominal", name, 0, sizeof(fixture), STATUS::OK, 6));
    }

    // Preserva o prefixo confirmado e testa a retomada no mesmo boot, sem remover o cartão.
    bool RunFull(
        const char* id
    ) {
        char name[name_bytes];
        MakeName(name, id, 'f');
        if (!Check("full_open", Measure([&] { return sd::OpenNew(name); }), STATUS::OK)
                || !Check("full_prefix", Measure([] { return sd::Append(fixture, 3); }), STATUS::OK, 3)) return false;

        for (unsigned index = 0; index < full_blocks; ++index) {
            std::memset(block, '0' + index, sizeof(block));
            const auto sample = Measure([] { return sd::Append(block, sizeof(block)); });
            const auto& result = sample.result;
            if (result.status == STATUS::OK) {
                if (!Report("full_append", sample, result.transferred == sizeof(block), index)) return false;
                continue;
            }
            const bool failure = result.status == STATUS::NO_SPACE || result.status == STATUS::WRITE_FAILED
                || result.status == STATUS::SYNC_FAILED;
            if (!Report("full_terminal", sample, failure && result.attempts == obc_storage::write_attempts, index)) return false;

            char fresh[name_bytes];
            MakeName(fresh, id, 'r');
            return Check("full_rejected", Measure([] { return sd::Append(fixture, 3); }), STATUS::WRITER_FAILED)
                && Check("full_init_again", Measure(sd::Init), STATUS::OK)
                && Check("full_still_rejected", Measure([] { return sd::Append(fixture, 3); }), STATUS::WRITER_FAILED)
                && Check("full_failed_end", Measure(sd::End), STATUS::WRITER_FAILED)
                && Check("full_remount", Measure(sd::Init), STATUS::OK)
                && Check("full_fresh_name", Measure([&] { return sd::OpenNew(fresh); }), STATUS::OK)
                && Check("full_fresh_close", Measure(sd::Close), STATUS::OK);
        }
        Serial.println("[TES006] full=space_not_exhausted");
        return false;
    }

    // Despacha só os cenários fixos e sempre encerra o serviço antes de trocar o cartão.
    bool Run(
        char scenario,
        const char* id
    ) {
        const STATUS expected = scenario == 'A' ? STATUS::MOUNT_FAILED : STATUS::OK;
        bool passed = Check("init", Measure(sd::Init), expected);
        if (passed && scenario != 'A') {
            passed = scenario == 'F' ? RunFull(id) : RunNominal(id, scenario == 'R');
        }
        const auto end = Measure(sd::End);
        return Check("end", end, STATUS::OK) && passed;
    }
} // namespace

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(100);
    Serial.printf("[TES006] ready sck=%d miso=%d mosi=%d cs=%d hz=%lu\n",
        pinout::sd_sck, pinout::sd_miso, pinout::sd_mosi, pinout::sd_cs,
        static_cast<unsigned long>(sd::sd_clock_hz));
}

void loop() {
    if (!Serial.available()) {
        delay(10);
        return;
    }
    char command[16] = {};
    const size_t length = Serial.readBytesUntil('\n', command, sizeof(command) - 1);
    const char scenario = command[0];
    const char* id = command + 2;
    if (length != 6 || command[1] != ' ' || std::strchr("NAFR", scenario) == nullptr
            || std::strspn(id, "0123456789abcdef") != 4) {
        Serial.println("[TES006] bad_command");
        return;
    }
    Serial.printf("[TES006] start case=%c id=%s\n", scenario, id);
    const bool passed = Run(scenario, id);
    Serial.printf("[TES006] done case=%c id=%s passed=%u\n", scenario, id, passed ? 1U : 0U);
}
