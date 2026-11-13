// SPDX-License-Identifier: MIT
#pragma once
#include "storage.hpp"
#include <chrono>
namespace tab5 {
class Lock {
  Storage &disk_;
  std::string salt_, verifier_;
  std::atomic<bool> enabled_{false};
  bool invalid_ = false;
  std::atomic<bool> locked_{false};
  unsigned attempts_ = 0;
  std::chrono::steady_clock::time_point until_{};
  static std::string derive(const std::string &, const std::string &);
public:
  explicit Lock(Storage &s):disk_(s) {}
  void load();
  void bootReset() { locked_=false; }
  bool enabled() const { return enabled_; }
  bool locked() const { return locked_; }
  void engage() { if(enabled_) locked_=true; }
  unsigned wait() const;
  bool unlock(const std::string &pin);
  bool configure(const std::string &oldPin, const std::string &newPin, const std::string &salt);
};
}
