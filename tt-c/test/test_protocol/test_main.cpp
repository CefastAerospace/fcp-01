#include <unity.h>       // para TEST_ASSERT_*, RUN_TEST
#include <stdio.h>       // para printf, snprintf
#include <string.h>      // para strlen, strstr, strcmp, strcpy, memset
#include <string>        // para std::string
#include "TtcProtocol.h" // para o contrato JSON

// Testes nativos do contrato JSON do enlace (common/protocol/ttc-json.md).

// ─── HELPERS ──────────────────────────────────────────────────────

// Interpreta um TC escrito como literal.
static bool Parse(
    const char* json,
    ttc::TC_INFO& info
) {
    return ttc::ParseTelecommand(json, strlen(json), info);
}

// TM com os valores de maior representação textual previstos.
static ttc::TELEMETRY_DATA WorstTelemetry(
) {
    ttc::TELEMETRY_DATA tm = {};
    tm.timestamp = 4294967295UL;
    tm.mode = ttc::MODE_SUN_POINTING;
    tm.vbat = -99.99f;
    tm.ibat = -9999.9f;
    tm.vsolar = -99.99f;
    tm.isolar = -9999.9f;
    for (size_t i = 0; i < ttc::thermal_sensors; i++) tm.temps[i] = -55.5f;
    tm.rate_dps = -999.9f;
    tm.azimuth_deg = -180.5f;
    tm.aircraft_count = 65535;
    tm.message_count = 4294967295UL;
    tm.rssi = -150;
    tm.snr = -20.5f;
    tm.rx_count = 65535;
    tm.error_count = 65535;
    tm.radio_resets = 65535;
    return tm;
}

// ─── TESTES ───────────────────────────────────────────────────────

void setUp(
) {
}

void tearDown(
) {
}

static void TestCommandTableRoundTrip(
) {
    const ttc::COMMAND_ID ids[] = {
        ttc::CMD_PING, ttc::CMD_SET_MODE, ttc::CMD_START_MISSION, ttc::CMD_STOP_MISSION,
        ttc::CMD_GET_STATUS, ttc::CMD_ENTER_SAFE, ttc::CMD_EXIT_SAFE, ttc::CMD_REQUEST_TELEMETRY,
        ttc::CMD_SET_TIME, ttc::CMD_ADCS_START, ttc::CMD_ADCS_STOP, ttc::CMD_DEPLOY_ANTENNA,
        ttc::CMD_REQUEST_LOG, ttc::CMD_TEST_TM, ttc::CMD_RESET_OBC
    };
    for (ttc::COMMAND_ID id : ids) {
        TEST_ASSERT_EQUAL_UINT8(id, ttc::CommandFromName(ttc::CommandName(id)));
    }
    TEST_ASSERT_EQUAL_UINT8(ttc::CMD_UNKNOWN, ttc::CommandFromName("nope"));
    TEST_ASSERT_EQUAL_UINT8(ttc::CMD_UNKNOWN, ttc::CommandFromName(nullptr));
}

static void TestParseValid(
) {
    ttc::TC_INFO info;
    TEST_ASSERT_TRUE(Parse("{\"t\":\"tc\",\"seq\":12,\"cmd\":\"set_time\",\"args\":{\"unix\":1760000000}}", info));
    TEST_ASSERT_TRUE(info.has_seq);
    TEST_ASSERT_EQUAL_UINT16(12, info.seq);
    TEST_ASSERT_EQUAL_UINT8(ttc::CMD_SET_TIME, info.cmd);
    TEST_ASSERT_EQUAL_UINT8(ttc::ACK_OK, info.status);
    TEST_ASSERT_EQUAL_STRING("set_time", info.cmd_name);

    TEST_ASSERT_TRUE(Parse("{\"t\":\"tc\",\"seq\":0,\"cmd\":\"ping\"}", info));
    TEST_ASSERT_EQUAL_UINT8(ttc::CMD_PING, info.cmd);
}

static void TestParseMalformed(
) {
    ttc::TC_INFO info;
    TEST_ASSERT_FALSE(Parse("{\"t\":\"tc\",\"seq\":1,\"cmd\":", info));
    TEST_ASSERT_FALSE(info.has_seq);
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_BAD_JSON, info.status);

    TEST_ASSERT_FALSE(Parse("[1,2,3]", info));
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_BAD_JSON, info.status);

    TEST_ASSERT_FALSE(ttc::ParseTelecommand(nullptr, 10, info));
}

