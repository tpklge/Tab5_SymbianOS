// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
namespace tab5 {
// ESP-IDF FAT VFS assigns unsigned FSIZE_t to signed 32-bit off_t.
inline uint64_t storageStatBytes(int64_t value) {
#ifdef ESP_PLATFORM
  return uint32_t(value);
#else
  return uint64_t(value);
#endif
}

struct File {
  std::string path;
  uint64_t size;
  bool directory = false;
};
class Storage {
  std::string root_;
  std::string fatRoot_;
#ifndef ESP_PLATFORM
  std::function<uint64_t()> freeProvider_;
#endif
  uint64_t limit_, reserve_, used_ = 0;
  std::atomic<bool> online_{false};
  std::atomic<uint64_t> generation_{0};
  size_t scanned_ = 0;
  uint64_t streamCheckMs_ = 0;
  mutable std::recursive_mutex mutex_;
  bool resolve(const std::string &, std::string &, bool internal = false);
  bool scan(const std::string &, std::vector<File> &, unsigned depth = 0);
  bool recover();
  bool recoverRestore();
  bool room(uint64_t);
  bool parents(const std::string &);

public:
  void nativeFatRoot(const std::string &path){std::lock_guard<std::recursive_mutex> lock(mutex_);fatRoot_=path;}
  std::string error;
  std::string lastError() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return error;
  }
  void restoreDiagnosticError(const std::string &text) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error = text;
  }
  explicit Storage(std::string root, uint64_t limit = 4000000000ULL,
                   uint64_t reserve = 64ULL * 1024 * 1024);
  static bool validPath(const std::string &);
  bool probe(const std::function<bool()> &callback, bool &present) {
    if (!mutex_.try_lock())
      return false;
    present = callback();
    if (!present)
      { online_ = false; ++generation_; }
    mutex_.unlock();
    return true;
  }
  bool mount();
  bool reconcile();
  void pause() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    online_ = false;
    ++generation_;
  }
  void invalidate();
  bool online() const { return online_; }
  uint64_t generation() const { return generation_.load(); }
  uint64_t used() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return used_;
  }
  uint64_t limit() const { return limit_; }
  bool maintenance() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return used_ > limit_;
  }
  bool read(const std::string &, std::string &, size_t maximum = 1024 * 1024);
  bool readWithError(const std::string &path,std::string &out,std::string &why,size_t maximum=1024*1024) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    bool ok=read(path,out,maximum);why=ok?"":error;return ok;
  }
  bool write(const std::string &, const std::string &,
             const std::atomic<bool> *cancel = nullptr);
  bool readRange(const std::string &, uint64_t, size_t, std::string &);
  bool append(const std::string &, const std::string &);
  bool appendAt(const std::string &, const std::string &, uint64_t, uint64_t);
  // Bind a trusted native operation to one mounted card while holding the
  // storage lock. The callback may use Storage's recursive operations.
  bool atGeneration(uint64_t expected, const std::function<bool()> &operation) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!online_ || generation_.load()!=expected) {error="card changed during transaction";return false;}
    return operation();
  }
  bool pathExists(const std::string &,bool &);
  bool fileSize(const std::string &, uint64_t &);
  bool patch(const std::string &, uint64_t, const std::string &);
  bool canFit(uint64_t);
  bool removeAt(const std::string &path,uint64_t generation) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return generation==generation_ && remove(path);
  }
  bool removeTree(const std::string &);
  bool remove(const std::string &);
  bool list(std::vector<File> &);
  bool listDir(const std::string &, std::vector<File> &);
  bool prepare(const std::string &);
  bool mkdir(const std::string &);
  bool move(const std::string &, const std::string &);
  bool copy(const std::string &, const std::string &,
            const std::atomic<bool> *cancel = nullptr);
  bool backup(const std::string &, const std::atomic<bool> *cancel = nullptr);
  bool restore(const std::string &, const std::atomic<bool> *cancel = nullptr);
  uint64_t reserve() const { return reserve_; }
  uint64_t physicalFree() const;
#ifndef ESP_PLATFORM
  void setFreeSpaceProvider(std::function<uint64_t()> fn) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    freeProvider_ = fn;
  }
#endif
};
std::string sha256(const std::string &);
} // namespace tab5
