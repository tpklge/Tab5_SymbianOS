// SPDX-License-Identifier: MIT
#include "storage.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#ifndef ESP_PLATFORM
#include <sys/statvfs.h>
#else
#include "esp_vfs_fat.h"
#endif
#include <unistd.h>
static int pathInfo(const char *path, struct stat *info) {
#ifdef ESP_PLATFORM
  // The mounted FAT filesystem has no symbolic links. Host must use lstat.
  return stat(path, info);
#else
  return lstat(path, info);
#endif
}
namespace tab5 {
Storage::Storage(std::string root, uint64_t limit, uint64_t reserve)
    : root_(std::move(root)), limit_(limit), reserve_(reserve) {
  while (root_.size() > 1 && root_.back() == '/')
    root_.pop_back();
}
static bool suffix(const std::string &s, const char *t) {
  size_t n = strlen(t);
  return s.size() >= n && s.compare(s.size() - n, n, t) == 0;
}
bool Storage::validPath(const std::string &p) {
  if (p.empty() || p[0] != '/' || p.size() > 240 || p.back() == '/')
    return false;
  if (std::count(p.begin(), p.end(), '/') > 17)
    return false;
  size_t start = 1;
  while (start < p.size()) {
    size_t end = p.find('/', start);
    if (end == std::string::npos)
      end = p.size();
    std::string part = p.substr(start, end - start);
    if (part.empty() || part == "." || part == ".." || part.size() > 100 ||
        part.back() == '.' || part.back() == ' ' ||
        part.find(".t5-") != std::string::npos)
      return false;
    for (unsigned char c : part)
      if (c < 32 || c == 127 || strchr("\\:*?\"<>|~", c))
        return false;
    start = end + 1;
  }
  return true;
}
bool Storage::resolve(const std::string &p, std::string &out, bool internal) {
  if (!online_) {
    error = "storage offline";
    return false;
  }
  if (!validPath(p) && !internal) {
    error = "invalid logical path";
    return false;
  }
  struct stat rootStat{};
  if (pathInfo(root_.c_str(), &rootStat) != 0 || !S_ISDIR(rootStat.st_mode) ||
      S_ISLNK(rootStat.st_mode)) {
    invalidate();
    return false;
  }
  out = root_;
  size_t start = 1;
  while (start < p.size()) {
    size_t end = p.find('/', start);
    if (end == std::string::npos)
      end = p.size();
    auto part = p.substr(start, end - start);
    DIR *parent = opendir(out.c_str());
    if (parent) {
      while (auto *entry = readdir(parent)) {
        std::string candidate = entry->d_name;
        auto a = candidate, b = part;
        for (char &c : a)
          if (c >= 'A' && c <= 'Z')
            c += 32;
        for (char &c : b)
          if (c >= 'A' && c <= 'Z')
            c += 32;
        if (a == b && candidate != part) {
          closedir(parent);
          error = "case alias forbidden";
          return false;
        }
      }
      closedir(parent);
    }
    out += '/' + part;
    struct stat st{};
    if (pathInfo(out.c_str(), &st) == 0) {
      if (S_ISLNK(st.st_mode) ||
          (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))) {
        error = "links and special files forbidden";
        return false;
      }
    } else if (errno != ENOENT) {
      error = strerror(errno);
      return false;
    }
    start = end + 1;
  }
  return true;
}
bool Storage::scan(const std::string &rel, std::vector<File> &files,
                   unsigned depth) {
  if (depth == 0)
    scanned_ = 0;
  if (depth > 16 || files.size() > 10000) {
    error = "tree limit exceeded";
    return false;
  }
  DIR *d = opendir((root_ + rel).c_str());
  if (!d) {
    error = strerror(errno);
    return false;
  }
  bool ok = true;
  std::vector<std::string> names;
  while (auto *e = readdir(d)) {
    if (++scanned_ > 20000) {
      error = "too many directory entries";
      ok = false;
      break;
    }
    std::string n = e->d_name;
    if (n == "." || n == "..")
      continue;
    std::string lower = n;
    for (char &c : lower)
      if (c >= 'A' && c <= 'Z')
        c += 32;
    if (std::find(names.begin(), names.end(), lower) != names.end()) {
      error = "case alias";
      ok = false;
      break;
    }
    names.push_back(lower);
    std::string p = rel + "/" + n;
    struct stat st{};
    if (pathInfo((root_ + p).c_str(), &st) || S_ISLNK(st.st_mode)) {
      error = "unreadable tree or symlink";
      ok = false;
      break;
    }
    if (S_ISDIR(st.st_mode)) {
      if (!validPath(p) || !scan(p, files, depth + 1)) {
        ok = false;
        break;
      }
    } else if (S_ISREG(st.st_mode)) {
      if (!validPath(p) && !suffix(p, ".t5-new") && !suffix(p, ".t5-old")) {
        error = "invalid FAT path";
        ok = false;
        break;
      }
      files.push_back({p, storageStatBytes(st.st_size)});
      if (files.size() > 10000) {
        ok = false;
        error = "too many files";
        break;
      }
    } else {
      ok = false;
      error = "special file";
      break;
    }
  }
  closedir(d);
  return ok;
}
bool Storage::reconcile() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<File> files;
  if (!online_ || !scan("", files)) {
    online_ = false;
    return false;
  }
  used_ = 0;
  for (auto &f : files) {
    if (UINT64_MAX - used_ < f.size) {
      error = "size overflow";
      online_ = false;
      return false;
    }
    used_ += f.size;
  }
  return true;
}
bool Storage::recover() {
  if (!recoverRestore())
    return false;
  std::vector<File> files;
  if (!scan("", files))
    return false;
  // An old version wins over an uncommitted temporary. Renames never overwrite
  // it.
  for (auto &f : files)
    if (suffix(f.path, ".t5-old")) {
      std::string dest = root_ + f.path.substr(0, f.path.size() - 7);
      struct stat st{};
      if (stat(dest.c_str(), &st) != 0) {
        if (rename((root_ + f.path).c_str(), dest.c_str()))
          return false;
      } else if (unlink((root_ + f.path).c_str()))
        return false;
    }
  for (auto &f : files)
    if (suffix(f.path, ".t5-new") && unlink((root_ + f.path).c_str()))
      return false;
  return true;
}
bool Storage::mount() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  ++generation_;
  struct stat st{};
  online_ = pathInfo(root_.c_str(), &st) == 0 && S_ISDIR(st.st_mode) &&
            !S_ISLNK(st.st_mode);
  if (!online_) {
    error = "SymbianOS root missing";
    return false;
  }
  if (!reconcile())
    return false;
  if (!maintenance() && !recover()) {
    online_ = false;
    error = "transaction recovery failed";
    return false;
  }
  return reconcile();
}
void Storage::invalidate() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  online_ = false;
  ++generation_;
  error = "card removed or I/O failure";
}
bool Storage::list(std::vector<File> &f) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  f.clear();
  return online_ && scan("", f);
}
bool Storage::read(const std::string &p, std::string &data, size_t max) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  data.clear();
  if (!resolve(p, path))
    return false;
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) {
    const int code=errno;
    error = code == ENOENT ? "not found" : strerror(code);
    // Descriptor/memory pressure is not proof that the SD disappeared.
    if (code==EIO || code==ENODEV || code==ENXIO)invalidate();
    return false;
  }
  char buf[4096];
  bool ok = true;
  while (size_t n = fread(buf, 1, sizeof(buf), f)) {
    if (data.size() + n > max) {
      error = "file read limit";
      ok = false;
      break;
    }
    data.append(buf, n);
  }
  if (ferror(f)) {
    invalidate();
    ok = false;
  }
  fclose(f);
  return ok;
}
bool Storage::write(const std::string &p, const std::string &data,
                    const std::atomic<bool> *cancel) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  if (!resolve(p, path))
    return false;
  struct stat destination{};
  if (pathInfo(path.c_str(), &destination) == 0 &&
      !S_ISREG(destination.st_mode)) {
    error = "write destination must be a regular file";
    return false;
  }
  // Entire transaction is a reservation: no second writer can spend its peak
  // space.
  uint64_t ceiling = limit_ > reserve_ ? limit_ - reserve_ : 0;
  if (used_ > ceiling || data.size() > ceiling - used_) {
    error = "quota exceeded (including operational reserve)";
    return false;
  }
  if (data.size() > physicalFree()) {
    error = "physical space unavailable";
    return false;
  }
  std::string temp = path + ".t5-new", old = path + ".t5-old";
  struct stat st{};
  if (pathInfo(temp.c_str(), &st) == 0 || pathInfo(old.c_str(), &st) == 0) {
    error = "pending transaction: remount required";
    return false;
  }
  if (cancel && cancel->load()) {
    error = "cancelled";
    return false;
  }
  FILE *f = fopen(temp.c_str(), "wb");
  if (!f) {
    error = strerror(errno);
    return false;
  }
  bool ok = fwrite(data.data(), 1, data.size(), f) == data.size() &&
            fflush(f) == 0 && fsync(fileno(f)) == 0;
  if (fclose(f))
    ok = false;
  if (!ok) {
    unlink(temp.c_str());
    invalidate();
    return false;
  }
  if (cancel && cancel->load()) {
    unlink(temp.c_str());
    error = "cancelled";
    return false;
  }
  bool exists = pathInfo(path.c_str(), &st) == 0;
  uint64_t previous = exists && storageStatBytes(st.st_size)>0 ? storageStatBytes(st.st_size) : 0;
  if(previous>used_) {unlink(temp.c_str());error="quota ledger changed: remount required";invalidate();return false;}
  if (exists && rename(path.c_str(), old.c_str())) {
    unlink(temp.c_str());
    invalidate();
    return false;
  }
  if (rename(temp.c_str(), path.c_str())) {
    if (exists)
      rename(old.c_str(), path.c_str());
    invalidate();
    return false;
  }
  if (exists && unlink(old.c_str())) {
    invalidate();
    return false;
  }
  // All mutations use the same mutex; peak was reserved above. Mount/recovery
  // still performs a full audit. External card edits require remounting.
  used_ = used_ - std::min(used_,previous) + data.size();
  return true;
}
bool Storage::remove(const std::string &p) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  if (!resolve(p, path))
    return false;
  struct stat entry{};
  if (pathInfo(path.c_str(), &entry)) {
    error = strerror(errno);
    return false;
  }
  if ((S_ISDIR(entry.st_mode) ? rmdir(path.c_str()) : unlink(path.c_str()))) {
    error = strerror(errno);
    return false;
  }
  if(!S_ISDIR(entry.st_mode) && storageStatBytes(entry.st_size)>0) used_-=std::min(used_,storageStatBytes(entry.st_size));
  return true;
}
} // namespace tab5