static void TestParseEnvelopeErrors(
) {
    ttc::TC_INFO info;

    // Sem seq: nada a responder.
    TEST_ASSERT_FALSE(Parse("{\"t\":\"tc\",\"cmd\":\"ping\"}", info));
    TEST_ASSERT_FALSE(info.has_seq);

    // seq fora de 16 bits é tratado como ausente.
    TEST_ASSERT_FALSE(Parse("{\"t\":\"tc\",\"seq\":70000,\"cmd\":\"ping\"}", info));
    TEST_ASSERT_FALSE(info.has_seq);

    // Tipo errado com seq legível: NACK bad_json.
    TEST_ASSERT_FALSE(Parse("{\"t\":\"tm\",\"seq\":5,\"cmd\":\"ping\"}", info));
    TEST_ASSERT_TRUE(info.has_seq);
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_BAD_JSON, info.status);

    // Comando fora do catálogo: NACK unknown_cmd com o nome recebido.
    TEST_ASSERT_FALSE(Parse("{\"t\":\"tc\",\"seq\":6,\"cmd\":\"fly\"}", info));
    TEST_ASSERT_TRUE(info.has_seq);
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_UNKNOWN_CMD, info.status);
    TEST_ASSERT_EQUAL_STRING("fly", info.cmd_name);

    // args que não é objeto.
    TEST_ASSERT_FALSE(Parse("{\"t\":\"tc\",\"seq\":7,\"cmd\":\"set_time\",\"args\":5}", info));
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_INVALID_PARAM, info.status);
}

static void TestParseOversized(
) {
    std::string json = "{\"t\":\"tc\",\"seq\":1,\"cmd\":\"ping\",\"pad\":\"";
    json.append(ttc::max_frame_len, 'x');
    json += "\"}";
    ttc::TC_INFO info;
    TEST_ASSERT_FALSE(ttc::ParseTelecommand(json.c_str(), json.size(), info));
}

static void TestBuildAck(
) {
    char out[ttc::max_ack_len];
    size_t len = ttc::BuildAck(12, "ping", ttc::ACK_OK, 3, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"seq\":12,\"cmd\":\"ping\",\"st\":\"ok\",\"q\":3}", out);
    TEST_ASSERT_EQUAL_size_t(strlen(out), len);

    ttc::BuildAck(9, "fly", ttc::ERR_UNKNOWN_CMD, -1, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"seq\":9,\"cmd\":\"fly\",\"st\":\"unknown_cmd\"}", out);

    // Pior caso cabe em max_ack_len.
    len = ttc::BuildAck(65535, "abcdefghijklmnopqrstuvw", ttc::ERR_STATE_REJECTED, 2147483647, out, sizeof(out));
    TEST_ASSERT_GREATER_THAN(0, len);
}

static void TestBuildTelemetryTypical(
) {
    ttc::TELEMETRY_DATA tm = {};
    tm.timestamp = 1760000123UL;
    tm.mode = ttc::MODE_MISSION;
    tm.vbat = 7.81f;
    tm.ibat = 420.04f;
    tm.temps[0] = 21.46f;
    tm.snr = 9.5f;

    char out[ttc::max_frame_len + 1];
    size_t len = ttc::BuildTelemetry(tm, out, sizeof(out));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"t\":\"tm\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"md\":\"mission\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"vb\":7.81,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"ib\":420,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"tp\":[21.5,"));
}

static void TestBuildTelemetryWorstCaseFits(
) {
    char out[ttc::max_frame_len + 1];
    size_t len = ttc::BuildTelemetry(WorstTelemetry(), out, sizeof(out));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_body_len, len);

    // Com o maior contador, o quadro ainda cabe no FIFO.
    TEST_ASSERT_TRUE(ttc::InsertCounter(out, len, sizeof(out), 65535));
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_frame_len, len);
}

