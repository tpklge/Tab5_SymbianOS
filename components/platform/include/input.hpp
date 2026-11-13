// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace tab5 {
struct Input {
  std::string key, text;
  bool down = true, shift = false, ctrl = false, alt = false, repeat = false;
};
class Keyboard {
  std::array<bool, 70> held_{};
  std::array<std::string, 70> normal_{}, shift_{}, symbol_{};
  int repeat_ = -1;
  bool symLatched_ = false, symUsed_ = false, repeatSymbol_ = false;
  uint64_t due_ = 0;
  std::string accent_;
  Input translate(unsigned, bool, bool symbol = false);

public:
  Keyboard();
  bool configure(const std::string &);
  std::vector<Input> matrix(unsigned row, unsigned col, bool down, uint64_t ms);
  std::vector<Input> tick(uint64_t ms);
  void reset();
};
} // namespace tab5
