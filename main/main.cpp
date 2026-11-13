// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "keyboard.hpp"
#include "runtime.hpp"
#include "browser_profile.hpp"
#include "sdmmc_cmd.h"
#include "diskio_sdmmc.h"
#include "ui.hpp"
#include "boot_screen.hpp"
#include <cstdlib>
#include <ctime>
#include <memory>
using namespace tab5;
bool tab5_read_clock(int64_t &);
bool tab5_write_clock(int64_t);
void tab5_signal(bool);
void tab5_setup_media(Media &);
void tab5_setup_power(Runtime &);
void tab5_setup_system(Runtime &);
void tab5_confirm_runtime(bool);
void tab5_setup_network(Runtime &);
void tab5_setup_radio(Runtime &);
void tab5_setup_mail(Runtime &);
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
  bsp_display_backlight_off();
  BootScreen bootScreen;
  bsp_display_lock(0);
  lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
  bootScreen.show(5,"Preparando o sistema");
  bsp_display_unlock();
  bsp_display_backlight_on();

  tab5_probe_hardware();
  TabKeyboard keyboard;
  keyboard.begin();
  auto storageOwner = std::make_unique<Storage>("/sdcard/SymbianOS");
  auto &storage = *storageOwner;
  auto runtimeOwner = std::make_unique<Runtime>(storage);
  auto &runtime = *runtimeOwner;
  auto bootStage=[&](unsigned percent,const char *stage){bsp_display_lock(0);bootScreen.show(percent,stage);bsp_display_unlock();};
  runtime.bootProgress=[&](unsigned percent){bootStage(40+percent*40/100,"Verificando arquivos do cartao");};
  runtime.entropy=[] {
    uint8_t bytes[16];esp_fill_random(bytes,sizeof(bytes));std::string out;
    const char *hex="0123456789abcdef";for(auto b:bytes) {out+=hex[b>>4];out+=hex[b&15];}return out;
  };
  runtime.memoryReader=[] {
    MemoryInfo m;m.available=true;
    m.internalTotal=heap_caps_get_total_size(MALLOC_CAP_INTERNAL);m.internalFree=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    m.internalLargest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);m.internalMinimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    m.psramTotal=heap_caps_get_total_size(MALLOC_CAP_SPIRAM);m.psramFree=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);m.psramLargest=heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);return m;
  };
  runtime.setClockWriter(tab5_write_clock);
  runtime.setSignal(tab5_signal);
  std::string savedVolume;
  tab5_setup_media(runtime.media());
  tab5_setup_power(runtime);
  tab5_setup_system(runtime);
  tab5_setup_network(runtime);
  tab5_setup_radio(runtime);
  tab5_setup_mail(runtime);
  auto uiOwner = std::make_unique<UI>(runtime);
  auto &ui = *uiOwner;
  bootStage(20,"Montando cartao SD");
  bool mounted = bsp_sdcard_mount() == ESP_OK;
  if (mounted) {
    storage.nativeFatRoot(std::to_string(ff_diskio_get_pdrv_card(bsp_sdcard_get_handle()))+":/SymbianOS");
    bootStage(30,"Conferindo dados do cartao");
    storage.mount();
    auto stage = [&](const char *name) {
      storage.write(
          "/logs/startup.txt",
          std::string("runtime=1.0.0\nbrowser_build=")+tab5::browserProfile::build+"\nstage=" + name + "\nreset_reason=" +
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
    bootStage(40,"Carregando aplicativos");
    if (runtime.boot()) {
      tab5_load_transfer_page(runtime);
      storage.write("/logs/boot.txt", "runtime=1.0.0\nreset_reason=" +
                                          std::to_string(esp_reset_reason()) +
                                          "\n");
    }
  } else
    runtime.error = "microSD ausente ou ilegível";
  if (mounted)
    storage.write("/logs/startup-stage.txt", "runtime=1.0.0\nstage=ui-start\n");
  bootStage(90,"Preparando interface");
  bsp_display_lock(0);
  bool uiHealthy = ui.start();
  bsp_display_unlock();
  if(!uiHealthy && mounted && Releases(storage).restore() && runtime.boot()) {
    tab5_load_transfer_page(runtime);
    bsp_display_lock(0);
    uiHealthy = ui.start();
    bsp_display_unlock();
  }
  runtime.bootProgress={};
  bsp_display_lock(0);bootScreen.release();bsp_display_unlock();
  if (mounted)
    storage.write("/logs/startup-stage.txt", "runtime=1.0.0\nstage=running\n");
  tab5_poll_power(runtime);
  if (mounted)
    tab5_restore_charge(runtime);
  uint64_t check = 0, powerCheck = 0, healthCheck = 15000;
  const uint64_t healthyAfter = esp_timer_get_time()/1000 + 15000;
  while (true) {
    if(runtime.suspended) { vTaskDelay(pdMS_TO_TICKS(20));continue; }
    uint64_t ms = esp_timer_get_time() / 1000;
    if(ms >= healthyAfter) tab5_confirm_runtime(uiHealthy && runtime.ready() && mounted);
    if(ms >= healthCheck && mounted) {
      healthCheck = ms + 60000;
      auto power = runtime.powerInfo();
      storage.write("/logs/health.txt", "runtime=1.0.0\nuptime_ms="+std::to_string(ms)+
          "\nwakeup_cause="+std::to_string(esp_sleep_get_wakeup_cause())+
          "\ninternal_free="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL))+
          "\ninternal_largest="+std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))+
          "\npsram_free="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))+
          "\nbattery_mv="+std::to_string(power.millivolts)+"\nbattery_ma="+std::to_string(power.milliamps)+"\n");
    }
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
        storage.nativeFatRoot(std::to_string(ff_diskio_get_pdrv_card(bsp_sdcard_get_handle()))+":/SymbianOS");
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