static void TestBuildAircraftWorstCaseFits(
) {
    ttc::AIRCRAFT_DATA aircraft = {};
    aircraft.timestamp = 4294967295UL;
    strcpy(aircraft.icao, "ABCDEF");
    aircraft.latitude = -89.12345678;
    aircraft.longitude = -179.12345678;
    aircraft.altitude = -99999.0f;
    aircraft.velocity = 9999.9f;
    aircraft.heading = 359.9f;
    aircraft.message_count = 65535;

    char out[ttc::max_frame_len + 1];
    size_t len = ttc::BuildAircraft(aircraft, out, sizeof(out));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"lat\":-89.12346,"));
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_body_len, len);
}

static void TestBuildRejectsSmallBuffer(
) {
    char out[16];
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildTelemetry(WorstTelemetry(), out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
}

static void TestBuildEventAndBeacon(
) {
    char out[ttc::max_frame_len + 1];
    ttc::BuildEvent("low_power", 10, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ev\",\"ts\":10,\"ev\":\"low_power\"}", out);

    ttc::BuildBeacon(10, ttc::MODE_SAFE, 7.2f, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"bcn\",\"ts\":10,\"md\":\"safe\",\"vb\":7.2}", out);
}

static void TestInsertCounter(
) {
    char json[64] = "{\"t\":\"tm\"}";
    size_t len = strlen(json);
    TEST_ASSERT_TRUE(ttc::InsertCounter(json, len, sizeof(json), 41));
    TEST_ASSERT_EQUAL_STRING("{\"n\":41,\"t\":\"tm\"}", json);
    TEST_ASSERT_EQUAL_size_t(strlen(json), len);

    char empty[16] = "{}";
    len = 2;
    TEST_ASSERT_TRUE(ttc::InsertCounter(empty, len, sizeof(empty), 0));
    TEST_ASSERT_EQUAL_STRING("{\"n\":0}", empty);

    // Sem espaço no buffer.
    char small[12] = "{\"t\":\"tm\"}";
    len = strlen(small);
    TEST_ASSERT_FALSE(ttc::InsertCounter(small, len, sizeof(small), 1));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"tm\"}", small);

    // Não é objeto.
    char array[16] = "[1]";
    len = 3;
    TEST_ASSERT_FALSE(ttc::InsertCounter(array, len, sizeof(array), 1));
}

static void TestInsertCounterRespectsFrameLimit(
) {
    // Corpo de max_frame_len - 5 bytes não comporta "n":65535,.
    char json[ttc::max_frame_len + 32];
    std::string body = "{\"p\":\"";
    body.append(ttc::max_frame_len - 5 - body.size() - 2, 'x');
    body += "\"}";
    strcpy(json, body.c_str());
    size_t len = body.size();
    TEST_ASSERT_FALSE(ttc::InsertCounter(json, len, sizeof(json), 65535));
}

// A máscara altera disponibilidade, sem mudar chaves, valores ou API antiga.
static void TestTelemetryAvailability(
) {
    ttc::TELEMETRY_DATA tm = {};
    tm.mode = ttc::MODE_BENCH;
    char legacy[ttc::max_frame_len + 1];
    char explicit_all[ttc::max_frame_len + 1];
    TEST_ASSERT_GREATER_THAN(0, ttc::BuildTelemetry(tm, legacy, sizeof(legacy)));
    TEST_ASSERT_GREATER_THAN(0, ttc::BuildTelemetry(tm, explicit_all,
        sizeof(explicit_all), ttc::TELEMETRY_ALL));
    TEST_ASSERT_EQUAL_STRING(legacy, explicit_all);

    const char* keys[] = {"ts", "md", "vb", "ib", "vs", "is", "tp", "w",
        "az", "ac", "msg", "rssi", "snr", "rx", "err", "rst"};
    JsonDocument expected;
    TEST_ASSERT_TRUE(deserializeJson(expected, legacy) == DeserializationError::Ok);
    // Cada campo pode ser disponibilizado independentemente, incluindo zeros.
    for (unsigned bit = 0; bit < 16; bit++) {
        char out[ttc::max_frame_len + 1];
        size_t len = ttc::BuildTelemetry(tm, out, sizeof(out), 1UL << bit);
        TEST_ASSERT_GREATER_THAN(0, len);
        JsonDocument actual;
        TEST_ASSERT_TRUE(deserializeJson(actual, out) == DeserializationError::Ok);
        TEST_ASSERT_EQUAL_STRING("tm", actual["t"]);
        TEST_ASSERT_EQUAL(17, actual.size());
        for (unsigned field = 0; field < 16; field++) {
            if (field == bit) TEST_ASSERT_TRUE(actual[keys[field]] == expected[keys[field]]);
            else TEST_ASSERT_TRUE(actual[keys[field]].isNull());
        }
        TEST_ASSERT_TRUE(ttc::InsertCounter(out, len, sizeof(out), 65535));
        TEST_ASSERT_LESS_OR_EQUAL(ttc::max_frame_len, len);
    }

    char out[ttc::max_frame_len + 1];
    TEST_ASSERT_GREATER_THAN(0, ttc::BuildTelemetry(tm, out, sizeof(out), 0));
    JsonDocument unavailable;
    TEST_ASSERT_TRUE(deserializeJson(unavailable, out) == DeserializationError::Ok);
    for (const char* key : keys) TEST_ASSERT_TRUE(unavailable[key].isNull());
    char small[16];
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildTelemetry(tm, small, sizeof(small), 0));
    TEST_ASSERT_EQUAL_STRING("", small);
}

