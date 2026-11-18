/** @file SnapshotBuffersCheck.cpp
 * @brief Tamanhos reais, divisão, falhas de cada alocação e codec usando o estágio real.
 */
#include "ObcDataFlow.h" // SnapshotBuffers e codec usados pelo fluxo ESP32.
#include <cassert>
#include <cstdio>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlow.h".
 */

namespace {
    size_t allocation_calls = 0;
    size_t allocation_sizes[2] = {};
    size_t fail_at = 0;

    // Injeção exclusiva deste executável; nenhuma flag ou allocator de produção muda.
    void ResetFaults(
        size_t failure = 0
    ) {
        allocation_calls = 0;
        allocation_sizes[0] = allocation_sizes[1] = 0;
        fail_at = failure;
    }
} // namespace

// GNU ld intercepta somente operator new[](size_t, nothrow) no nativo Linux.
extern "C" void* __real__ZnamRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t& tag
) noexcept;

// Mantém o allocator real/ASan, recusando apenas a chamada selecionada.
extern "C" void* __wrap__ZnamRKSt9nothrow_t(
    size_t size,
    const std::nothrow_t& tag
) noexcept {
    if (allocation_calls < 2) allocation_sizes[allocation_calls] = size;
    ++allocation_calls;
    if (allocation_calls == fail_at) return nullptr;
    return __real__ZnamRKSt9nothrow_t(size, tag);
}

int main(
) {
    obc_flow::SnapshotBuffers buffers;
    assert(buffers.GetFirstData() == nullptr && buffers.GetSecondData() == nullptr);
    const size_t sizes[] = {37, 32768, 32769, 64497};
    const size_t first_sizes[] = {37, 32768, 32768, 32768};
    const size_t second_sizes[] = {0, 0, 1, 31729};
    for (size_t index = 0; index < 4; ++index) {
        ResetFaults();
        assert(buffers.Reserve(sizes[index]));
        assert(allocation_calls == (second_sizes[index] == 0 ? 1U : 2U));
        assert(allocation_sizes[0] == first_sizes[index] && allocation_sizes[1] == second_sizes[index]);
        buffers.GetFirstData()[first_sizes[index] - 1] = 0xA5;
        if (second_sizes[index] != 0) buffers.GetSecondData()[second_sizes[index] - 1] = 0x5A;
        else assert(buffers.GetSecondData() == nullptr);
        buffers.Reset();
        assert(buffers.GetFirstData() == nullptr && buffers.GetSecondData() == nullptr);
    }

    // Entrada inválida não aloca nem destrói o conteúdo existente.
    ResetFaults();
    assert(buffers.Reserve(37));
    auto* first = buffers.GetFirstData();
    first[36] = 0xA5;
    const size_t invalid[] = {0, 36, 64498, std::numeric_limits<size_t>::max()};
    for (size_t size : invalid) {
        ResetFaults();
        assert(!buffers.Reserve(size) && allocation_calls == 0);
        assert(buffers.GetFirstData() == first && first[36] == 0xA5);
    }
    buffers.Reset();

    // Cada ponto de falha libera a reserva parcial e permite retomada posterior.
    for (size_t failure = 1; failure <= 2; ++failure) {
        ResetFaults(failure);
        assert(!buffers.Reserve(64497));
        assert(allocation_calls == failure);
        assert(buffers.GetFirstData() == nullptr && buffers.GetSecondData() == nullptr);
        ResetFaults();
        assert(buffers.Reserve(37));
        buffers.Reset();
    }

    // Registro vazio real usa 37 bytes e segundo bloco ausente, sem representação paralela.
    std::unique_ptr<obc_data::PAYLOAD_SNAPSHOT> snapshot(new obc_data::PAYLOAD_SNAPSHOT{});
    snapshot->stamp = {1, 0};
    snapshot->id = 1;
    size_t expected = 0;
    assert(obc_record::GetSnapshotSize(*snapshot, expected) == obc_record::STATUS::OK && expected == 37);
    ResetFaults();
    assert(buffers.Reserve(expected));
    size_t written = 0;
    assert(obc_record::EncodeSnapshotParts(*snapshot, buffers.GetFirstData(), expected,
        buffers.GetSecondData(), 0, written) == obc_record::STATUS::OK && written == 37);
    std::unique_ptr<obc_data::PAYLOAD_SNAPSHOT> decoded(new obc_data::PAYLOAD_SNAPSHOT{});
    size_t consumed = 0;
    assert(obc_record::DecodeSnapshot(buffers.GetFirstData(), written, *decoded, consumed)
        == obc_record::STATUS::OK);
    assert(consumed == written && decoded->id == 1 && decoded->aircraft_count == 0);
    std::puts("SnapshotBuffers: tamanhos, fronteiras, duas falhas de reserva e codec aprovados.");
}
