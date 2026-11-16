/** @file test_main.cpp
 * @brief Verifica a API diagnóstica real, contato por meio e cópias concorrentes.
 */
#include "Ttc.h" // Proprietário real, transportes substituíveis e FreeRTOS.
#include <unity.h> // RUN_TEST e TEST_ASSERT_*.
#include <deque>
#include <string>
#include <cstring> // memcpy dos quadros de teste.
#include <atomic> // Disponibilidade consultada pela tarefa após Start.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "Ttc.h".
 */

namespace {

    // ─── CLASSES ──────────────────────────────────────────────────────

    class DiagnosticRadio : public LoraRadio {
    public:
        std::atomic<bool> available{true};
        std::deque<std::string> inbox; // Alimentada somente antes de Start.
        bool Init() override { return available.load(); }
        bool IsReady() override { return available.load(); }
        bool CheckHealth() override { return available.load(); }
        bool Send(
            const uint8_t* data,
            size_t len
        ) override { return data != nullptr && len > 0 && available.load(); }
        int Receive(
            uint8_t* bytes,
            size_t capacity
        ) override {
            if (inbox.empty()) return 0;
            const std::string frame = inbox.front();
            inbox.pop_front();
            if (frame.size() > capacity) return -1;
            std::memcpy(bytes, frame.data(), frame.size());
            return static_cast<int>(frame.size());
        }
        int16_t GetRssi() override { return -80; }
        float GetSnr() override { return 9.5f; }
    };

    // Instâncias permanentes: a tarefa criada no último caso continua válida.
    DiagnosticRadio radio;
    DiagnosticRadio wired;
    Ttc link(radio, &wired);

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Quadros inválidos não viram contato; TC e cf válidos conservam seu meio.
    void TestInitialAndContacts(
    ) {
        auto sample = link.GetDiagnostics();
        TEST_ASSERT_FALSE(sample.init_attempted);
        radio.available.store(false);
        TEST_ASSERT_FALSE(link.Init());
        sample = link.GetDiagnostics();
        TEST_ASSERT_TRUE(sample.init_attempted && sample.initialized && sample.wired_ready);
        TEST_ASSERT_FALSE(sample.radio_ready || sample.task_started || sample.rf_contact.seen || sample.wired_contact.seen);
        TEST_ASSERT_EQUAL_UINT32(0, sample.stack_free_bytes);

        radio.inbox.push_back("{\"t\":\"tc\",\"seq\":1,\"cmd\":\"fly\"}");
        wired.inbox.push_back("{\"t\":\"cf\",\"id\":[0,1]}");
        link.Update();
        sample = link.GetDiagnostics();
        TEST_ASSERT_FALSE(sample.rf_contact.seen || sample.wired_contact.seen);

        radio.inbox.push_back("{\"t\":\"tc\",\"seq\":51,\"cmd\":\"ping\"}");
        wired.inbox.push_back("{\"t\":\"cf\",\"id\":[1,1]}");
        link.Update();
        sample = link.GetDiagnostics();
        TEST_ASSERT_TRUE(sample.rf_contact.seen && sample.wired_contact.seen);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(sample.sampled_ms, sample.rf_contact.received_ms);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(sample.sampled_ms, sample.wired_contact.received_ms);
        TC_FRAME tc = {};
        TEST_ASSERT_TRUE(link.ReceiveTelecommand(tc, 0));
        TEST_ASSERT_EQUAL_UINT16(51, tc.seq);
        TEST_ASSERT_TRUE(link.SendAck(tc.seq, tc.cmd, ttc::ACK_OK));
    }

    // Update manual não fabrica tarefa; conclusão/erro/ociosidade avançam o contador.
    void TestManualProgressAndRadio(
    ) {
        const auto before = link.GetDiagnostics();
        for (unsigned index = 0; index < 20; ++index) link.Update();
        auto sample = link.GetDiagnostics();
        TEST_ASSERT_TRUE(sample.completions == before.completions + 20);
        TEST_ASSERT_FALSE(sample.updating || sample.task_started);
        TEST_ASSERT_FALSE(sample.radio_ready);
        const uint32_t attempts = sample.radio_resets;
        radio.available.store(true);
        delay(1100);
        link.Update();
        sample = link.GetDiagnostics();
        TEST_ASSERT_TRUE(sample.radio_ready && link.IsHealthy());
        TEST_ASSERT_EQUAL_UINT32(attempts, sample.radio_resets);
    }

    // Só Start cria concorrência; getter nunca chama driver nem mantém lock na varredura de pilha.
    void TestTaskSnapshots(
    ) {
        // Este caso prepara seu rádio mesmo quando o caso anterior aborta.
        radio.available.store(true);
        delay(1100);
        link.Update();
        TEST_ASSERT_TRUE(link.GetDiagnostics().radio_ready);
        TEST_ASSERT_TRUE(link.Start());
        TEST_ASSERT_TRUE(link.Start());
        uint64_t previous = link.GetDiagnostics().completions;
        bool progressed = false;
        for (unsigned index = 0; index < 100; ++index) {
            const auto sample = link.GetDiagnostics();
            TEST_ASSERT_TRUE(sample.task_started && sample.initialized && sample.radio_ready);
            TEST_ASSERT_GREATER_THAN_UINT32(0, sample.stack_free_bytes);
            TEST_ASSERT_TRUE(sample.completions >= previous);
            TEST_ASSERT_TRUE(static_cast<uint32_t>(sample.sampled_ms - sample.progress_ms) < 2000);
            progressed = progressed || sample.completions > previous;
            previous = sample.completions;
            delay(1);
        }
        TEST_ASSERT_TRUE(progressed);
    }

} // namespace

void setUp(
) {}

void tearDown(
) {}

void setup(
) {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(TestInitialAndContacts);
    RUN_TEST(TestManualProgressAndRadio);
    RUN_TEST(TestTaskSnapshots);
    UNITY_END();
}

void loop(
) { delay(1000); }