// ID com a maior representação textual.
static constexpr ttc::DELIVERY_ID worst_id = {18446744073709551615ULL, 18446744073709551615ULL};

// Pior aeronave identificada: todos os campos com a maior representação.
static void TestBuildAircraftWithIdWorstCaseFits(
) {
    ttc::AIRCRAFT_DATA aircraft = {};
    aircraft.timestamp = 4294967295UL;
    strcpy(aircraft.icao, "ABCDEF");
    aircraft.latitude = -89.12345678;
    aircraft.longitude = -179.12345678;
    aircraft.altitude = -99999.0f;
    aircraft.velocity = -9999.9f;
    aircraft.heading = -359.9f;
    aircraft.message_count = 65535;

    char out[ttc::max_frame_len + 1];
    size_t len = ttc::BuildAircraft(aircraft, out, sizeof(out), ttc::AIRCRAFT_ALL, &worst_id);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_body_len, len);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"id\":[18446744073709551615,18446744073709551615]"));

    // Com o contador "n" no maior valor, o quadro cabe no FIFO.
    TEST_ASSERT_TRUE(ttc::InsertCounter(out, len, sizeof(out), 65535));
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_frame_len, len);
    printf("Pior aeronave identificada: %u bytes no quadro.\n", (unsigned)len);
}

static void TestBuildAircraftUnavailableFields(
) {
    ttc::AIRCRAFT_DATA aircraft = {};
    strcpy(aircraft.icao, "e4953a");
    aircraft.latitude = -34.5;
    aircraft.longitude = -58.25;
    aircraft.altitude = 0.0f; // Zero medido continua número.
    ttc::DELIVERY_ID id = {3, 41};

    char out[ttc::max_frame_len + 1];
    uint32_t available = ttc::AC_POSITION | ttc::AC_ALTITUDE;
    TEST_ASSERT_GREATER_THAN(0, ttc::BuildAircraft(aircraft, out, sizeof(out), available, &id));
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"ac\",\"id\":[3,41],\"ts\":null,\"icao\":\"e4953a\",\"lat\":-34.5,\"lon\":-58.25,"
        "\"alt\":0,\"vel\":null,\"hdg\":null,\"cnt\":null}", out);

    // Sem ID e com máscara padrão: formato anterior.
    TEST_ASSERT_GREATER_THAN(0, ttc::BuildAircraft(aircraft, out, sizeof(out)));
    TEST_ASSERT_NULL(strstr(out, "\"id\""));
    TEST_ASSERT_NULL(strstr(out, "null"));
}

static void TestBuildAircraftRejectsInvalidIdentity(
) {
    ttc::AIRCRAFT_DATA aircraft = {};
    char out[ttc::max_frame_len + 1];
    strcpy(aircraft.icao, "E4953");
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildAircraft(aircraft, out, sizeof(out)));
    strcpy(aircraft.icao, "E4953G");
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildAircraft(aircraft, out, sizeof(out)));

    strcpy(aircraft.icao, "E4953A");
    ttc::DELIVERY_ID zero_boot = {0, 1};
    ttc::DELIVERY_ID zero_sequence = {1, 0};
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildAircraft(aircraft, out, sizeof(out), ttc::AIRCRAFT_ALL, &zero_boot));
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildAircraft(aircraft, out, sizeof(out), ttc::AIRCRAFT_ALL, &zero_sequence));
    TEST_ASSERT_EQUAL_STRING("", out);
}

