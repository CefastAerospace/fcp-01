#include "ObcStorageEsp32.h" // SD, HSPI, mutex estático e núcleo de arquivos.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStorageEsp32.h".
 */

namespace obc_storage {
namespace esp32 {

    namespace {

        // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

        SPIClass sd_spi(HSPI);
        Storage storage;
        StaticSemaphore_t service_mutex_storage;
        SemaphoreHandle_t service_mutex = nullptr;
        bool mounted = false;

        // ─── HELPERS ──────────────────────────────────────────────────────

        // Não espera nem permite uso em ISR. A criação do mutex pertence
        // apenas à primeira inicialização, antes das tarefas consumidoras.
        STATUS LockService(
        ) {
            if (xPortInIsrContext()) return STATUS::INVALID_CONTEXT;
            if (service_mutex == nullptr) return STATUS::NOT_INITIALIZED;
            return xSemaphoreTake(service_mutex, 0) == pdTRUE ? STATUS::OK : STATUS::BUSY;
        }

    } // namespace

    // ─── HELPERS ──────────────────────────────────────────────────────

    RESULT Init(
    ) {
        if (xPortInIsrContext()) return {STATUS::INVALID_CONTEXT, 0, 0, 0};
        if (service_mutex == nullptr) {
            service_mutex = xSemaphoreCreateMutexStatic(&service_mutex_storage);
            if (service_mutex == nullptr) return {STATUS::INIT_FAILED, 0, 0, 0};
        }
        const STATUS lock = LockService();
        if (lock != STATUS::OK) return {lock, 0, 0, 0};
        if (mounted) {
            xSemaphoreGive(service_mutex);
            return {STATUS::OK, 0, 0, 0};
        }

        // SD.begin reutiliza os pinos já configurados na instância HSPI.
        sd_spi.begin(pinout::sd_sck, pinout::sd_miso, pinout::sd_mosi, pinout::sd_cs);
        if (!SD.begin(pinout::sd_cs, sd_spi, sd_clock_hz, "/sd", 5, false)) {
            sd_spi.end();
            xSemaphoreGive(service_mutex);
            return {STATUS::MOUNT_FAILED, 0, 0, 0};
        }
        const RESULT result = storage.Init("/sd/obc");
        if (result.status == STATUS::OK) {
            mounted = true;
        } else {
            SD.end();
            sd_spi.end();
        }
        xSemaphoreGive(service_mutex);
        return result;
    }

    RESULT OpenNew(
        const char* name
    ) {
        const STATUS lock = LockService();
        if (lock != STATUS::OK) return {lock, 0, 0, 0};
        const RESULT result = storage.OpenNew(name);
        xSemaphoreGive(service_mutex);
        return result;
    }

    RESULT Append(
        const uint8_t* data,
        size_t size
    ) {
        const STATUS lock = LockService();
        if (lock != STATUS::OK) return {lock, 0, 0, 0};
        const RESULT result = storage.Append(data, size);
        xSemaphoreGive(service_mutex);
        return result;
    }

    RESULT Read(
        const char* name,
        uint32_t offset,
        uint8_t* buffer,
        size_t capacity,
        uint32_t& total_size
    ) {
        total_size = 0;
        const STATUS lock = LockService();
        if (lock != STATUS::OK) return {lock, 0, 0, 0};
        const RESULT result = storage.Read(name, offset, buffer, capacity, total_size);
        xSemaphoreGive(service_mutex);
        return result;
    }

    RESULT Close(
    ) {
        const STATUS lock = LockService();
        if (lock != STATUS::OK) return {lock, 0, 0, 0};
        const RESULT result = storage.Close();
        xSemaphoreGive(service_mutex);
        return result;
    }

    RESULT End(
    ) {
        const STATUS lock = LockService();
        if (lock != STATUS::OK) return {lock, 0, 0, 0};
        const RESULT result = storage.End();
        if (mounted) {
            SD.end();
            sd_spi.end();
            mounted = false;
        }
        // Mantém o mutex para não invalidar referências de outros chamadores.
        xSemaphoreGive(service_mutex);
        return result;
    }

} // namespace esp32
} // namespace obc_storage
