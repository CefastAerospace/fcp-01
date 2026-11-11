/** @file ObcDeliveryCheck.cpp
 * @brief Evidência própria INT-006 para cache, consumidores, resets e arquivos reais.
 */
#include "ObcDelivery.h" // Fila, codec e scanner reais.
#include <cassert>
#include <cstdlib>
#include <vector>

namespace {
    obc_data::PAYLOAD_SNAPSHOT snapshot;

    // O backend real também atende ao roteiro de extração por blocos.
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

    // Acrescenta o mesmo formato do firmware, sem serializar estruturas de RAM.
    void Append(
        obc_storage::Storage& storage,
        const obc_delivery::RECORD& record
    ) {
        uint8_t bytes[obc_delivery::data_bytes];
        size_t size = 0;
        assert(obc_delivery::Encode(record, bytes, sizeof(bytes), size));
        assert(storage.Append(bytes, size).status == obc_storage::STATUS::OK);
    }

    // Uma aeronave com posição ausente e altitude conhecida em pés.
    void Observe(
        obc_delivery::Queue& queue,
        uint64_t boot,
        double altitude,
        uint64_t ms = 123
    ) {
        snapshot = {};
        snapshot.stamp = {boot, ms};
        snapshot.aircraft_count = 1;
        snapshot.aircraft[0].icao = 0xabcdef;
        snapshot.aircraft[0].available_fields = obc_data::SNAPSHOT_ALTITUDE;
        snapshot.aircraft[0].altitude = altitude;
        assert(obc_data::Validate(snapshot) == obc_data::STATUS::OK);
        queue.ObserveValidatedSnapshot(snapshot);
    }