static void TestConfirmationRoundTrip(
) {
    char out[ttc::max_frame_len + 1];
    size_t len = ttc::BuildConfirmation(worst_id, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"cf\",\"id\":[18446744073709551615,18446744073709551615]}", out);
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_frame_len, len);

    ttc::DELIVERY_ID id = {};
    TEST_ASSERT_EQUAL_UINT8(ttc::CF_VALID, ttc::ParseConfirmation(out, len, id));
    TEST_ASSERT_TRUE(id.boot == worst_id.boot && id.sequence == worst_id.sequence);

    ttc::DELIVERY_ID zero = {0, 5};
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildConfirmation(zero, out, sizeof(out)));
}

static void TestConfirmationRejectsInvalid(
) {
    const char* invalid[] = {
        "{\"t\":\"cf\"}",
        "{\"t\":\"cf\",\"id\":[1]}",
        "{\"t\":\"cf\",\"id\":[1,2,3]}",
        "{\"t\":\"cf\",\"id\":[0,2]}",
        "{\"t\":\"cf\",\"id\":[1,0]}",
        "{\"t\":\"cf\",\"id\":[-1,2]}",
        "{\"t\":\"cf\",\"id\":[1.5,2]}",
        "{\"t\":\"cf\",\"id\":[\"1\",2]}",
        "{\"t\":\"cf\",\"id\":[1,true]}",
        "{\"t\":\"cf\",\"id\":[1,18446744073709551616]}",
        "{\"t\":\"cf\",\"id\":{\"b\":1,\"s\":2}}",
        "{\"t\":\"cf\",\"id\":[1,2],\"seq\":4}",
    };
    for (const char* json : invalid) {
        ttc::DELIVERY_ID id = {9, 9};
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ttc::CF_INVALID, ttc::ParseConfirmation(json, strlen(json), id), json);
        TEST_ASSERT_TRUE(id.boot == 0 && id.sequence == 0);
    }

    // Outros quadros seguem para a validação de TC.
    const char* others[] = {"{\"t\":\"tc\",\"seq\":1,\"cmd\":\"ping\"}", "{bad", "[1,2]"};
    for (const char* json : others) {
        ttc::DELIVERY_ID id;
        TEST_ASSERT_EQUAL_UINT8(ttc::CF_NOT_CONFIRMATION, ttc::ParseConfirmation(json, strlen(json), id));
    }
}

// Interpreta args de request_log escritos como literal.
static ttc::ACK_STATUS ParseLog(
    const char* args,
    ttc::LOG_REQUEST& out
) {
    std::string json = std::string("{\"t\":\"tc\",\"seq\":7,\"cmd\":\"request_log\",\"args\":") + args + "}";
    return ttc::ParseLogRequest(json.c_str(), json.size(), out);
}

static void TestParseLogRequestValid(
) {
    ttc::LOG_REQUEST request;
    TEST_ASSERT_EQUAL_UINT8(ttc::ACK_OK, ParseLog("{\"op\":\"list\"}", request));
    TEST_ASSERT_EQUAL_UINT8(ttc::LOG_LIST, request.op);
    TEST_ASSERT_EQUAL_STRING("", request.name);
    TEST_ASSERT_EQUAL_UINT8(ttc::LOG_DEST_ORIGIN, request.dest);

    TEST_ASSERT_EQUAL_UINT8(ttc::ACK_OK,
        ParseLog("{\"op\":\"list\",\"after\":\"boot_0000000000000001_00000002.bin\",\"to\":\"wired\"}", request));
    TEST_ASSERT_EQUAL_STRING("boot_0000000000000001_00000002.bin", request.name);
    TEST_ASSERT_EQUAL_UINT8(ttc::LOG_DEST_WIRED, request.dest);

    TEST_ASSERT_EQUAL_UINT8(ttc::ACK_OK,
        ParseLog("{\"op\":\"read\",\"name\":\"boot_01_02.bin\",\"off\":4294967295,\"len\":90,\"to\":\"rf\"}", request));
    TEST_ASSERT_EQUAL_UINT8(ttc::LOG_READ, request.op);
    TEST_ASSERT_EQUAL_STRING("boot_01_02.bin", request.name);
    TEST_ASSERT_EQUAL_UINT32(4294967295UL, request.offset);
    TEST_ASSERT_EQUAL_UINT16(90, request.length);
    TEST_ASSERT_EQUAL_UINT8(ttc::LOG_DEST_RF, request.dest);
}

