// SPDX-License-Identifier: MIT
#pragma once
#include <chrono>
#include <functional>
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#else
#include <condition_variable>
#include <mutex>
#include <thread>
#endif
namespace tab5 {
// Unlike usleep below one tick, this always blocks the ESP task and lets
// lower-priority tasks (including idle/watchdog and startup) execute.
inline void workerWait(std::chrono::microseconds delay) {
#ifdef ESP_PLATFORM
  const uint64_t tickUs = uint64_t(portTICK_PERIOD_MS) * 1000;
  const auto us = delay.count();
  vTaskDelay(us > 0 ? TickType_t((uint64_t(us) + tickUs - 1) / tickUs) : 1);
#else
  std::this_thread::sleep_for(delay);
#endif
}
// One persistent task: a click never allocates another task stack.
class Executor {
  std::function<void()> job_;
  bool active_ = false, stop_ = false;
#ifdef ESP_PLATFORM
  TaskHandle_t task_ = nullptr;
  SemaphoreHandle_t wake_ = nullptr, done_ = nullptr;
  static void run(void *arg) {
    auto *self = static_cast<Executor *>(arg);
    for (;;) {
      xSemaphoreTake(self->wake_, portMAX_DELAY);
      if (self->stop_) {
        xSemaphoreGive(self->done_);
        vTaskDelete(nullptr);
      }
      self->job_();
      xSemaphoreGive(self->done_);
    }
  }
#else
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable wake_, done_;
  bool pending_ = false, finished_ = false;
#endif
public:
  explicit Executor(unsigned stack = 65536, unsigned priority = 3) {
#ifdef ESP_PLATFORM
    wake_ = xSemaphoreCreateBinary();
    done_ = xSemaphoreCreateBinary();
    if (wake_ && done_ &&
        xTaskCreate(run, "symbian-worker", stack, this, priority, &task_) !=
            pdPASS)
      task_ = nullptr;
#else
    (void)stack;
    (void)priority;
    thread_ = std::thread([this] {
      std::unique_lock<std::mutex> lock(mutex_);
      for (;;) {
        wake_.wait(lock, [this] { return pending_ || stop_; });
        if (stop_)
          return;
        pending_ = false;
        lock.unlock();
        job_();
        lock.lock();
        finished_ = true;
        done_.notify_one();
      }
    });
#endif
  }
  bool submit(std::function<void()> job) {
    if (active_)
      return false;
#ifdef ESP_PLATFORM
    if (!task_)
      return false;
    job_ = std::move(job);
    active_ = true;
    xSemaphoreGive(wake_);
#else
    std::lock_guard<std::mutex> lock(mutex_);
    job_ = std::move(job);
    active_ = true;
    finished_ = false;
    pending_ = true;
    wake_.notify_one();
#endif
    return true;
  }
  bool joinable() const { return active_; }
  void join() {
    if (!active_)
      return;
#ifdef ESP_PLATFORM
    xSemaphoreTake(done_, portMAX_DELAY);
#else
    std::unique_lock<std::mutex> lock(mutex_);
    done_.wait(lock, [this] { return finished_; });
#endif
    job_ = {};
    active_ = false;
  }
  ~Executor() {
    join();
#ifdef ESP_PLATFORM
    if (task_) {
      stop_ = true;
      xSemaphoreGive(wake_);
      xSemaphoreTake(done_, portMAX_DELAY);
    }
    if (wake_)
      vSemaphoreDelete(wake_);
    if (done_)
      vSemaphoreDelete(done_);
#else
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stop_ = true;
      wake_.notify_one();
    }
    thread_.join();
#endif
  }
};
} // namespace tab5