    // Cópias entregues aos consumidores permanecem válidas em mudanças do cache.
    void CheckQueue(
    ) {
        using namespace obc_delivery;
        Queue queue;
        queue.Init(7, false);
        Observe(queue, 7, 100);
        Observe(queue, 7, 100, 124);
        Observe(queue, 7, 200, 125);
        Observe(queue, 7, 100, 126);
        assert(queue.GetStatistics().accepted == 3 && queue.GetStatistics().repeated == 1);
        RECORD writing = {};
        assert(queue.BeginStorage(writing) && writing.delivery.id.sequence == 1);
        DELIVERY transmitting = {};
        assert(queue.BeginTransmit(1000, 50, transmitting));
        assert(transmitting.monotonic_ms == 123 && transmitting.aircraft.altitude == 100);
        assert(!queue.BeginTransmit(1001, 50, transmitting));
        assert(!queue.Confirm({7, 99}));
        assert(queue.Confirm(writing.delivery.id)); // ACK durante gravação e transmissão.
        assert(queue.Confirm(writing.delivery.id)); // ACK repetido.
        assert(!queue.FinishTransmit(writing.delivery.id, true)); // Evento local atrasado.
        queue.FinishStorage(writing, true);
        RECORD confirmation = {};
        assert(queue.BeginStorage(confirmation) && confirmation.confirmation);
        queue.FinishStorage(confirmation, false); // ACK em RAM avança apesar de falha persistente.
        assert(queue.Restore(queue.GetFrontier(), nullptr));
        assert(queue.BeginTransmit(1002, 50, transmitting) && transmitting.id.sequence == 2);
        assert(queue.FinishTransmit(transmitting.id, false));
        assert(!queue.BeginTransmit(1051, 50, transmitting));
        assert(queue.BeginTransmit(1052, 50, transmitting) && transmitting.id.sequence == 2);
        assert(queue.FinishTransmit(transmitting.id, true));
        assert(queue.Confirm(transmitting.id));
        assert(queue.Restore(queue.GetFrontier(), nullptr));
        assert(queue.BeginStorage(writing) && !writing.confirmation && writing.delivery.id.sequence == 2);
        queue.FinishStorage(writing, false); // SD falha, entrega já confirmada sai da RAM.
        assert(queue.BeginTransmit(1100, 50, transmitting) && transmitting.id.sequence == 3);
        assert(queue.FinishTransmit(transmitting.id, true));
        assert(queue.BeginStorage(writing) && writing.delivery.id.sequence == 3);
        queue.FinishStorage(writing, true);
        assert(queue.Confirm(transmitting.id));
        assert(queue.BeginStorage(confirmation) && confirmation.confirmation);
        queue.FinishStorage(confirmation, true);
        assert(queue.GetStatistics().occupied == 0);
        assert(queue.GetStatistics().confirmed == 3 && queue.GetStatistics().storage_failures == 2);

        // Saturação conserva todas as 32 alterações anteriores, com perda explícita.
        queue.Init(8, false);
        for (size_t index = 0; index < cache_slots + 1; ++index) Observe(queue, 8, index);
        assert(queue.GetStatistics().occupied == 32 && queue.GetStatistics().lost == 1);
        assert(queue.BeginStorage(writing));
        const RECORD stable = writing;
        Observe(queue, 8, 99);
        assert(writing.delivery.aircraft.altitude == stable.delivery.aircraft.altitude);
        queue.FinishStorage(writing, true);
        Observe(queue, 8, 100); // A cópia já durável pode sair para o SD.
        assert(queue.GetStatistics().evicted == 1 && queue.NeedsRecovery());
        assert(queue.GetStatistics().occupied == cache_slots - 1);
        assert(queue.BeginStorage(writing) && writing.delivery.id.sequence == 2);
        queue.FinishStorage(writing, true);
        assert(queue.Restore({}, &stable.delivery));
        assert(queue.BeginTransmit(0, 50, transmitting) && transmitting.id.sequence == 1);

        // Recuperação pendente, rajada de recepção e falha SD mantêm espaço para o FIFO antigo.
        queue.Init(10, true);
        for (size_t index = 0; index < cache_slots; ++index) Observe(queue, 10, index);
        assert(queue.GetStatistics().accepted == cache_slots - 1);
        assert(queue.GetStatistics().occupied == cache_slots - 1 && queue.GetStatistics().lost == 1);
        assert(queue.BeginStorage(writing));
        queue.FinishStorage(writing, false);
        const DELIVERY old = {{9, 1}, 456, {0xabcdef, obc_data::SNAPSHOT_ALTITUDE, 0, 0, 99}};
        assert(queue.Restore({}, &old));
        assert(queue.GetStatistics().occupied == cache_slots);
        assert(queue.BeginTransmit(0, 50, transmitting));
        assert(Compare(transmitting.id, old.id) == 0 && transmitting.monotonic_ms == 456);
        assert(queue.FinishTransmit(transmitting.id, true));
        assert(queue.Confirm(transmitting.id));
        Observe(queue, 10, 100);
        assert(queue.GetStatistics().occupied == cache_slots && queue.GetStatistics().lost == 2);
        assert(queue.BeginStorage(confirmation) && confirmation.confirmation);
        queue.FinishStorage(confirmation, false);
        assert(queue.GetStatistics().occupied == cache_slots - 1);
        DELIVERY next = old;
        next.id.sequence = 2;
        assert(queue.Restore(queue.GetFrontier(), &next));
        assert(queue.BeginTransmit(1, 50, transmitting) && Compare(transmitting.id, next.id) == 0);
        assert(queue.FinishTransmit(transmitting.id, true) && queue.Confirm(transmitting.id));
        assert(queue.BeginStorage(confirmation) && confirmation.confirmation);
        queue.FinishStorage(confirmation, false);
        assert(queue.Restore(queue.GetFrontier(), nullptr));
        Observe(queue, 10, 101); // Consulta encerrada libera novamente as 32 posições.
        assert(queue.GetStatistics().occupied == cache_slots);
        assert(queue.BeginTransmit(2, 50, transmitting) && transmitting.id.boot == 10 && transmitting.id.sequence == 1);

        // Cópias duráveis podem ceder lugar a novas alterações conservando a reserva.
        queue.Init(11, false);
        for (size_t index = 0; index < cache_slots; ++index) Observe(queue, 11, index);
        assert(queue.BeginStorage(writing));
        const DELIVERY first_stored = writing.delivery;
        queue.FinishStorage(writing, true);
        assert(queue.BeginStorage(writing));
        queue.FinishStorage(writing, true);
        Observe(queue, 11, 32);
        assert(queue.GetStatistics().accepted == 33 && queue.GetStatistics().lost == 0);
        assert(queue.GetStatistics().occupied == 31 && queue.GetStatistics().evicted == 2);
        Observe(queue, 11, 33);
        assert(queue.GetStatistics().lost == 1 && queue.GetStatistics().occupied == 31);
        assert(queue.BeginStorage(writing));
        queue.FinishStorage(writing, true);
        Observe(queue, 11, 34);
        assert(queue.GetStatistics().accepted == 34 && queue.GetStatistics().occupied == 31);
        assert(queue.GetStatistics().evicted == 3 && queue.NeedsRecovery());
        assert(queue.Restore({}, &first_stored));
        assert(queue.BeginTransmit(0, 50, transmitting) && Compare(transmitting.id, first_stored.id) == 0);

        queue.Init(0, false);
        snapshot.stamp.boot_id = 0;
        queue.ObserveValidatedSnapshot(snapshot);
        assert(queue.GetStatistics().accepted == 0);
    }
}

