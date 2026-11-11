#include "DeliveryLogs.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "DeliveryLogs.h".
 */

namespace {
    // Adaptadores do mesmo serviço de arquivos usado pelo firmware.
    obc_storage::RESULT List(
        void* context,
        const char* after,
        obc_storage::FILE_INFO& out
    ) { return static_cast<obc_storage::Storage*>(context)->List(after, out); }

    obc_storage::RESULT Read(
        void* context,
        const char* name,
        uint32_t offset,
        uint8_t* bytes,
        size_t capacity,
        uint32_t& total
    ) { return static_cast<obc_storage::Storage*>(context)->Read(name, offset, bytes, capacity, total); }

    // Números decimais estritos, sem sinal nem overflow.
    bool Number(
        const char* text,
        uint32_t& out
    ) {
        if (!text[0]) return false;
        uint64_t value = 0;
        for (const char* cursor = text; *cursor; ++cursor) {
            if (*cursor < '0' || *cursor > '9') return false;
            value = value * 10 + static_cast<unsigned>(*cursor - '0');
            if (value > obc_storage::max_file_bytes) return false;
        }
        out = static_cast<uint32_t>(value);
        return true;
    }

    // IDs são strings hexadecimais para conservar os 64 bits em ferramentas JSON.
    void Print(
        const obc_delivery::DELIVERY& delivery
    ) {
        const auto& aircraft = delivery.aircraft;
        std::printf("{\"boot\":\"%016llx\",\"sequence\":\"%016llx\",\"monotonic_ms\":%llu,\"icao\":\"%06lx\",\"lat\":",
            static_cast<unsigned long long>(delivery.id.boot), static_cast<unsigned long long>(delivery.id.sequence),
            static_cast<unsigned long long>(delivery.monotonic_ms), static_cast<unsigned long>(aircraft.icao));
        if (aircraft.available_fields & obc_data::SNAPSHOT_LATITUDE) std::printf("%.17g", aircraft.latitude);
        else std::printf("null");
        std::printf(",\"lon\":");
        if (aircraft.available_fields & obc_data::SNAPSHOT_LONGITUDE) std::printf("%.17g", aircraft.longitude);
        else std::printf("null");
        std::printf(",\"alt\":");
        if (aircraft.available_fields & obc_data::SNAPSHOT_ALTITUDE) std::printf("%.17g", aircraft.altitude);
        else std::printf("null");
        std::printf(",\"vel\":null,\"hdg\":null,\"ts\":null,\"count\":null}\n");
    }
}

int main(
    int argc,
    char** argv
) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: delivery-logs DIRECTORY list|recover|read NAME OFFSET LENGTH\n");
        return 2;
    }
    // Exige diretório existente para que extração nunca crie a origem por engano.
    struct stat info;
    if (::stat(argv[1], &info) != 0 || !S_ISDIR(info.st_mode)) return 2;
    obc_storage::Storage storage;
    if (storage.Init(argv[1]).status != obc_storage::STATUS::OK) return 2;
    if (!std::strcmp(argv[2], "list") && argc == 3) {
        char after[obc_storage::filename_chars + 1] = {};
        for (;;) {
            obc_storage::FILE_INFO file = {};
            if (storage.List(after, file).status != obc_storage::STATUS::OK) return 2;
            if (!file.name[0]) break;
            std::printf("%s\t%lu\t%s\n", file.name, static_cast<unsigned long>(file.size), file.active ? "active" : "closed");
            std::strcpy(after, file.name);
        }
    } else if (!std::strcmp(argv[2], "read") && argc == 6) {
        uint32_t offset = 0;
        uint32_t length = 0;
        if (!Number(argv[4], offset) || !Number(argv[5], length) || !length) return 2;
        while (length) {
            uint8_t bytes[256];
            uint32_t total = 0;
            const size_t wanted = length < sizeof(bytes) ? length : sizeof(bytes);
            const auto result = storage.Read(argv[3], offset, bytes, wanted, total);
            if (result.status != obc_storage::STATUS::OK) return 2;
            if (!result.transferred) break;
            if (std::fwrite(bytes, 1, result.transferred, stdout) != result.transferred) return 2;
            offset += static_cast<uint32_t>(result.transferred);
            length -= static_cast<uint32_t>(result.transferred);
        }
    } else if (!std::strcmp(argv[2], "recover") && argc == 3) {
        const obc_delivery::READER_IO io = {List, Read, &storage};
        obc_delivery::RECOVERY diagnostic = {};
        if (!obc_delivery::Audit(io, diagnostic)) return 2;
        obc_delivery::ID after = {};
        for (;;) {
            obc_delivery::DELIVERY delivery = {};
            bool found = false;
            if (!obc_delivery::FindPending(io, after, delivery, found)) return 2;
            if (!found) break;
            Print(delivery);
            after = delivery.id;
        }
        std::fprintf(stderr, "data=%llu confirm=%llu corrupt_bytes=%llu incomplete_tails=%llu orphan_confirmations=%llu\n",
            static_cast<unsigned long long>(diagnostic.data), static_cast<unsigned long long>(diagnostic.confirmations),
            static_cast<unsigned long long>(diagnostic.corrupt_bytes), static_cast<unsigned long long>(diagnostic.incomplete_tails),
            static_cast<unsigned long long>(diagnostic.orphan_confirmations));
        if (diagnostic.corrupt_bytes || diagnostic.incomplete_tails || diagnostic.orphan_confirmations) return 1;
    } else return 2;
    return std::fflush(stdout) == 0 ? 0 : 2;
}
