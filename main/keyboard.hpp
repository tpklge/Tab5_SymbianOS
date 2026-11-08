// SPDX-License-Identifier: MIT
#pragma once
#include "driver/i2c_master.h"
#include "input.hpp"
namespace tab5 {
class TabKeyboard {
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t dev_ = nullptr;
  bool connected_ = false;
  uint64_t retry_ = 0;
  bool read(uint8_t, uint8_t &);
  bool write(uint8_t, uint8_t);

public:
  bool begin();
  std::vector<Input> poll(uint64_t);
  bool connected() const { return connected_; }
};
} // namespace tab5