int main(
) {
    using namespace obc_delivery;
    // Codec e domínios com saídas preservadas em falha.
    RECORD first = {false, {{4, 1}, 123, {0xabcdef, obc_data::SNAPSHOT_ALTITUDE, 0, 0, 100}}};
    uint8_t bytes[data_bytes];
    size_t size = 0;
    assert(Encode(first, bytes, sizeof(bytes), size) && size == data_bytes);
    RECORD decoded = {};
    assert(Decode(bytes, size, decoded));
    assert(decoded.delivery.id.boot == 4 && decoded.delivery.monotonic_ms == 123);
    for (size_t length = 0; length < size; ++length) assert(!Decode(bytes, length, decoded));
    bytes[4] = 2;
    assert(!Decode(bytes, size, decoded));
    assert(decoded.delivery.id.boot == 4);
    assert(Encode(first, bytes, sizeof(bytes), size));
    bytes[45] ^= 1;
    assert(!Decode(bytes, size, decoded));

    // Arquivos temporários reais, leitura em blocos e reconstrução após resets.
    char directory[] = "/tmp/fcp01-delivery-XXXXXX";
    assert(::mkdtemp(directory));
    obc_storage::Storage storage;
    assert(storage.Init(directory).status == obc_storage::STATUS::OK);
    const READER_IO io = {List, Read, &storage};
    const char* name = "boot_0000000000000004_00000000.bin";
    assert(storage.OpenNew(name).status == obc_storage::STATUS::OK);
    Append(storage, first);
    obc_storage::FILE_INFO info = {};
    assert(storage.List("", info).status == obc_storage::STATUS::OK && info.active);
    uint32_t total = 99;
    assert(storage.Read(name, 0, bytes, sizeof(bytes), total).status == obc_storage::STATUS::BUSY);
    assert(storage.Close().status == obc_storage::STATUS::OK);
    DELIVERY pending = {};
    bool found = false;
    assert(FindPending(io, {}, pending, found) && found && pending.id.sequence == 1);
    Queue reset;
    reset.Init(5, true);
    assert(!reset.BeginTransmit(0, 50, pending));
    assert(reset.Restore({}, &first.delivery));
    assert(reset.BeginTransmit(0, 50, pending) && pending.id.boot == 4 && pending.monotonic_ms == 123);
    assert(reset.FinishTransmit(pending.id, true)); // Reset après TX sem ACK conserva ID.
    assert(FindPending(io, {}, pending, found) && found && pending.id.boot == 4);
    assert(reset.Confirm(pending.id)); // Reset antes de persistir ACK também conserva pendência.
    assert(FindPending(io, {}, pending, found) && found);

    const char* second_name = "boot_0000000000000005_00000000.bin";
    assert(storage.OpenNew(second_name).status == obc_storage::STATUS::OK);
    RECORD ack = {true, first.delivery};
    Append(storage, ack);
    RECORD second = first;
    second.delivery.id.sequence = 2;
    second.delivery.monotonic_ms = 456;
    Append(storage, second);
    RECORD orphan = ack;
    orphan.delivery.id.sequence = 99;
    Append(storage, orphan);
    assert(Encode(ack, bytes, sizeof(bytes), size));
    assert(storage.Append(bytes, 20).status == obc_storage::STATUS::OK); // ACK interrompido.
    assert(storage.Close().status == obc_storage::STATUS::OK);
    RECOVERY diagnostic = {};
    assert(Audit(io, diagnostic));
    assert(diagnostic.data == 2 && diagnostic.confirmations == 2);
    assert(diagnostic.incomplete_tails == 1 && diagnostic.orphan_confirmations == 1);
    assert(FindPending(io, {}, pending, found) && found && pending.id.sequence == 2);
    assert(pending.monotonic_ms == 456);
    assert(FindPending(io, {4, 2}, pending, found) && !found);

    // Extração lista, lê blocos pequenos, encontra EOF e preserva bytes.
    std::vector<uint8_t> extracted;
    for (uint32_t offset = 0;;) {
        uint8_t block[7];
        const auto result = storage.Read(name, offset, block, sizeof(block), total);
        assert(result.status == obc_storage::STATUS::OK);
        extracted.insert(extracted.end(), block, block + result.transferred);
        offset += static_cast<uint32_t>(result.transferred);
        if (!result.transferred) break;
    }
    assert(extracted.size() == data_bytes && Decode(extracted.data(), extracted.size(), decoded));
    assert(storage.Read("missing.bin", 0, bytes, sizeof(bytes), total).status == obc_storage::STATUS::NOT_FOUND);
    assert(storage.Read(name, data_bytes + 1, bytes, sizeof(bytes), total).status == obc_storage::STATUS::INVALID_ARGUMENT);
    assert(storage.List(name, info).status == obc_storage::STATUS::OK && !std::strcmp(info.name, second_name));
    assert(storage.List(second_name, info).status == obc_storage::STATUS::OK && !info.name[0]);

    // Corrupção entre registros, DATA parcial e DATA íntegro posterior.
    const char* third_name = "boot_0000000000000006_00000000.bin";
    assert(storage.OpenNew(third_name).status == obc_storage::STATUS::OK);
    assert(Encode(first, bytes, sizeof(bytes), size));
    assert(storage.Append(bytes, 41).status == obc_storage::STATUS::OK);
    RECORD third = second;
    third.delivery.id = {6, 1};
    Append(storage, third);
    assert(storage.Close().status == obc_storage::STATUS::OK);
    assert(Audit(io, diagnostic) && diagnostic.data == 3 && diagnostic.corrupt_bytes >= 41);
    assert(FindPending(io, {4, 2}, pending, found) && found && pending.id.boot == 6);

    CheckQueue();
    assert(storage.End().status == obc_storage::STATUS::OK);
    char path[256];
    for (const char* file : {name, second_name, third_name}) {
        std::snprintf(path, sizeof(path), "%s/%s", directory, file);
        assert(::unlink(path) == 0);
    }
    assert(::rmdir(directory) == 0);
    std::printf("INT-006 delivery checks passed; queue_bytes=%zu\n", sizeof(Queue));
}
