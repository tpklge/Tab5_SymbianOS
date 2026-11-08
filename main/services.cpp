// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "esp_log.h"
#include <cmath>
#include <ctime>
#include <initializer_list>
static bool rtcRead(uint8_t reg, uint8_t *bytes, size_t n) {
  auto bus = bsp_i2c_get_handle();
  if (!bus)
    return false;
  i2c_master_dev_handle_t dev = nullptr;
  i2c_device_config_t cfg{};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = 0x32;
  cfg.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK)
    return false;
  bool ok = i2c_master_transmit_receive(dev, &reg, 1, bytes, n, 30) == ESP_OK;
  i2c_master_bus_rm_device(dev);
  return ok;
}
static bool rtcWrite(uint8_t reg, const uint8_t *bytes, size_t n) {
  auto bus = bsp_i2c_get_handle();
  if (!bus || n > 7)
    return false;
  i2c_master_dev_handle_t dev = nullptr;
  i2c_device_config_t cfg{};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = 0x32;
  cfg.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK)
    return false;
  uint8_t buffer[8];
  buffer[0] = reg;
  for (size_t i = 0; i < n; ++i)
    buffer[i + 1] = bytes[i];
  bool ok = i2c_master_transmit(dev, buffer, n + 1, 30) == ESP_OK;
  i2c_master_bus_rm_device(dev);
  return ok;
}
bool tab5_read_clock(int64_t &epoch) {
  uint8_t flag = 0, raw[7];
  if (!rtcRead(0x1d, &flag, 1) || (flag & 0x80) || !rtcRead(0x10, raw, 7))
    return false;
  auto bcd = [](uint8_t b) { return int((b >> 4) * 10 + (b & 15)); };
  for (int i : {0, 1, 2, 4, 5, 6})
    if ((raw[i] & 15) > 9)
      return false;
  int y = 2000 + bcd(raw[6]), m = bcd(raw[5] & 31), d = bcd(raw[4] & 63),
      h = bcd(raw[2] & 63), minute = bcd(raw[1] & 127), sec = bcd(raw[0] & 127);
  if (m < 1 || m > 12 || d < 1 || h > 23 || minute > 59 || sec > 59)
    return false;
  auto leap = [](int y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
  };
  int months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (d > months[m - 1] + (m == 2 && leap(y)))
    return false;
  int64_t days = 0;
  for (int a = 1970; a < y; ++a)
    days += 365 + leap(a);
  for (int a = 1; a < m; ++a)
    days += months[a - 1] + (a == 2 && leap(y));
  days += d - 1;
  epoch = ((days * 24 + h) * 60 + minute) * 60 + sec;
  return true;
}
bool tab5_write_clock(int64_t epoch) {
  time_t clock = epoch;
  tm t{};
  if (!gmtime_r(&clock, &t) || t.tm_year < 100 || t.tm_year > 199)
    return false;
  auto bcd = [](int x) { return uint8_t((x / 10) * 16 + x % 10); };
  uint8_t raw[] = {bcd(t.tm_sec),           bcd(t.tm_min),  bcd(t.tm_hour),
                   uint8_t(1 << t.tm_wday), bcd(t.tm_mday), bcd(t.tm_mon + 1),
                   bcd(t.tm_year - 100)};
  if (!rtcWrite(0x10, raw, 7))
    return false;
  uint8_t zero = 0, control = 0;
  if (!rtcRead(0x1f, &control, 1))
    return false;
  control |= 0x30;
  return rtcWrite(0x1e, &zero, 1) && rtcWrite(0x1f, &control, 1) &&
         rtcWrite(0x1d, &zero, 1);
}
