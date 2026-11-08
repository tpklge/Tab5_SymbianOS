// SPDX-License-Identifier: MIT
#pragma once
#include "storage.hpp"
#include <map>
namespace tab5 {
struct Package {
  std::string id, version, title, author, permissions, orientation, manifest,
      archiveHash;
  std::map<std::string, std::string> hashes;
  uint64_t bytes = 0;
};
class Installer {
  Storage &disk_;
  bool parse(const std::string &, Package &);
  bool archive(const std::string &, Package &,
               std::map<std::string, std::string> &);

public:
  std::string error;
  explicit Installer(Storage &disk) : disk_(disk) {}
  bool inspect(const std::string &, Package &);
  bool install(const std::string &, const std::atomic<bool> *cancel = nullptr,
               const std::string &expectedHash = "");
  bool remove(const std::string &, bool eraseData = false);
  bool catalog(std::vector<Package> &);
  bool read(const Package &, const std::string &, std::string &);
  static bool identifier(const std::string &);
};
} // namespace tab5
