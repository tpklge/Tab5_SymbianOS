// SPDX-License-Identifier: MIT
#include "digest.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_vfs_fat.h"
#else
#include <sys/statvfs.h>
#endif
namespace tab5 {
static int info(const char *p, struct stat *s) {
#ifdef ESP_PLATFORM
  return stat(p, s);
#else
  return lstat(p, s);
#endif
}
static bool cancelled(const std::atomic<bool> *c) { return c && c->load(); }
uint64_t Storage::physicalFree() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
#ifndef ESP_PLATFORM
  if (freeProvider_)
    return freeProvider_();
#endif
#ifdef ESP_PLATFORM
  uint64_t total = 0, free = 0;
  return esp_vfs_fat_info("/sdcard", &total, &free) == ESP_OK ? free : 0;
#else
  struct statvfs s{};
  return statvfs(root_.c_str(), &s) == 0 ? uint64_t(s.f_bavail) * s.f_frsize
                                         : 0;
#endif
}
bool Storage::room(uint64_t bytes) {
  if (!reconcile())
    return false;
  uint64_t ceiling = limit_ > reserve_ ? limit_ - reserve_ : 0;
  if (used_ > ceiling || bytes > ceiling - used_) {
    error = "quota exceeded (transaction peak and reserve)";
    return false;
  }
  if (bytes > physicalFree()) {
    error = "physical space unavailable";
    return false;
  }
  return true;
}
bool Storage::parents(const std::string &p) {
  size_t pos = 1;
  while ((pos = p.find('/', pos)) != std::string::npos) {
    auto dir = p.substr(0, pos);
    std::string real;
    if (!resolve(dir, real))
      return false;
    struct stat st{};
    if (info(real.c_str(), &st) == 0) {
      if (!S_ISDIR(st.st_mode))
        return false;
    } else if (::mkdir(real.c_str(), 0755))
      return false;
    ++pos;
  }
  return true;
}
bool Storage::prepare(const std::string &p) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string real;
  if (!resolve(p, real) || std::count(p.begin(), p.end(), '/') > 16)
    return false;
  struct stat st{};
  if (info(real.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode))
      return true;
    error = "required directory conflicts with file: " + p;
    return false;
  }
  if (maintenance()) {
    error = "quota maintenance";
    return false;
  }
  if (!parents(p + "/placeholder")) {
    error = "cannot prepare directory: " + p;
    return false;
  }
  return true;
}
bool Storage::mkdir(const std::string &p) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string real;
  if (std::count(p.begin(), p.end(), '/') > 16) {
    error = "directory depth limit";
    return false;
  }
  if (!resolve(p, real) || !room(0))
    return false;
  if (::mkdir(real.c_str(), 0755)) {
    error = "cannot create directory";
    return false;
  }
  return true;
}
bool Storage::listDir(const std::string &p, std::vector<File> &entries) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  entries.clear();
  std::string real;
  if (p == "/") {
    if (!online_)
      return false;
    real = root_;
  } else if (!resolve(p, real))
    return false;
  DIR *d = opendir(real.c_str());
  if (!d) {
    error = "directory unreadable";
    return false;
  }
  bool ok = true;
  while (auto *e = readdir(d)) {
    std::string n = e->d_name;
    if (n == "." || n == ".." || n.find(".t5-") != std::string::npos)
      continue;
    std::string logical = (p == "/" ? "" : p) + "/" + n, physical;
    if (!resolve(logical, physical)) {
      ok = false;
      break;
    }
    struct stat st{};
    if (info(physical.c_str(), &st)) {
      ok = false;
      break;
    }
    entries.push_back({logical, S_ISDIR(st.st_mode) ? 0 : uint64_t(st.st_size),
                       S_ISDIR(st.st_mode)});
    if (entries.size() > 10000) {
      error = "directory entry limit";
      ok = false;
      break;
    }
  }
  closedir(d);
  std::sort(entries.begin(), entries.end(), [](auto &a, auto &b) {
    if (a.directory != b.directory)
      return a.directory > b.directory;
    return a.path < b.path;
  });
  return ok;
}
bool Storage::move(const std::string &src, const std::string &dst) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string a, b;
  if (!resolve(src, a) || !resolve(dst, b))
    return false;
  struct stat st{};
  if (info(b.c_str(), &st) == 0) {
    error = "destination exists";
    return false;
  }
  if (rename(a.c_str(), b.c_str())) {
    error = "move failed";
    return false;
  }
  return reconcile();
}
bool Storage::copy(const std::string &src, const std::string &dst,
                   const std::atomic<bool> *cancel) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string a, b;
  if (!resolve(src, a) || !resolve(dst, b))
    return false;
  struct stat st{}, other{};
  if (info(a.c_str(), &st) || !S_ISREG(st.st_mode)) {
    error = "source must be a regular file";
    return false;
  }
  if (info(b.c_str(), &other) == 0) {
    error = "destination exists";
    return false;
  }
  if (!room(st.st_size))
    return false;
  std::string temp = b + ".t5-new";
  if (info(temp.c_str(), &other) == 0) {
    error = "pending transaction";
    return false;
  }
  FILE *in = fopen(a.c_str(), "rb"),
       *out = in ? fopen(temp.c_str(), "wb") : nullptr;
  bool ok = in && out;
  char buffer[4096];
  uint64_t count = 0;
  while (ok && count < uint64_t(st.st_size)) {
    if (cancelled(cancel)) {
      ok = false;
      break;
    }
    size_t want =
               std::min<uint64_t>(sizeof(buffer), uint64_t(st.st_size) - count),
           n = fread(buffer, 1, want, in);
    if (n != want || fwrite(buffer, 1, n, out) != n) {
      ok = false;
      break;
    }
    count += n;
  }
  if (out) {
    if (ok && (fflush(out) || fsync(fileno(out))))
      ok = false;
    if (fclose(out))
      ok = false;
  }
  if (in)
    fclose(in);
  if (cancelled(cancel))
    ok = false;
  if (ok && rename(temp.c_str(), b.c_str()))
    ok = false;
  if (!ok) {
    unlink(temp.c_str());
    error = cancelled(cancel) ? "cancelled" : "copy I/O failed";
  }
  reconcile();
  return ok;
}
static bool excluded(const std::string &p) {
  for (auto *prefix : {"/backups/", "/cache/", "/tmp/", "/logs/", "/update/"})
    if (p.rfind(prefix, 0) == 0)
      return true;
  return p == "/meta/restore.plan" || p.find(".t5-") != std::string::npos;
}
static void encode(uint64_t n, unsigned char *p, int bytes) {
  for (int i = 0; i < bytes; ++i) {
    p[i] = n & 255;
    n >>= 8;
  }
}
static uint64_t decode(const unsigned char *p, int bytes) {
  uint64_t n = 0;
  for (int i = bytes - 1; i >= 0; --i)
    n = (n << 8) | p[i];
  return n;
}
bool Storage::backup(const std::string &dest, const std::atomic<bool> *cancel) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (dest.rfind("/backups/", 0) != 0) {
    error = "backup destination must be /backups";
    return false;
  }
  std::string path;
  if (!resolve(dest, path))
    return false;
  struct stat st{};
  if (info(path.c_str(), &st) == 0) {
    error = "backup already exists";
    return false;
  }
  std::vector<File> all;
  if (!list(all))
    return false;
  all.erase(std::remove_if(all.begin(), all.end(),
                           [](auto &f) { return excluded(f.path); }),
            all.end());
  if (all.size() > 2048) {
    error = "backup limited to 2048 files";
    return false;
  }
  uint64_t bytes = 16 + 64;
  for (auto &f : all) {
    if (f.size > limit_ || bytes > limit_ - f.size) {
      error = "backup too large";
      return false;
    }
    bytes += 10 + f.path.size() + f.size;
  }
  if (!room(bytes))
    return false;
  std::string temp = path + ".t5-new";
  FILE *out = fopen(temp.c_str(), "wb");
  if (!out) {
    error = "cannot create backup";
    return false;
  }
  Digest digest;
  bool ok = true;
  auto write = [&](const void *p, size_t n) {
    if (fwrite(p, 1, n, out) != n) {
      ok = false;
      return;
    }
    digest.update(p, n);
  };
  unsigned char header[16] = {'T', '5', 'B', 'A', 'K', '0', '0', '1'};
  encode(all.size(), header + 8, 8);
  write(header, 16);
  char buf[4096];
  for (auto &f : all) {
    if (!ok || cancelled(cancel)) {
      ok = false;
      break;
    }
    unsigned char record[10];
    encode(f.path.size(), record, 2);
    encode(f.size, record + 2, 8);
    write(record, 10);
    write(f.path.data(), f.path.size());
    std::string real;
    if (!resolve(f.path, real)) {
      ok = false;
      break;
    }
    FILE *in = fopen(real.c_str(), "rb");
    if (!in) {
      ok = false;
      break;
    }
    uint64_t remaining = f.size;
    while (ok && remaining) {
      if (cancelled(cancel)) {
        ok = false;
        break;
      }
      size_t n = std::min<uint64_t>(remaining, sizeof(buf));
      if (fread(buf, 1, n, in) != n) {
        ok = false;
        break;
      }
      write(buf, n);
      remaining -= n;
    }
    fclose(in);
  }
  auto sum = digest.finish();
  if (ok && (fwrite(sum.data(), 1, 64, out) != 64 || fflush(out) ||
             fsync(fileno(out))))
    ok = false;
  if (fclose(out))
    ok = false;
  if (cancelled(cancel))
    ok = false;
  if (ok && rename(temp.c_str(), path.c_str()))
    ok = false;
  if (!ok) {
    unlink(temp.c_str());
    error = cancelled(cancel) ? "cancelled" : "backup I/O failed";
  }
  reconcile();
  return ok;
}
struct ArchiveEntry {
  std::string path, hash;
  uint64_t size;
};
bool Storage::restore(const std::string &source,
                      const std::atomic<bool> *cancel) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string real;
  if (source.rfind("/backups/", 0) != 0 || !resolve(source, real)) {
    error = "invalid backup path";
    return false;
  }
  FILE *in = fopen(real.c_str(), "rb");
  if (!in) {
    error = "backup missing";
    return false;
  }
  Digest digest;
  bool ok = true;
  auto read = [&](void *p, size_t n) {
    if (fread(p, 1, n, in) != n) {
      ok = false;
      return;
    }
    digest.update(p, n);
  };
  unsigned char header[16];
  read(header, 16);
  uint64_t count = ok ? decode(header + 8, 8) : 0;
  if (!ok || memcmp(header, "T5BAK001", 8) || count > 2048) {
    fclose(in);
    error = "invalid backup header";
    return false;
  }
  std::vector<ArchiveEntry> entries;
  std::set<std::string> seen;
  uint64_t peak = 0;
  char buffer[4096];
  for (uint64_t i = 0; ok && i < count; ++i) {
    unsigned char rec[10];
    read(rec, 10);
    uint64_t n = ok ? decode(rec, 2) : 0, size = ok ? decode(rec + 2, 8) : 0;
    if (!n || n > 240 || size > limit_ || peak > limit_ - size) {
      ok = false;
      break;
    }
    std::string p(n, '\0');
    read(p.data(), n);
    std::string physical;
    std::string alias = p;
    for (char &c : alias)
      if (c >= 'A' && c <= 'Z')
        c += 32;
    if (!ok || !validPath(p) || excluded(p) || !seen.insert(alias).second ||
        !resolve(p, physical)) {
      ok = false;
      break;
    }
    ArchiveEntry entry{p, "", size};
    Digest fileHash;
    uint64_t left = size;
    while (ok && left) {
      if (cancelled(cancel)) {
        ok = false;
        break;
      }
      size_t chunk = std::min<uint64_t>(left, sizeof(buffer));
      read(buffer, chunk);
      fileHash.update(buffer, chunk);
      left -= chunk;
    }
    entry.hash = fileHash.finish();
    entries.push_back(entry);
    peak += size;
  }
  char hash[64];
  if (!ok || fread(hash, 1, 64, in) != 64 ||
      digest.finish() != std::string(hash, 64) || fgetc(in) != EOF ||
      cancelled(cancel)) {
    fclose(in);
    error = cancelled(cancel) ? "cancelled" : "backup integrity/path rejected";
    return false;
  }
  // Stage all entries before commit. The durable plan is the commit boundary.
  uint64_t planBytes = entries.size() * 330 + 16;
  if (peak > limit_ - std::min(limit_, planBytes) || !room(peak + planBytes)) {
    fclose(in);
    return false;
  }
  rewind(in);
  if (fread(header, 1, 16, in) != 16) {
    fclose(in);
    error = "backup reread failed";
    return false;
  }
  std::vector<std::string> staged;
  std::string plan = "T5RESTORE1\n";
  for (auto &e : entries) {
    if (cancelled(cancel) || !parents(e.path)) {
      ok = false;
      break;
    }
    std::string path;
    if (!resolve(e.path, path)) {
      ok = false;
      break;
    }
    struct stat st{};
    if (info((path + ".t5-new").c_str(), &st) == 0 ||
        info((path + ".t5-old").c_str(), &st) == 0 ||
        (info(path.c_str(), &st) == 0 && !S_ISREG(st.st_mode))) {
      ok = false;
      break;
    }
    FILE *out = fopen((path + ".t5-new").c_str(), "wb");
    if (!out) {
      ok = false;
      break;
    }
    staged.push_back(path + ".t5-new");
    unsigned char skip[10];
    std::string skipName(e.path.size(), '\0');
    if (fread(skip, 1, 10, in) != 10 ||
        fread(skipName.data(), 1, skipName.size(), in) != skipName.size() ||
        skipName != e.path)
      ok = false;
    uint64_t left = e.size;
    while (ok && left) {
      if (cancelled(cancel)) {
        ok = false;
        break;
      }
      size_t n = std::min<uint64_t>(left, sizeof(buffer));
      if (fread(buffer, 1, n, in) != n || fwrite(buffer, 1, n, out) != n) {
        ok = false;
        break;
      }
      left -= n;
    }
    if (ok && (fflush(out) || fsync(fileno(out))))
      ok = false;
    if (fclose(out))
      ok = false;
    plan += e.path + "|" + std::to_string(e.size) + "|" + e.hash + "\n";
    if (!ok)
      break;
  }
  fclose(in);
  if (cancelled(cancel))
    ok = false;
  if (ok)
    ok = parents("/meta/restore.plan") &&
         write("/meta/restore.plan", plan, cancel);
  if (!ok) {
    for (auto &p : staged)
      unlink(p.c_str());
    error =
        cancelled(cancel) ? "cancelled" : "restore staging failed: " + error;
    reconcile();
    return false;
  }
  // Once journaled, roll forward even if cancellation is requested.
  ok = recoverRestore();
  if (!ok)
    invalidate();
  else
    reconcile();
  return ok;
}
bool Storage::recoverRestore() {
  std::string planFile = root_ + "/meta/restore.plan";
  struct stat st{};
  if (info(planFile.c_str(), &st))
    return true;
  if (!S_ISREG(st.st_mode) || st.st_size > 1024 * 1024) {
    error = "invalid restore journal";
    return false;
  }
  FILE *f = fopen(planFile.c_str(), "rb");
  if (!f)
    return false;
  std::string plan(st.st_size, '\0');
  bool ok = fread(plan.data(), 1, plan.size(), f) == plan.size();
  fclose(f);
  if (!ok || plan.rfind("T5RESTORE1\n", 0) != 0) {
    error = "invalid restore journal";
    return false;
  }
  struct Item {
    std::string path, physical, hash;
    uint64_t size;
    bool staged;
  };
  std::vector<Item> items;
  std::set<std::string> seen;
  auto verify = [&](const std::string &p, const std::string &expected,
                    uint64_t size) {
    struct stat s{};
    if (info(p.c_str(), &s) || !S_ISREG(s.st_mode) ||
        uint64_t(s.st_size) != size)
      return false;
    FILE *in = fopen(p.c_str(), "rb");
    if (!in)
      return false;
    Digest hash;
    char buffer[4096];
    while (size_t n = fread(buffer, 1, sizeof(buffer), in))
      hash.update(buffer, n);
    bool good = !ferror(in) && hash.finish() == expected;
    fclose(in);
    return good;
  };
  std::istringstream lines(plan.substr(11));
  std::string line;
  while (std::getline(lines, line)) {
    auto a = line.find('|'),
         b = line.find('|', a == std::string::npos ? 0 : a + 1);
    if (a == std::string::npos || b == std::string::npos) {
      error = "invalid restore record";
      return false;
    }
    auto p = line.substr(0, a), sizeText = line.substr(a + 1, b - a - 1),
         expected = line.substr(b + 1);
    std::string path, alias = p;
    for (char &c : alias)
      if (c >= 'A' && c <= 'Z')
        c += 32;
    if (!validPath(p) || excluded(p) || !seen.insert(alias).second ||
        !resolve(p, path) || expected.size() != 64 || sizeText.empty() ||
        sizeText.find_first_not_of("0123456789") != std::string::npos) {
      error = "invalid restore path/size";
      return false;
    }
    uint64_t size = strtoull(sizeText.c_str(), nullptr, 10);
    if (size > limit_ || items.size() >= 2048) {
      error = "restore limit";
      return false;
    }
    bool staged = info((path + ".t5-new").c_str(), &st) == 0;
    if (!verify(staged ? path + ".t5-new" : path, expected, size)) {
      error = "restore stage/target corrupted; retain backup";
      return false;
    }
    items.push_back({p, path, expected, size, staged});
  }
  // Every record is validated before the first live rename.
  for (auto &i : items)
    if (i.staged) {
      auto old = i.physical + ".t5-old";
      if (info(i.physical.c_str(), &st) == 0) {
        if (info(old.c_str(), &st) == 0 ||
            rename(i.physical.c_str(), old.c_str())) {
          error = "restore collision";
          return false;
        }
      }
      if (rename((i.physical + ".t5-new").c_str(), i.physical.c_str()))
        return false;
    }
  for (auto &i : items)
    if (info((i.physical + ".t5-old").c_str(), &st) == 0 &&
        unlink((i.physical + ".t5-old").c_str()))
      return false;
  return unlink(planFile.c_str()) == 0;
}
} // namespace tab5
