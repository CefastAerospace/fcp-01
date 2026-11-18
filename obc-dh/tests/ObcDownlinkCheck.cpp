/** @file ObcDownlinkCheck.cpp
 * @brief Verificação portátil do mapeamento, extração real e caixa assíncrona.
 */
#include "ObcDownlink.h" // Código de produção, Storage e contrato TT&C.
#include <cassert>
#include <cstdlib>
#include <string>

namespace {

    // Reutiliza o núcleo real de arquivos; nenhum codec ou serviço SD alternativo.
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

    // Cada entrada é um TC independente, validado pelo parser real.
    ttc::LOG_REQUEST Request(
        const char* args
    ) {
        const std::string json = std::string("{\"t\":\"tc\",\"seq\":7,\"cmd\":\"request_log\",\"args\":") + args + "}";
        ttc::LOG_REQUEST request = {};
        assert(ttc::ParseLogRequest(json.c_str(), json.size(), request) == ttc::ACK_OK);
        return request;
    }

    // Valida resposta produzida, inclusive reserva do contador no maior valor.
    void ParseResponse(
        const obc_downlink::LOG_RESPONSE& response,
        JsonDocument& doc
    ) {
        assert(response.length && response.length <= ttc::max_body_len);
        assert(deserializeJson(doc, response.json, response.length) == DeserializationError::Ok);
        assert(doc["rq"].as<uint16_t>() == response.sequence);
        char framed[ttc::max_frame_len + 1];
        std::strcpy(framed, response.json);
        size_t length = response.length;
        assert(ttc::InsertCounter(framed, length, sizeof(framed), 65535));
        assert(length <= ttc::max_frame_len);
    }

    // Falhas injetadas exercitam o mapeamento sem alterar o serviço real.
    obc_storage::RESULT FailRead(
        void* context,
        const char*,
        uint32_t,
        uint8_t*,
        size_t,
        uint32_t& total
    ) {
        total = 0;
        return {*static_cast<obc_storage::STATUS*>(context), 1, EIO, 0};
    }

    void CheckAircraft(
    ) {
        obc_delivery::DELIVERY delivery = {{3, 41}, 123, {0xE4953A,
            obc_data::SNAPSHOT_LATITUDE | obc_data::SNAPSHOT_LONGITUDE | obc_data::SNAPSHOT_ALTITUDE,
            -15.1, -47.1, 0}};
        ttc::AIRCRAFT_DATA aircraft = {};
        uint32_t available = 0;
        assert(obc_downlink::MapAircraft(delivery, aircraft, available));
        assert(available == (ttc::AC_POSITION | ttc::AC_ALTITUDE));
        assert(aircraft.altitude == 0 && !std::strcmp(aircraft.icao, "e4953a"));
        char first[256];
        const ttc::DELIVERY_ID id = {delivery.id.boot, delivery.id.sequence};
        assert(ttc::BuildAircraft(aircraft, first, sizeof(first), available, &id));
        JsonDocument doc;
        assert(deserializeJson(doc, first) == DeserializationError::Ok);
        assert(doc["ts"].isNull() && doc["vel"].isNull() && doc["hdg"].isNull() && doc["cnt"].isNull());
        assert(doc["alt"].is<float>() && doc["alt"].as<float>() == 0);
        char retry[256];
        assert(obc_downlink::MapAircraft(delivery, aircraft, available));
        assert(ttc::BuildAircraft(aircraft, retry, sizeof(retry), available, &id));
        assert(!std::strcmp(first, retry) && delivery.monotonic_ms == 123);
        delivery.aircraft.available_fields = 0;
        assert(obc_downlink::MapAircraft(delivery, aircraft, available) && available == 0);
        delivery.aircraft.available_fields = obc_data::SNAPSHOT_ALTITUDE;
        delivery.aircraft.altitude = std::numeric_limits<double>::max();
        assert(!obc_downlink::MapAircraft(delivery, aircraft, available));
        delivery.aircraft.available_fields = 0;
        delivery.aircraft.icao = 0x1000000UL;
        assert(!obc_downlink::MapAircraft(delivery, aircraft, available));
    }

    void CheckMailbox(
    ) {
        obc_downlink::LogMailbox mailbox;
        const auto request = Request("{\"op\":\"list\",\"to\":\"wired\"}");
        obc_downlink::LOG_RESPONSE response = {};
        uint16_t sequence = 0;
        ttc::LOG_REQUEST work = {};
        assert(mailbox.Submit(1, request, 100));
        assert(!mailbox.Submit(2, request, 100));
        assert(!mailbox.Take(1099, response));
        assert(mailbox.Take(1100, response) && response.sequence == 1 && !response.length);
        assert(!mailbox.Begin(sequence, work)); // Cancelado antes de iniciar I/O.
        assert(mailbox.Submit(2, request, 2000));
        assert(mailbox.Begin(sequence, work) && sequence == 2);
        assert(!mailbox.Begin(sequence, work));
        assert(mailbox.Take(3000, response) && response.sequence == 2 && !response.length);
        assert(!mailbox.Take(3001, response));
        assert(!mailbox.Submit(3, request, 3001)); // Trabalho antigo ainda possui a caixa.
        obc_downlink::BuildLogFailure(2, work, ttc::LOG_IO, response);
        mailbox.Complete(response);
        assert(!mailbox.Take(3002, response)); // Resultado tardio descartado.
        assert(mailbox.Submit(3, request, 4000));
        assert(mailbox.Begin(sequence, work) && sequence == 3);
        obc_downlink::BuildLogFailure(3, work, ttc::LOG_BUSY, response);
        mailbox.Complete(response);
        assert(mailbox.Take(4001, response) && response.sequence == 3 && response.length);
        assert(response.request.dest == ttc::LOG_DEST_WIRED);
        assert(!mailbox.Take(4002, response));
        assert(mailbox.Submit(4, request, 5000));
        assert(mailbox.Begin(sequence, work));
        obc_downlink::BuildLogFailure(4, work, ttc::LOG_IO, response);
        mailbox.Complete(response);
        assert(mailbox.Take(6000, response) && !response.length); // Concluiu após prazo.
    }