static void TestParseLogRequestInvalid(
) {
    const char* invalid[] = {
        "{}",
        "{\"op\":\"delete\"}",
        "{\"op\":\"list\",\"name\":\"a.bin\"}",
        "{\"op\":\"list\",\"after\":\"../a\"}",
        "{\"op\":\"list\",\"to\":\"usb\"}",
        "{\"op\":\"list\",\"to\":1}",
        "{\"op\":\"read\",\"name\":\"\",\"off\":0,\"len\":1}",
        "{\"op\":\"read\",\"name\":\"dir/a.bin\",\"off\":0,\"len\":1}",
        "{\"op\":\"read\",\"name\":\".hidden\",\"off\":0,\"len\":1}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"off\":-1,\"len\":1}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"off\":4294967296,\"len\":1}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"off\":1.5,\"len\":1}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"off\":0,\"len\":0}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"off\":0,\"len\":91}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"len\":1}",
        "{\"op\":\"read\",\"name\":\"a.bin\",\"off\":0,\"len\":1,\"after\":\"x\"}",
        "{\"op\":\"read\",\"name\":\"0123456789012345678901234567890123456789\",\"off\":0,\"len\":1}",
    };
    for (const char* args : invalid) {
        ttc::LOG_REQUEST request;
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ttc::ERR_INVALID_PARAM, ParseLog(args, request), args);
    }
    ttc::LOG_REQUEST request;
    const char* no_args = "{\"t\":\"tc\",\"seq\":7,\"cmd\":\"request_log\"}";
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_INVALID_PARAM, ttc::ParseLogRequest(no_args, strlen(no_args), request));
    TEST_ASSERT_EQUAL_UINT8(ttc::ERR_BAD_JSON, ttc::ParseLogRequest("{", 1, request));
}

static void TestBuildLogDataWorstCaseFits(
) {
    // Maior nome aceito, maiores offsets e bloco máximo.
    char name[ttc::log_name_len];
    memset(name, 'a', ttc::log_name_len - 1);
    name[ttc::log_name_len - 1] = '\0';
    uint8_t block[ttc::log_block_max];
    for (size_t i = 0; i < sizeof(block); i++) block[i] = (uint8_t)(0xFF - i);

    char out[ttc::max_frame_len + 1];
    uint32_t total = 4294967295UL;
    size_t len = ttc::BuildLogData(65535, name, total - ttc::log_block_max, total, block, sizeof(block), out, sizeof(out));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_body_len, len);
    TEST_ASSERT_TRUE(ttc::InsertCounter(out, len, sizeof(out), 65535));
    printf("Pior resposta read: %u bytes no quadro.\n", (unsigned)len);

    // Um byte acima do bloco não é aceito.
    uint8_t big[ttc::log_block_max + 1] = {};
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildLogData(1, "a.bin", 0, 1000, big, sizeof(big), out, sizeof(out)));
}

static void TestBuildLogDataContent(
) {
    char out[ttc::max_frame_len + 1];
    const uint8_t bytes[] = {'F', 'C', 'D', '1', 0x01};
    ttc::BuildLogData(12, "boot_01.bin", 10, 20, bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"lg\",\"rq\":12,\"op\":\"read\",\"name\":\"boot_01.bin\",\"off\":10,\"tot\":20,\"d\":\"RkNEMQE=\"}", out);

    // EOF: offset igual ao total e zero bytes.
    TEST_ASSERT_GREATER_THAN(0, ttc::BuildLogData(12, "boot_01.bin", 20, 20, nullptr, 0, out, sizeof(out)));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"d\":\"\""));

    // Bytes além do total ou offset maior que o total são erro do chamador.
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildLogData(12, "boot_01.bin", 21, 20, nullptr, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0, ttc::BuildLogData(12, "boot_01.bin", 18, 20, bytes, sizeof(bytes), out, sizeof(out)));
}

