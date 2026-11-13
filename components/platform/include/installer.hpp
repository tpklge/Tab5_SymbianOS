// SPDX-License-Identifier: MIT
#pragma once
#include "storage.hpp"
#include <map>
#include <mutex>
namespace tab5 {
std::recursive_mutex &installerMutex();
struct PackageDependency { uint32_t uid=0; std::string minimum,maximum; };
struct Package {
  std::string id, version, title, author, permissions, orientation, manifest,
      archiveHash;
  std::map<std::string, std::string> hashes;
  std::vector<PackageDependency> dependencies;
  unsigned sisProfile=0, schema=1, migrateFrom=0;
  std::string migration;
  uint32_t uid = 0;
  uint64_t inspectionMs = 0, bytes = 0;
};
class Installer {
  Storage &disk_;
  bool parse(const std::string &, Package &);
  bool sisArchive(const std::string &, Package &, std::map<std::string,std::string> &, const std::atomic<bool> *, bool collect);
  bool lifecycleInstall(const std::string &, const std::atomic<bool> *, const std::string &, bool);
  bool lifecycleRemove(const std::string &, bool);
  bool recoverLifecycle(bool boot=false);
  bool lifecycleFinish(bool restore);
  bool installSis(const std::string &, const std::atomic<bool> *, const std::string &);
  bool archive(const std::string &, Package &,
               std::map<std::string, std::string> &);

public:
  std::string error;
#ifndef ESP_PLATFORM
  std::function<bool(const std::string &)> transactionCheckpoint;
#endif
  bool recoverSis();
  bool recoverSisBoot();
  bool validateSisGraph(const std::vector<Package> &);
  bool pendingSis(std::string &,std::string &);
  bool beginSisTrial(const std::string &);
  bool finishSisTrial(const std::string &,bool);
  bool rollbackSis(const std::string &);
  bool previousSis(const std::string &,Package &);
  bool installDowngrade(const std::string &,const std::atomic<bool> *,const std::string &);
  explicit Installer(Storage &disk) : disk_(disk) {}
  bool inspect(const std::string &, Package &, const std::atomic<bool> *cancel = nullptr);
  bool install(const std::string &, const std::atomic<bool> *cancel = nullptr,
               const std::string &expectedHash = "");
  bool remove(const std::string &, bool eraseData = false);
  bool catalog(std::vector<Package> &);
  bool read(const Package &, const std::string &, std::string &);
  static bool identifier(const std::string &);
};
} // namespace tab5
