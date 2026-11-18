#include "ObcBenchSerial.h"

#ifdef OBC_BUFFERED_FRAMEWORK_LOG
// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcBenchSerial.h".
 */

extern "C" int __real_log_printfv(
    const char* format,
    va_list arguments
);

namespace {
    constexpr size_t tx_buffer_bytes = 8192;
    std::atomic<bool> uart_ready{false};

    // Uma escrita por mensagem usa o mesmo mutex e a mesma fila de Serial.
    int WriteLog(
        const char* format,
        va_list arguments
    ) {
        char local[192];
        va_list copy;
        va_copy(copy, arguments);
        const int length = std::vsnprintf(local, sizeof(local), format, copy);
        va_end(copy);
        if (length <= 0) return length;
        const auto size = static_cast<size_t>(length);
        if (size < sizeof(local)) {
            return static_cast<int>(Serial.write(reinterpret_cast<const uint8_t*>(local), size));
        }
        auto* buffer = static_cast<char*>(std::malloc(size + 1));
        if (!buffer) return 0;
        const int formatted = std::vsnprintf(buffer, size + 1, format, arguments);
        const int written = formatted == length
            ? static_cast<int>(Serial.write(reinterpret_cast<const uint8_t*>(buffer), size)) : 0;
        std::free(buffer);
        return written;
    }
}

namespace obc_bench_serial {

bool Init(
) {
    if (Serial.setTxBufferSize(tx_buffer_bytes) != tx_buffer_bytes) return false;
    Serial.begin(115200);
    const bool ready = static_cast<bool>(Serial);
    uart_ready.store(ready, std::memory_order_release);
    return ready;
}

} // namespace obc_bench_serial

// Preserva o wrapper de diagnóstico do SDK e redireciona sua chamada a log_printfv.
extern "C" int __wrap_log_printfv(
    const char* format,
    va_list arguments
) {
    if (!uart_ready.load(std::memory_order_acquire)) return __real_log_printfv(format, arguments);
    return WriteLog(format, arguments);
}
#endif
