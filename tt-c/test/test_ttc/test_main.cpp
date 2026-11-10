#include <Arduino.h>     // Framework Arduino.
#include <unity.h>       // para TEST_ASSERT_*, RUN_TEST
#include <string>        // para std::string
#include <deque>         // para std::deque
#include <vector>        // para std::vector
#include "Ttc.h"         // para Ttc, LoraRadio, LinkTransport

// Testes do Ttc na placa, com meios falsos no lugar do SX1276 e da UART0.
// Update() é chamada diretamente, sem a task do enlace.

// ─── CLASSES ──────────────────────────────────────────────────────

// Meio falso: entrega quadros injetados e guarda os transmitidos.
class FakeTransport : public LoraRadio {
public:
    std::deque<std::string> inbox;
    std::vector<std::string> sent;
    bool healthy = true;
    int init_calls = 0;

    bool Init() override {
        init_calls++;
        healthy = true;
        return true;
    }

    bool Send(
        const uint8_t* data,
        size_t len
    ) override {
        sent.emplace_back((const char*)data, len);
        return true;
    }

    int Receive(
        uint8_t* buffer,
        size_t max_len
    ) override {
        if (inbox.empty()) return 0;
        std::string frame = inbox.front();
        inbox.pop_front();
        size_t len = min(frame.size(), max_len);
        memcpy(buffer, frame.data(), len);
        return (int)len;
    }

    bool IsReady() override { return true; }
    bool CheckHealth() override { return healthy; }
    int16_t GetRssi() override { return -80; }
    float GetSnr() override { return 9.5f; }
};

// ─── HELPERS ──────────────────────────────────────────────────────

// TC no formato do contrato.
static std::string Tc(
    unsigned seq,
    const char* cmd
) {
    return "{\"t\":\"tc\",\"seq\":" + std::to_string(seq) + ",\"cmd\":\"" + cmd + "\"}";
}

static bool Contains(
    const std::string& text,
    const char* fragment
) {
    return text.find(fragment) != std::string::npos;
}

// Executa Update() até esvaziar a descida (limite de segurança).
static void Drain(
    Ttc& link
) {
    for (int i = 0; i < 64 && link.Update(); i++) {}
}

// ─── TESTES ───────────────────────────────────────────────────────

void setUp(
) {
}

void tearDown(
) {
}

static void TestTelecommandAndAck(
) {
    FakeTransport radio;
    Ttc link(radio);
    TEST_ASSERT_TRUE(link.Init());

    radio.inbox.push_back(Tc(7, "ping"));
    link.Update();

    TC_FRAME tc;
    TEST_ASSERT_TRUE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL_UINT16(7, tc.seq);
    TEST_ASSERT_EQUAL_UINT8(ttc::CMD_PING, tc.cmd);
    TEST_ASSERT_EQUAL_UINT8(LINK_RF, tc.origin);

    TEST_ASSERT_TRUE(link.SendAck(tc.seq, tc.cmd, ttc::ACK_OK));
    Drain(link);
    TEST_ASSERT_EQUAL(1, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "{\"n\":0,\"t\":\"ack\",\"seq\":7,\"cmd\":\"ping\",\"st\":\"ok\",\"q\":"));
    TEST_ASSERT_NOT_EQUAL(0, link.GetLinkStats().last_tc_ms);
}

static void TestAckHasPriority(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();

    radio.inbox.push_back(Tc(1, "request_tm"));
    link.Update();
    TC_FRAME tc;
    TEST_ASSERT_TRUE(link.ReceiveTelecommand(tc, 0));

    link.Send("{\"t\":\"ev\",\"ev\":\"a\"}", 19);
    link.Send("{\"t\":\"ev\",\"ev\":\"b\"}", 19);
    link.SendAck(tc.seq, tc.cmd, ttc::ACK_OK);
    Drain(link);

    TEST_ASSERT_EQUAL(3, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "\"t\":\"ack\""));
    TEST_ASSERT_TRUE(Contains(radio.sent[1], "{\"n\":1,"));
    TEST_ASSERT_TRUE(Contains(radio.sent[2], "{\"n\":2,"));
}

static void TestOneFramePerUpdate(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();

    for (int i = 0; i < 3; i++) link.Send("{\"t\":\"ev\",\"ev\":\"x\"}", 19);
    TEST_ASSERT_TRUE(link.Update());
    TEST_ASSERT_EQUAL(1, radio.sent.size());
    TEST_ASSERT_TRUE(link.Update());
    TEST_ASSERT_FALSE(link.Update());
    TEST_ASSERT_EQUAL(3, radio.sent.size());
}

