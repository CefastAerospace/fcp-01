/**
 * @file NativeI2cRuntime.h
 * @brief Primitivas mínimas para executar o executor real com threads no computador.
 */
#pragma once
#include <stdint.h> // Relógio e tipos do runtime.
#include <stddef.h> // size_t.
#include <mutex> // Proteção dos tokens e notificações.
#include <condition_variable> // Esperas com prazo e conclusão.
#include <thread> // Tarefas nativas usadas na verificação.
#include <chrono> // Relógio monotônico e esperas.
#include <vector> // Recursos de tarefas liberados ao encerrar o teste.

using TickType_t = uint32_t;
using UBaseType_t = unsigned;
using BaseType_t = int;
constexpr int pdTRUE = 1;
constexpr int pdFALSE = 0;
constexpr int pdPASS = 1;
constexpr uint32_t configTICK_RATE_HZ = 1000;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(value) (static_cast<TickType_t>(value))

struct StaticSemaphore_t {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned tokens = 0;
};
using SemaphoreHandle_t = StaticSemaphore_t*;

struct NATIVE_TASK {
    std::thread thread;
    std::mutex mutex;
    std::condition_variable changed;
    unsigned notifications = 0;
    bool stop = false;
};
using TaskHandle_t = NATIVE_TASK*;

/** @brief Wire de ensaio só participa da inicialização e da assinatura do callback. */
class TwoWire {
public:
    bool begin(
        int,
        int,
        uint32_t
    ) { return true; }
    bool end(
    ) { return true; }
    void setTimeOut(
        uint16_t
    ) {}
};
extern TwoWire Wire;

// ─── HELPERS ──────────────────────────────────────────────────────

/** @brief Inicializa primitivas com os mesmos tokens iniciais usados pelo serviço. */
SemaphoreHandle_t xSemaphoreCreateMutexStatic(
    StaticSemaphore_t* storage
);
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(
    StaticSemaphore_t* storage
);
BaseType_t xSemaphoreTake(
    SemaphoreHandle_t semaphore,
    TickType_t ticks
);
BaseType_t xSemaphoreGive(
    SemaphoreHandle_t semaphore
);
BaseType_t xTaskCreate(
    void (*entry)(void*),
    const char*,
    uint32_t,
    void*,
    UBaseType_t,
    TaskHandle_t*
);
TaskHandle_t xTaskGetCurrentTaskHandle(
);
void xTaskNotifyGive(
    TaskHandle_t task
);
uint32_t ulTaskNotifyTake(
    BaseType_t clear,
    TickType_t ticks
);
void vTaskDelay(
    TickType_t ticks
);
void vTaskDelete(
    TaskHandle_t task
);
UBaseType_t uxTaskGetStackHighWaterMark(
    TaskHandle_t task
);
bool xPortInIsrContext(
);
uint32_t millis(
);
uint32_t micros(
);
/** @brief Interrompe o worker ocioso e reúne todas as threads antes de liberar globais. */
void StopNativeI2cTasks(
);
