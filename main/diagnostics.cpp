// SPDX-License-Identifier: MIT
// Read-only hardware probes. An ACK is not a functional audio/RTC validation.
#include "bsp/m5stack_tab5.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <initializer_list>
#include <utility>
void tab5_probe_hardware() {
  auto bus = bsp_i2c_get_handle();
  if (!bus)
    return;
  for (auto probe : {std::make_pair(0x10, "ES8388 output codec"),
                     std::make_pair(0x40, "ES7210 input codec"),
                     std::make_pair(0x32, "RX8130 RTC")}) {
    esp_err_t result = i2c_master_probe(bus, probe.first, 20);
    ESP_LOGI("probe", "%s address 0x%02x: %s", probe.second, probe.first,
             esp_err_to_name(result));
  }
  i2c_master_dev_handle_t rtc = nullptr;
  i2c_device_config_t cfg{};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = 0x32;
  cfg.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus, &cfg, &rtc) == ESP_OK) {
    uint8_t reg = 0x10, raw[7]{};
    esp_err_t result =
        i2c_master_transmit_receive(rtc, &reg, 1, raw, sizeof(raw), 30);
    if (result == ESP_OK)
      ESP_LOGI("probe",
               "RTC raw BCD (not applied to system clock): %02x:%02x:%02x "
               "%02x/%02x/%02x",
               raw[2], raw[1], raw[0], raw[4], raw[5], raw[6]);
    i2c_master_bus_rm_device(rtc);
  }
  // ESP32-C6 transport/firmware must be validated before advertising
  // networking.
  ESP_LOGI("probe", "C6 network: not integrated; no firmware image embedded");
}