static void TestDuplicateTelecommand(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();

    // Repetido antes do ACK: não reentrega nem responde.
    radio.inbox.push_back(Tc(9, "ping"));
    radio.inbox.push_back(Tc(9, "ping"));
    link.Update();
    link.Update();
    TC_FRAME tc;
    TEST_ASSERT_TRUE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_FALSE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL(0, radio.sent.size());

    link.SendAck(tc.seq, tc.cmd, ttc::ACK_OK);
    Drain(link);
    TEST_ASSERT_EQUAL(1, radio.sent.size());

    // Repetido depois do ACK: reenvia o mesmo ACK, com novo "n".
    radio.inbox.push_back(Tc(9, "ping"));
    Drain(link);
    TEST_ASSERT_FALSE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL(2, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[1], "{\"n\":1,\"t\":\"ack\",\"seq\":9"));
    TEST_ASSERT_EQUAL(2, link.GetLinkStats().duplicates_count);
}

static void TestNackForBadEnvelope(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();

    radio.inbox.push_back("{\"t\":\"tc\",\"seq\":4,\"cmd\":\"fly\"}");
    radio.inbox.push_back("{\"t\":\"tc\",\"seq\":");
    Drain(link);
    link.Update();
    Drain(link);

    TC_FRAME tc;
    TEST_ASSERT_FALSE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL(1, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "\"seq\":4,\"cmd\":\"fly\",\"st\":\"unknown_cmd\"}"));
    TEST_ASSERT_EQUAL(2, link.GetLinkStats().rx_errors_count);
}

static void TestHoldTransmit(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();

    TEST_ASSERT_TRUE(link.HoldTransmit(true, 0));
    link.Send("{\"t\":\"ev\",\"ev\":\"x\"}", 19);
    TEST_ASSERT_FALSE(link.Update());
    TEST_ASSERT_EQUAL(0, radio.sent.size());

    link.HoldTransmit(false, 0);
    Drain(link);
    TEST_ASSERT_EQUAL(1, radio.sent.size());
}

static void TestWiredOriginAndCounters(
) {
    FakeTransport radio;
    FakeTransport wired;
    Ttc link(radio, &wired);
    link.Init();

    // ACK de TC do cabo volta só pelo cabo.
    wired.inbox.push_back(Tc(3, "ping"));
    link.Update();
    TC_FRAME tc;
    TEST_ASSERT_TRUE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL_UINT8(LINK_WIRED, tc.origin);
    link.SendAck(tc.seq, tc.cmd, ttc::ACK_OK);
    Drain(link);
    TEST_ASSERT_EQUAL(0, radio.sent.size());
    TEST_ASSERT_EQUAL(1, wired.sent.size());

    // Descida comum vai pelos dois, cada um com seu contador.
    link.Send("{\"t\":\"ev\",\"ev\":\"x\"}", 19);
    Drain(link);
    TEST_ASSERT_EQUAL(1, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "{\"n\":0,"));
    TEST_ASSERT_TRUE(Contains(wired.sent[1], "{\"n\":1,"));

    // Cabo desligado: só RF.
    link.SetUmbilicalEnabled(false);
    link.Send("{\"t\":\"ev\",\"ev\":\"y\"}", 19);
    Drain(link);
    TEST_ASSERT_EQUAL(2, radio.sent.size());
    TEST_ASSERT_EQUAL(2, wired.sent.size());
    TEST_ASSERT_EQUAL(1, link.GetLinkStats().wired_rx_packets_count);
}

static void TestRadioHealthReset(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();
    TEST_ASSERT_TRUE(link.IsHealthy());

    radio.healthy = false;
    delay(1100); // Período da verificação de saúde.
    link.Update();
    TEST_ASSERT_EQUAL(2, radio.init_calls);
    TEST_ASSERT_EQUAL(1, link.GetLinkStats().radio_resets_count);
    TEST_ASSERT_TRUE(link.IsHealthy());
}

static void TestTelemetryFillsLinkFields(
) {
    FakeTransport radio;
    Ttc link(radio);
    link.Init();

    radio.inbox.push_back(Tc(1, "ping"));
    link.Update();

    ttc::TELEMETRY_DATA tm = {};
    tm.mode = ttc::MODE_MISSION;
    TEST_ASSERT_TRUE(link.SendTelemetry(tm));
    Drain(link);
    TEST_ASSERT_EQUAL(1, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "\"md\":\"mission\""));
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "\"rssi\":-80,\"snr\":9.5,\"rx\":1,"));
}

// Ausência real do rádio não deve impedir o transporte cabeado.
class MissingRadio : public FakeTransport {
public:
    bool Init() override { return false; }
    bool IsReady() override { return false; }
    bool CheckHealth() override { return false; }
    bool Send(
        const uint8_t*,
        size_t
    ) override { return false; }
};

