// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <memory>
#include <string>
namespace tab5 {
class Digest {
  struct Impl;
  std::unique_ptr<Impl> state_;

public:
  Digest();
  ~Digest();
  void update(const void *, size_t);
  std::string finish();
};
} // namespace tab5
