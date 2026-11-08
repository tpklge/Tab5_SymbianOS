// SPDX-License-Identifier: MIT
// Protocol reference: official M5Stack Tab5 Keyboard UserDemo, pinned in docs.
#include "keyboard.hpp"
#include "driver/gpio.h"
namespace tab5 {
bool TabKeyboard::read(uint8_t reg, uint8_t &value) {
  return dev_ &&
         i2c_master_transmit_receive(dev_, &reg, 1, &value, 1, 15) == ESP_OK;
}
bool TabKeyboard::write(uint8_t reg, uint8_t value) {
  uint8_t buf[] = {reg, value};
  return i2c_master_transmit(dev_, buf, 2, 15) == ESP_OK;
}
bool TabKeyboard::begin() {
  i2c_master_bus_config_t cfg{};
  cfg.i2c_port = I2C_NUM_1;
  cfg.sda_io_num = GPIO_NUM_0;
  cfg.scl_io_num = GPIO_NUM_1;
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  cfg.flags.enable_internal_pullup = true;
  if (i2c_new_master_bus(&cfg, &bus_) != ESP_OK)
    return false;
  i2c_device_config_t d{};
  d.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  d.device_address = 0x6d;
  d.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus_, &d, &dev_) != ESP_OK)
    return false;
  gpio_config_t irq{};
  irq.pin_bit_mask = 1ULL << 50;
  irq.mode = GPIO_MODE_INPUT;
  irq.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&irq);
  return true;
}
std::vector<Input> TabKeyboard::poll(uint64_t now) {
  std::vector<Input> out;
  if (!dev_)
    return out;
  if (!connected_) {
    if (now < retry_)
      return out;
    retry_ = now + 1000;
    uint8_t version;
    if (!read(0xfe, version) || !write(0x10, 0) || !write(0, 1))
      return out;
    connected_ = true;
  }
  // Polling is intentional: all bounded I2C operations run outside any ISR and
  // UI lock.
  uint8_t count = 0;
  if (!read(2, count)) {
    connected_ = false;
    retry_ = now + 1000;
    out.push_back({"reset", "", true});
    return out;
  }
  if (count > 64)
    count = 64;
  for (unsigned n = 0; n < count; ++n) {
    uint8_t raw;
    if (!read(0x20, raw)) {
      connected_ = false;
      retry_ = now + 1000;
      out.push_back({"reset", "", true});
      break;
    }
    if (raw == 0xff)
      break;
    unsigned row = (raw >> 4) & 7, col = raw & 15;
    if (row < 5 && col < 14)
      out.push_back(
          {"matrix:" + std::to_string(row * 14 + col), "", (raw & 128) != 0});
  }
  return out;
}
} // namespace tab5
