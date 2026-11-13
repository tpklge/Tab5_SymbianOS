// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "driver/i2c_master.h"
#include "runtime.hpp"
#include <mutex>
namespace {
std::mutex lock;
i2c_master_dev_handle_t sensor = nullptr;
bool charge = false, fast = false;
bool read16(uint8_t reg, uint16_t &out) {
  uint8_t bytes[2];
  if (!sensor ||
      i2c_master_transmit_receive(sensor, &reg, 1, bytes, 2, 25) != ESP_OK)
    return false;
  out = uint16_t(bytes[0]) << 8 | bytes[1];
  return true;
}
bool setMode(const std::string &mode) {
  std::lock_guard<std::mutex> guard(lock);
  if (mode != "off" && mode != "normal" && mode != "fast")
    return false;
  auto io = bsp_io_expander1_init();
  if (!io)
    return false;
  const uint32_t qc = IO_EXPANDER_PIN_NUM_5, en = IO_EXPANDER_PIN_NUM_7;
  // Program levels before switching to output; QC request is active low.
  if (esp_io_expander_set_level(io, qc, mode != "fast") != ESP_OK ||
      esp_io_expander_set_level(io, en, mode != "off") != ESP_OK ||
      esp_io_expander_set_dir(io, qc | en, IO_EXPANDER_OUTPUT) != ESP_OK ||
      esp_io_expander_set_output_mode(
          io, qc | en, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL) != ESP_OK)
    return false;
  charge = mode != "off";
  fast = mode == "fast";
  return true;
}
} // namespace
void tab5_setup_power(tab5::Runtime &runtime) {
  runtime.setPowerWriter(setMode);
  runtime.screenWriter=[](int percent) { bsp_display_brightness_set(percent); };
}
void tab5_poll_power(tab5::Runtime &runtime) {
  tab5::PowerInfo info;
  {
    std::lock_guard<std::mutex> guard(lock);
    if (!sensor) {
      i2c_device_config_t config{};
      config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
      config.device_address = 0x41;
      config.scl_speed_hz = 100000;
      if (i2c_master_bus_add_device(bsp_i2c_get_handle(), &config, &sensor) ==
          ESP_OK) {
        const uint8_t continuous[] = {
            0x00, 0x45, 0x27}; // 16 averages, 1.1 ms ADC, continuous shunt+bus.
        if (i2c_master_transmit(sensor, continuous, sizeof(continuous), 25) !=
            ESP_OK) {
          i2c_master_bus_rm_device(sensor);
          sensor = nullptr;
        }
      }
    }
    uint16_t id = 0, bus = 0, shunt = 0;
    if (read16(0xfe, id) && id == 0x5449 && read16(2, bus) &&
        read16(1, shunt)) {
      info.millivolts = int(bus) * 5 / 4;
      // INA226 shunt LSB 2.5 uV, board resistor 5 milliohms.
      info.milliamps = int(int16_t(shunt)) / 2;
      info.available = info.millivolts >= 5800 && info.millivolts <= 8600;
      info.percent =
          info.available ? tab5::batteryPercent(info.millivolts) : -1;
      info.charging = info.available && info.milliamps < -30;
    } else
      info.error = "INA226 indisponível";
    info.enabled = charge;
    info.fast = fast;
  }
  runtime.setPowerInfo(info);
}
void tab5_restore_charge(tab5::Runtime &runtime) {
  std::string mode;
  if (!runtime.storage().read("/config/public/charge", mode))
    mode = "normal";
  // Do not request charging if the monitor is unavailable/outside battery
  // range.
  if (runtime.powerInfo().available)
    setMode(mode);
}