    void CheckFiles(
    ) {
        char directory[] = "/tmp/fcp01-downlink-XXXXXX";
        assert(::mkdtemp(directory));
        obc_storage::Storage storage;
        assert(storage.Init(directory).status == obc_storage::STATUS::OK);
        const obc_delivery::READER_IO io = {List, Read, &storage};
        auto request = Request("{\"op\":\"list\"}");
        obc_downlink::LOG_RESPONSE response = {};
        JsonDocument doc;
        obc_downlink::BuildLogResponse(7, request, io, response);
        ParseResponse(response, doc);
        assert(doc["ls"].size() == 0 && doc["end"].as<bool>());
        // Nomes reais longos obrigam paginação dentro do limite de quadro.
        for (unsigned index = 1; index <= 6; ++index) {
            char name[40];
            std::snprintf(name, sizeof(name), "boot_%016x_%08x.bin", 1, index);
            assert(storage.OpenNew(name).status == obc_storage::STATUS::OK);
            const std::string content = std::string(180, 'x') + "FCD1";
            assert(storage.Append(reinterpret_cast<const uint8_t*>(content.data()), content.size()).status == obc_storage::STATUS::OK);
            if (index < 6) assert(storage.Close().status == obc_storage::STATUS::OK);
        }
        size_t listed = 0;
        bool active = false;
        for (;;) {
            obc_downlink::BuildLogResponse(7, request, io, response);
            ParseResponse(response, doc);
            assert(!doc["ls"].isNull() && doc["ls"].size() > 0);
            for (JsonArray entry : doc["ls"].as<JsonArray>()) {
                const char* name = entry[0];
                assert(std::strcmp(name, request.name) > 0);
                std::strcpy(request.name, name);
                assert(entry[1].as<unsigned>() == 184);
                active |= entry[2].as<bool>();
                ++listed;
            }
            if (doc["end"].as<bool>()) break;
            assert(listed < 6);
        }
        assert(listed == 6 && active);
        request = Request("{\"op\":\"read\",\"name\":\"boot_0000000000000001_00000006.bin\",\"off\":0,\"len\":90}");
        obc_downlink::BuildLogResponse(8, request, io, response);
        ParseResponse(response, doc);
        assert(doc["err"] == "busy");
        assert(storage.Close().status == obc_storage::STATUS::OK);
        std::string block;
        for (size_t index = 0; index < 30; ++index) block += "eHh4";
        for (uint32_t offset : {0U, 90U, 180U, 184U, 185U}) {
            request.offset = offset;
            obc_downlink::BuildLogResponse(9, request, io, response);
            ParseResponse(response, doc);
            if (offset == 185) assert(doc["err"] == "range");
            else {
                assert(doc["tot"].as<unsigned>() == 184 && doc["off"].as<unsigned>() == offset);
                const char* expected = offset < 180 ? block.c_str() : offset == 180 ? "RkNEMQ==" : "";
                assert(!std::strcmp(doc["d"].as<const char*>(), expected));
            }
        }
        std::strcpy(request.name, "missing.bin");
        obc_downlink::BuildLogResponse(10, request, io, response);
        ParseResponse(response, doc);
        assert(doc["err"] == "not_found");
        obc_downlink::BuildLogResponse(11, request, {}, response);
        ParseResponse(response, doc);
        assert(doc["err"] == "no_sd");
        obc_storage::STATUS failure = obc_storage::STATUS::READ_FAILED;
        obc_downlink::BuildLogResponse(12, request, {List, FailRead, &failure}, response);
        ParseResponse(response, doc);
        assert(doc["err"] == "io" && doc["d"].isNull()); // Bytes parciais não são publicados.
        failure = obc_storage::STATUS::BUSY;
        obc_downlink::BuildLogResponse(13, request, {List, FailRead, &failure}, response);
        ParseResponse(response, doc);
        assert(doc["err"] == "busy");
        assert(storage.End().status == obc_storage::STATUS::OK);
        for (unsigned index = 1; index <= 6; ++index) {
            char path[128];
            std::snprintf(path, sizeof(path), "%s/boot_%016x_%08x.bin", directory, 1, index);
            assert(::unlink(path) == 0);
        }
        assert(::rmdir(directory) == 0);
    }

} // namespace

int main(
) {
    CheckAircraft();
    CheckMailbox();
    CheckFiles();
    std::puts("ObcDownlinkCheck: mapping, mailbox and real file extraction passed");
}
