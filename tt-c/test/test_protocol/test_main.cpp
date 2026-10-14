#include <unity.h>       // para TEST_ASSERT_*, RUN_TEST
#include <string.h>      // para strlen, strstr, strcmp
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
    return UNITY_END();
}
