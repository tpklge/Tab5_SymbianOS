// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "keyboard.hpp"
#include "runtime.hpp"
#include "sdmmc_cmd.h"
#include "ui.hpp"
#include <cstdlib>
#include <ctime>
#include <memory>
using namespace tab5;
bool tab5_read_clock(int64_t &);
bool tab5_write_clock(int64_t);
void tab5_signal(bool);
void tab5_setup_media(Media &);
void tab5_setup_power(Runtime &);
void tab5_setup_network(Runtime &);
void tab5_load_transfer_page(Runtime &);
void tab5_poll_power(Runtime &);
void tab5_restore_charge(Runtime &);
void tab5_probe_hardware();
bool tab5_graphics_memory_available();
void tab5_deliver(UI *, const std::vector<Input> &);
extern "C" void app_main() {
  esp_chip_info_t chip{};
  esp_chip_info(&chip);
  ESP_LOGI("runtime", "P4 rev=%u cores=%u PSRAM=%u", chip.revision, chip.cores,
           (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
  setenv("TZ", "<-04>4", 1);
  tzset(); // Default civil time is America/Cuiaba; no fabricated clock.
  if (!tab5_graphics_memory_available()) {
    ESP_LOGE("runtime", "PSRAM unavailable for graphical allocator");
    return;
  }
  bsp_display_cfg_t displayConfig{};
  displayConfig.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  displayConfig.lvgl_port_cfg.task_stack = 16384;
  displayConfig.buffer_size = 720 * 50;
  displayConfig.double_buffer = true;
  displayConfig.flags.buff_dma = true;
  displayConfig.flags.buff_spiram = true;
  displayConfig.flags.sw_rotate = true;
  auto *display = bsp_display_start_with_config(&displayConfig);
  if (!display) {
    ESP_LOGE("runtime", "display initialization failed");
    return;
  }
  bsp_display_lock(0);
  lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
  bsp_display_unlock();
  bsp_display_backlight_on();

  tab5_probe_hardware();
  TabKeyboard keyboard;
  keyboard.begin();
  auto storageOwner = std::make_unique<Storage>("/sdcard/SymbianOS");
  auto &storage = *storageOwner;
  auto runtimeOwner = std::make_unique<Runtime>(storage);
  auto &runtime = *runtimeOwner;
  runtime.setClockWriter(tab5_write_clock);
  runtime.setSignal(tab5_signal);
  std::string savedVolume;
  tab5_setup_media(runtime.media());
  tab5_setup_power(runtime);
  tab5_setup_network(runtime);
  auto uiOwner = std::make_unique<UI>(runtime);
  auto &ui = *uiOwner;
  bool mounted = bsp_sdcard_mount() == ESP_OK;
  if (mounted) {
    storage.mount();
    auto stage = [&](const char *name) {
      storage.write(
          "/logs/startup.txt",
          std::string("runtime=0.7.2\nstage=") + name + "\nreset_reason=" +
              std::to_string(esp_reset_reason()) + "\nmain_stack_free_bytes=" +
              std::to_string(uxTaskGetStackHighWaterMark(nullptr)) +
              "\ninternal_free_bytes=" +
              std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) +
              "\n");
    };
    stage("sd-mounted");
    std::string format;
    int64_t epoch;
    if (storage.read("/config/public/rtc_format", format) && format == "utc" &&
        tab5_read_clock(epoch))
      runtime.setClock(epoch);
    if (storage.read("/config/public/volume", savedVolume, 4))
      runtime.media().setVolume(atoi(savedVolume.c_str()));
    stage("runtime-boot");
    if (runtime.boot()) {
      tab5_load_transfer_page(runtime);
      storage.write("/logs/boot.txt", "runtime=0.7.2\nreset_reason=" +
                                          std::to_string(esp_reset_reason()) +
                                          "\n");
    }
  } else
    runtime.error = "microSD ausente ou ilegível";
  if (mounted)
    storage.write("/logs/startup-stage.txt", "runtime=0.7.2\nstage=ui-start\n");
  bsp_display_lock(0);
  ui.start();
  bsp_display_unlock();
  if (mounted)
    storage.write("/logs/startup-stage.txt", "runtime=0.7.2\nstage=running\n");
  tab5_poll_power(runtime);
  if (mounted)
    tab5_restore_charge(runtime);
  uint64_t check = 0, powerCheck = 0;
  while (true) {
    uint64_t ms = esp_timer_get_time() / 1000;
    auto events = keyboard.poll(ms);
    if (ms >= powerCheck) {
      powerCheck = ms + 2000;
      tab5_poll_power(runtime);
    }
    if (ms >= check) {
      check = ms + 1500;
      if (mounted) {
        bool present = true;
        if (storage.probe(
                [] {
                  auto *card = bsp_sdcard_get_handle();
                  return card && sdmmc_get_status(card) == ESP_OK;
                },
                present) &&
            !present) {
          mounted = false;
          bsp_display_lock(0);
          bsp_sdcard_unmount();
          bsp_display_unlock();
          events.push_back({"card_removed", "", true});
        }
      } else if (bsp_sdcard_mount() == ESP_OK) {
        mounted = true;
        events.push_back({"card_inserted", "", true});
      }
    }
    // LVGL owns rendering/input; Lua and filesystem callbacks run on its
    // worker.

    tab5_deliver(&ui, events);
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}
// The producer copies keyboard events; the LVGL task never performs I2C.
static QueueHandle_t event_queue = nullptr;
struct WireInput {
  char key[24], text[16];
  bool down, shift, ctrl, alt, repeat;
};
static UI *active_ui = nullptr;
void tab5_deliver(UI *ui, const std::vector<Input> &events) {
  if (!event_queue) {
    event_queue = xQueueCreate(96, sizeof(WireInput));
    if (!event_queue) {
      ESP_LOGE("runtime", "input queue allocation failed");
      return;
    }
    active_ui = ui;
    bsp_display_lock(0);
    lv_timer_create(
        [](lv_timer_t *) {
          WireInput w;
          for (unsigned i = 0; i < 32 && xQueueReceive(event_queue, &w, 0);
               ++i) {
            active_ui->input(
                {w.key, w.text, w.down, w.shift, w.ctrl, w.alt, w.repeat});
          }
          active_ui->poll();
        },
        20, nullptr);
    bsp_display_unlock();
  }
  for (auto &e : events) {
    WireInput w{};
    snprintf(w.key, sizeof(w.key), "%s", e.key.c_str());
    snprintf(w.text, sizeof(w.text), "%s", e.text.c_str());
    w.down = e.down;
    w.shift = e.shift;
    w.ctrl = e.ctrl;
    w.alt = e.alt;
    w.repeat = e.repeat;
    if (xQueueSend(event_queue, &w, 0) != pdTRUE) {
      xQueueReset(event_queue);
      WireInput reset{};
      snprintf(reset.key, sizeof(reset.key), "reset");
      reset.down = true;
      xQueueSend(event_queue, &reset, 0);
      break;
    }
  }
}
