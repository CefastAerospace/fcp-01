#include "ObcTimeEsp32.h" // Timer ESP32 e persistência do identificador na NVS.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTimeEsp32.h".
 */

namespace obc_time {
namespace esp32 {

    namespace {

        // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

        uint64_t active_boot_id = 0;

        // ─── HELPERS ──────────────────────────────────────────────────────

        // Ausência da chave é a primeira reserva; falhas e zero persistido
        // não podem ser interpretados como uma NVS nova.
        bool ReadBootCount(
            void* context,
            uint64_t& count
        ) {
            const nvs_handle_t handle = *static_cast<nvs_handle_t*>(context);
            const esp_err_t result = nvs_get_u64(handle, "boot_count", &count);
            if (result == ESP_ERR_NVS_NOT_FOUND) {
                count = 0;
                return true;
            }
            return result == ESP_OK && count != 0;
        }

        // Só confirma sucesso depois do commit; nenhuma recuperação apaga a NVS.
        bool WriteBootCount(
            void* context,
            uint64_t count
        ) {
            const nvs_handle_t handle = *static_cast<nvs_handle_t*>(context);
            return nvs_set_u64(handle, "boot_count", count) == ESP_OK
                && nvs_commit(handle) == ESP_OK;
        }

    } // namespace

    // ─── HELPERS ──────────────────────────────────────────────────────

    uint64_t GetMonotonicMs(
    ) {
        return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
    }

    STATUS StartBoot(
        uint64_t& boot_id
    ) {
        // Repetir a inicialização neste boot não representa outro reset.
        boot_id = active_boot_id;
        if (active_boot_id != 0) {
            return STATUS::OK;
        }
        if (nvs_flash_init() != ESP_OK) {
            return STATUS::STORAGE_ERROR;
        }

        // Toda reserva usa a mesma chave; não há arquivo nem dependência do SD.
        nvs_handle_t handle;
        if (nvs_open("obc_time", NVS_READWRITE, &handle) != ESP_OK) {
            return STATUS::STORAGE_ERROR;
        }
        const BOOT_STORAGE storage = {ReadBootCount, WriteBootCount, &handle};
        const STATUS result = obc_time::StartBoot(storage, active_boot_id);
        nvs_close(handle);
        boot_id = active_boot_id;
        return result;
    }

} // namespace esp32
} // namespace obc_time