static void TestBuildLogList(
) {
    ttc::LOG_ENTRY entries[8] = {};
    for (size_t i = 0; i < 8; i++) {
        snprintf(entries[i].name, sizeof(entries[i].name), "boot_%016x_%08x.bin", (unsigned)i, (unsigned)i);
        entries[i].size = 4294967295UL;
    }
    entries[7].open = true;

    char out[ttc::max_frame_len + 1];
    size_t used = 99;
    size_t len = ttc::BuildLogList(65535, entries, 8, false, out, sizeof(out), used);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_OR_EQUAL(ttc::max_body_len, len);
    TEST_ASSERT_GREATER_THAN(0, used);
    TEST_ASSERT_LESS_THAN(8, used);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"end\":false"));
    printf("Listagem: %u nomes por resposta.\n", (unsigned)used);

    ttc::BuildLogList(3, &entries[7], 1, false, out, sizeof(out), used);
    TEST_ASSERT_EQUAL_size_t(1, used);
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"lg\",\"rq\":3,\"op\":\"list\",\"ls\":[[\"boot_0000000000000007_00000007.bin\",4294967295,1]],\"end\":true}",
        out);

    ttc::BuildLogList(3, &entries[0], 1, true, out, sizeof(out), used);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"end\":false"));

    ttc::BuildLogList(4, nullptr, 0, false, out, sizeof(out), used);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"lg\",\"rq\":4,\"op\":\"list\",\"ls\":[],\"end\":true}", out);
}

static void TestBuildLogError(
) {
    char out[ttc::max_frame_len + 1];
    ttc::BuildLogError(5, ttc::LOG_READ, "boot_01.bin", 90, ttc::LOG_BUSY, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"lg\",\"rq\":5,\"op\":\"read\",\"name\":\"boot_01.bin\",\"off\":90,\"err\":\"busy\"}", out);
    ttc::BuildLogError(6, ttc::LOG_LIST, nullptr, 0, ttc::LOG_NO_MEDIA, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"lg\",\"rq\":6,\"op\":\"list\",\"err\":\"no_sd\"}", out);
    TEST_ASSERT_EQUAL_STRING("not_found", ttc::LogErrorName(ttc::LOG_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("range", ttc::LogErrorName(ttc::LOG_RANGE));
    TEST_ASSERT_EQUAL_STRING("io", ttc::LogErrorName(ttc::LOG_IO));
}

int main(
    int argc,
    char** argv
) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(TestCommandTableRoundTrip);
    RUN_TEST(TestParseValid);
    RUN_TEST(TestParseMalformed);
    RUN_TEST(TestParseEnvelopeErrors);
    RUN_TEST(TestParseOversized);
    RUN_TEST(TestBuildAck);
    RUN_TEST(TestBuildTelemetryTypical);
    RUN_TEST(TestBuildTelemetryWorstCaseFits);
    RUN_TEST(TestBuildAircraftWorstCaseFits);
    RUN_TEST(TestBuildRejectsSmallBuffer);
    RUN_TEST(TestBuildEventAndBeacon);
    RUN_TEST(TestInsertCounter);
    RUN_TEST(TestInsertCounterRespectsFrameLimit);
    RUN_TEST(TestTelemetryAvailability);
    RUN_TEST(TestBuildAircraftWithIdWorstCaseFits);
    RUN_TEST(TestBuildAircraftUnavailableFields);
    RUN_TEST(TestBuildAircraftRejectsInvalidIdentity);
    RUN_TEST(TestConfirmationRoundTrip);
    RUN_TEST(TestConfirmationRejectsInvalid);
    RUN_TEST(TestParseLogRequestValid);
    RUN_TEST(TestParseLogRequestInvalid);
    RUN_TEST(TestBuildLogDataWorstCaseFits);
    RUN_TEST(TestBuildLogDataContent);
    RUN_TEST(TestBuildLogList);
    RUN_TEST(TestBuildLogError);
    return UNITY_END();
}
