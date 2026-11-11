#include "NativeI2cRuntime.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "NativeI2cRuntime.h".
 */

namespace {
    struct STOP_TASK {};
    thread_local TaskHandle_t current_task = nullptr;
    std::mutex tasks_mutex;
    std::vector<TaskHandle_t> tasks;
    const auto started = std::chrono::steady_clock::now();
}

TwoWire Wire;

SemaphoreHandle_t xSemaphoreCreateMutexStatic(
    StaticSemaphore_t* storage
) { storage->tokens = 1; return storage; }

SemaphoreHandle_t xSemaphoreCreateBinaryStatic(
    StaticSemaphore_t* storage
) { storage->tokens = 0; return storage; }

BaseType_t xSemaphoreTake(
    SemaphoreHandle_t semaphore,
    TickType_t ticks
) {
    std::unique_lock<std::mutex> lock(semaphore->mutex);
    if (ticks == portMAX_DELAY) semaphore->changed.wait(lock, [&] { return semaphore->tokens != 0; });
    else if (!semaphore->changed.wait_for(lock, std::chrono::milliseconds(ticks),
            [&] { return semaphore->tokens != 0; })) return pdFALSE;
    --semaphore->tokens;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(
    SemaphoreHandle_t semaphore
) {
    std::lock_guard<std::mutex> lock(semaphore->mutex);
    semaphore->tokens = 1;
    semaphore->changed.notify_one();
    return pdTRUE;
}

BaseType_t xTaskCreate(
    void (*entry)(void*),
    const char*,
    uint32_t,
    void* context,
    UBaseType_t,
    TaskHandle_t* out
) {
    TaskHandle_t task = new NATIVE_TASK;
    if (out) *out = task;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex);
        tasks.push_back(task);
    }
    task->thread = std::thread([=] {
        current_task = task;
        try { entry(context); } catch (const STOP_TASK&) {}
    });
    return pdPASS;
}

TaskHandle_t xTaskGetCurrentTaskHandle(
) { return current_task; }

void xTaskNotifyGive(
    TaskHandle_t task
) {
    std::lock_guard<std::mutex> lock(task->mutex);
    ++task->notifications;
    task->changed.notify_one();
}

uint32_t ulTaskNotifyTake(
    BaseType_t clear,
    TickType_t ticks
) {
    TaskHandle_t task = current_task;
    std::unique_lock<std::mutex> lock(task->mutex);
    const auto ready = [&] { return task->notifications || task->stop; };
    if (ticks == portMAX_DELAY) task->changed.wait(lock, ready);
    else task->changed.wait_for(lock, std::chrono::milliseconds(ticks), ready);
    if (task->stop) throw STOP_TASK{};
    const unsigned count = task->notifications;
    if (clear) task->notifications = 0;
    else if (count) --task->notifications;
    return count;
}

void vTaskDelay(
    TickType_t ticks
) { std::this_thread::sleep_for(std::chrono::milliseconds(ticks)); }

void vTaskDelete(
    TaskHandle_t
) { throw STOP_TASK{}; }

UBaseType_t uxTaskGetStackHighWaterMark(
    TaskHandle_t
) { return 0; } // A pilha da thread nativa não representa a pilha FreeRTOS.

bool xPortInIsrContext(
) { return false; }

uint32_t micros(
) {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count());
}

uint32_t millis(
) {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count());
}

void StopNativeI2cTasks(
) {
    for (TaskHandle_t task : tasks) {
        std::lock_guard<std::mutex> lock(task->mutex);
        task->stop = true;
        task->changed.notify_one();
    }
    for (TaskHandle_t task : tasks) {
        task->thread.join();
        delete task;
    }
    tasks.clear();
}
