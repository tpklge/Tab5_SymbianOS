// SPDX-License-Identifier: MIT
#include "storage.hpp"
#include <chrono>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
namespace tab5 {
bool Storage::canFit(uint64_t n) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return online_ && room(n);
}
bool Storage::readRange(const std::string &p, uint64_t offset, size_t n,
                        std::string &out) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  out.clear();
  if (n > 65536 || offset > 4000000000ULL || !resolve(p, path))
    return false;
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) {
    error = "cannot read stream";
    return false;
  }
  bool ok = fseek(f, offset, SEEK_SET) == 0;
  out.resize(n);
  size_t got = ok ? fread(out.data(), 1, n, f) : 0;
  ok = ok && !ferror(f);
  out.resize(got);
  fclose(f);
  if (!ok)
    error = "stream read failed";
  return ok;
}
bool Storage::append(const std::string &p, const std::string &data) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  if (data.size() > 65536 || !resolve(p, path))
    return false;
  uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count();
  if (now - streamCheckMs_ >= 1000) {
    if (!reconcile())
      return false;
    streamCheckMs_ = now;
  }
  uint64_t ceiling = limit_ > reserve_ ? limit_ - reserve_ : 0;
  if (used_ > ceiling || data.size() > ceiling - used_ ||
      data.size() > physicalFree()) {
    error = "stream quota or physical space exhausted";
    return false;
  }
  FILE *f = fopen(path.c_str(), "ab");
  if (!f) {
    error = "cannot append stream";
    return false;
  }
  struct stat before{}, after{};
  fstat(fileno(f), &before);
  bool ok = fwrite(data.data(), 1, data.size(), f) == data.size() &&
            fflush(f) == 0 && fsync(fileno(f)) == 0;
  if (fstat(fileno(f), &after) == 0 && after.st_size >= before.st_size)
    used_ += after.st_size - before.st_size;
  if (fclose(f))
    ok = false;
  if (!ok)
    error = "stream full or unavailable";
  return ok;
}
bool Storage::patch(const std::string &p, uint64_t offset,
                    const std::string &data) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  if (data.size() > 65536 || offset > 4000000000ULL || !resolve(p, path))
    return false;
  FILE *f = fopen(path.c_str(), "rb+");
  if (!f)
    return false;
  bool ok = fseek(f, 0, SEEK_END) == 0;
  long length = ftell(f);
  ok = ok && length >= 0 && offset + data.size() <= uint64_t(length) &&
       fseek(f, offset, SEEK_SET) == 0;
  ok = ok && fwrite(data.data(), 1, data.size(), f) == data.size() &&
       fflush(f) == 0 && fsync(fileno(f)) == 0;
  if (fclose(f))
    ok = false;
  if (!ok)
    error = "stream patch failed";
  return ok;
}
bool Storage::removeTree(const std::string &p) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (p == "/" || p.size() < 2 || !validPath(p))
    return false;
  std::vector<File> children;
  if (!listDir(p, children))
    return false;
  for (const auto &f : children)
    if (!(f.directory ? removeTree(f.path) : remove(f.path)))
      return false;
  return remove(p);
}
} // namespace tab5
