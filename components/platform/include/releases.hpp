// SPDX-License-Identifier: MIT
#pragma once
#include "storage.hpp"
namespace tab5 {
// SD manifests are independent from native OTA slots. Documents are untouched.
class Releases {
  Storage &disk_;
public:
  explicit Releases(Storage &disk) : disk_(disk) {}
  bool validate(const std::string &manifest, std::string &error);
  bool select(std::string &manifest, std::string &error);
  bool confirm(const std::string &manifest);
  bool restore();
  bool stage(const std::string &path, std::string &error);
};
}