static void TestWiredWithoutRadioAndUnavailableTelemetry(
) {
    MissingRadio radio;
    FakeTransport wired;
    Ttc link(radio, &wired);
    TEST_ASSERT_FALSE(link.Init());
    TEST_ASSERT_FALSE(link.IsHealthy());
    wired.inbox.push_back(Tc(40, "request_tm"));
    link.Update();
    TC_FRAME tc;
    TEST_ASSERT_TRUE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL_UINT8(LINK_WIRED, tc.origin);

    ttc::TELEMETRY_DATA tm = {};
    tm.mode = ttc::MODE_BENCH;
    uint32_t available = ttc::TM_MODE | ttc::TM_RX_COUNT
        | ttc::TM_ERROR_COUNT | ttc::TM_RADIO_RESETS;
    const LINK_STATS before_tm = link.GetLinkStats();
    TEST_ASSERT_TRUE(before_tm.radio_resets_count >= 1);
    TEST_ASSERT_TRUE(link.SendTelemetry(tm, available));
    TEST_ASSERT_TRUE(link.SendAck(tc.seq, tc.cmd, ttc::ACK_OK));
    Drain(link);
    TEST_ASSERT_EQUAL(2, wired.sent.size());
    TEST_ASSERT_TRUE(Contains(wired.sent[0], "\"t\":\"ack\""));
    TEST_ASSERT_TRUE(Contains(wired.sent[1], "\"ts\":null,\"md\":\"bench\""));
    TEST_ASSERT_TRUE(Contains(wired.sent[1], "\"rssi\":null,\"snr\":null,\"rx\":1,"));
    JsonDocument received_tm;
    TEST_ASSERT_TRUE(deserializeJson(received_tm, wired.sent[1]) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_UINT16(0, received_tm["err"].as<uint16_t>());
    TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(before_tm.radio_resets_count),
        received_tm["rst"].as<uint16_t>());

    // Retentativa reenvia o ACK, sem entregar outro pedido ao OBC.
    wired.inbox.push_back(Tc(40, "request_tm"));
    Drain(link);
    TEST_ASSERT_FALSE(link.ReceiveTelecommand(tc, 0));
    TEST_ASSERT_EQUAL(3, wired.sent.size());
    TEST_ASSERT_TRUE(Contains(wired.sent[2], "\"t\":\"ack\""));
}

static void TestTelemetryQueueFullKeepsAckQueueAvailable(
) {
    FakeTransport radio;
    Ttc link(radio);
    TEST_ASSERT_TRUE(link.Init());
    TEST_ASSERT_TRUE(link.HoldTransmit(true, 0));
    ttc::TELEMETRY_DATA tm = {};
    for (int i = 0; i < 16; i++) TEST_ASSERT_TRUE(link.SendTelemetry(tm));
    TEST_ASSERT_FALSE(link.SendTelemetry(tm));
    TEST_ASSERT_TRUE(link.SendAck(55, ttc::CMD_REQUEST_TELEMETRY, ttc::ERR_STATE_REJECTED));
    TEST_ASSERT_TRUE(link.HoldTransmit(false, 0));
    Drain(link);
    TEST_ASSERT_EQUAL(17, radio.sent.size());
    TEST_ASSERT_TRUE(Contains(radio.sent[0], "\"st\":\"state_rejected\""));
}

// A saída opcional é a mesma amostra enfileirada e permanece intacta em falha.
static void TestTelemetryQueuedOutput(
) {
    FakeTransport radio;
    Ttc link(radio);
    TEST_ASSERT_TRUE(link.Init());
    radio.inbox.push_back(Tc(60, "ping"));
    link.Update();
    ttc::TELEMETRY_DATA tm = {};
    tm.timestamp = 42;
    tm.mode = ttc::MODE_BENCH;
    ttc::TELEMETRY_DATA queued = {};
    TEST_ASSERT_TRUE(link.SendTelemetry(tm, ttc::TELEMETRY_ALL, &queued));
    TEST_ASSERT_EQUAL_UINT32(42, queued.timestamp);
    TEST_ASSERT_EQUAL_INT16(-80, queued.rssi);
    TEST_ASSERT_EQUAL_UINT16(1, queued.rx_count);
    for (unsigned index = 0; index < 15; ++index) TEST_ASSERT_TRUE(link.SendTelemetry(tm));
    queued.timestamp = 123456;
    TEST_ASSERT_FALSE(link.SendTelemetry(tm, ttc::TELEMETRY_ALL, &queued));
    TEST_ASSERT_EQUAL_UINT32(123456, queued.timestamp);
}

// setup() e loop() são funções obrigatórias do framework Arduino.

void setup(
) {
    delay(2000); // Tempo para o monitor serial conectar.
    UNITY_BEGIN();
    RUN_TEST(TestTelecommandAndAck);
    RUN_TEST(TestAckHasPriority);
    RUN_TEST(TestOneFramePerUpdate);
    RUN_TEST(TestDuplicateTelecommand);
    RUN_TEST(TestNackForBadEnvelope);
    RUN_TEST(TestHoldTransmit);
    RUN_TEST(TestWiredOriginAndCounters);
    RUN_TEST(TestRadioHealthReset);
    RUN_TEST(TestTelemetryFillsLinkFields);
    RUN_TEST(TestWiredWithoutRadioAndUnavailableTelemetry);
    RUN_TEST(TestTelemetryQueueFullKeepsAckQueueAvailable);
    RUN_TEST(TestTelemetryQueuedOutput);
    UNITY_END();
}

void loop(
) {
}
