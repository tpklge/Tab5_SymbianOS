// SPDX-License-Identifier: MIT
#include "input.hpp"
#include <cstdlib>
#include "text_lines.hpp"
namespace tab5 {
void Keyboard::reset() {
  held_.fill(false);
  repeat_ = -1;
  accent_.clear();
  symLatched_ = symUsed_ = repeatSymbol_ = false;
}
Keyboard::Keyboard() {
  normal_[0] = "escape";
  normal_[28] = "tab";
  normal_[55] = "enter";
  normal_[53] = "up";
  normal_[66] = "left";
  normal_[67] = "down";
  normal_[68] = "right";
}
bool Keyboard::configure(const std::string &data) {
  std::array<std::string, 70> n{}, s{}, y{};
  std::array<bool, 70> seen{};
  TextLines in(data);
  std::string line;
  unsigned count = 0;
  while (nextLine(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    auto eq = line.find('=');
    if (eq == std::string::npos)
      return false;
    auto id = line.substr(0, eq);
    if (id.empty() || id.find_first_not_of("0123456789") != std::string::npos)
      return false;
    unsigned k = unsigned(strtoul(id.c_str(), nullptr, 10));
    if (k >= 70 || seen[k])
      return false;
    seen[k] = true;
    ++count;
    auto value = line.substr(eq + 1);
    auto p = value.find('|'),
         q = value.find('|', p == std::string::npos ? 0 : p + 1);
    if (p == std::string::npos || q == std::string::npos)
      return false;
    n[k] = value.substr(0, p);
    s[k] = value.substr(p + 1, q - p - 1);
    y[k] = value.substr(q + 1);
    if (n[k].empty() || n[k].size() > 16 || s[k].size() > 16 ||
        y[k].size() > 16)
      return false;
  }
  if (count != 70)
    return false;
  normal_ = n;
  shift_ = s;
  symbol_ = y;
  reset();
  return true;
}
Input Keyboard::translate(unsigned k, bool down, bool symbol) {
  auto held = [&](const char *key) {
    for (unsigned i = 0; i < 70; ++i)
      if (normal_[i] == key && held_[i])
        return true;
    return false;
  };
  Input e;
  e.key = normal_[k];
  e.down = down;
  e.shift = held("shift");
  e.ctrl = held("ctrl");
  e.alt = held("alt");
  static const std::string controls = "|escape|tab|delete|backspace|enter|up|"
                                      "down|left|right|ctrl|shift|sym|alt|";
  if (!e.key.empty() && controls.find("|" + e.key + "|") == std::string::npos) {
    e.text = e.key;
    if (e.shift && !shift_[k].empty())
      e.text = shift_[k];
    if ((held("sym") || symLatched_ || symbol) && !symbol_[k].empty())
      e.text = symbol_[k];
  }
  if (e.ctrl || e.alt)
    e.text.clear();
  return e;
}
std::vector<Input> Keyboard::matrix(unsigned row, unsigned col, bool down,
                                    uint64_t ms) {
  if (row >= 5 || col >= 14)
    return {};
  unsigned k = row * 14 + col;
  if (held_[k] == down)
    return {};
  held_[k] = down;
  if (normal_[k] == "sym") {
    if (down)
      symUsed_ = false;
    else if (!symUsed_)
      symLatched_ = !symLatched_;
  }
  auto e = translate(k, down);
  if (down && e.key != "sym" && e.key != "shift" && e.key != "ctrl" &&
      e.key != "alt") {
    repeatSymbol_ = symLatched_ && !e.text.empty();
    if (!e.text.empty())
      symLatched_ = false;
    for (unsigned i = 0; i < 70; ++i)
      if (normal_[i] == "sym" && held_[i])
        symUsed_ = true;
    repeat_ = int(k);
    due_ = ms + 500;
  } else if (!down && repeat_ == int(k))
    repeat_ = -1;
  return {e};
}
std::vector<Input> Keyboard::tick(uint64_t ms) {
  if (repeat_ < 0 || ms < due_)
    return {};
  due_ = ms + 60;
  auto e = translate(repeat_, true, repeatSymbol_);
  e.repeat = true;
  return {e};
}
} // namespace tab5
