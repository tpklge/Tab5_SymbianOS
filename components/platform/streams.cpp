// SPDX-License-Identifier: MIT
#include "storage.hpp"
#include <chrono>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "ff.h"
#endif
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
#ifdef ESP_PLATFORM
  if(fatRoot_.empty()){error="FAT volume unavailable";return false;}
  FIL file{};UINT got=0;
  if(f_open(&file,(fatRoot_+p).c_str(),FA_READ)!=FR_OK)return false;
  bool ok=offset<=f_size(&file)&&f_lseek(&file,FSIZE_t(offset))==FR_OK;
  out.resize(n);
  ok=ok&&f_read(&file,out.data(),UINT(n),&got)==FR_OK;
  out.resize(got);ok=(f_close(&file)==FR_OK)&&ok;
  if(!ok)error="stream read failed";
  return ok;
#else
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) {
    error = "cannot read stream";
    return false;
  }
  bool ok = fseeko(f, off_t(offset), SEEK_SET) == 0;
  out.resize(n);
  size_t got = ok ? fread(out.data(), 1, n, f) : 0;
  ok = ok && !ferror(f);
  out.resize(got);
  fclose(f);
  if (!ok)
    error = "stream read failed";
  return ok;
#endif
}
bool Storage::append(const std::string &p, const std::string &data) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  if (data.size() > 65536 || !resolve(p, path))
    return false;
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
  if (fstat(fileno(f), &after) == 0 && storageStatBytes(after.st_size) >= storageStatBytes(before.st_size))
    used_ += storageStatBytes(after.st_size) - storageStatBytes(before.st_size);
  if (fclose(f))
    ok = false;
  if (!ok)
    error = "stream full or unavailable";
  return ok;
}
bool Storage::fileSize(const std::string &p, uint64_t &bytes) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
#ifdef ESP_PLATFORM
  if(!resolve(p,path)||fatRoot_.empty())return false;
  FILINFO info{};
  if(f_stat((fatRoot_+p).c_str(),&info)!=FR_OK||(info.fattrib&AM_DIR))return false;
  bytes=info.fsize;return true;
#else
  struct stat st{};
  if (!resolve(p,path) || stat(path.c_str(),&st) || !S_ISREG(st.st_mode)) {
    error = "stream file unavailable";
    return false;
  }
  bytes=storageStatBytes(st.st_size);
  return true;
#endif
}
bool Storage::appendAt(const std::string &p, const std::string &data,
                       uint64_t offset, uint64_t generation) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  uint64_t bytes=0;
  if (generation != generation_ || !fileSize(p,bytes) || bytes != offset) {
    error = "stream session or offset changed";
    return false;
  }
  return append(p,data);
}
bool Storage::patch(const std::string &p, uint64_t offset,
                    const std::string &data) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::string path;
  if (data.size() > 65536 || offset > 4000000000ULL || !resolve(p, path))
    return false;
#ifdef ESP_PLATFORM
  if(fatRoot_.empty()){error="FAT volume unavailable";return false;}
  FIL file{};UINT wrote=0;
  if(f_open(&file,(fatRoot_+p).c_str(),FA_READ|FA_WRITE)!=FR_OK)return false;
  bool ok=offset+data.size()<=f_size(&file)&&f_lseek(&file,FSIZE_t(offset))==FR_OK;
  ok=ok&&f_write(&file,data.data(),UINT(data.size()),&wrote)==FR_OK&&wrote==data.size()&&f_sync(&file)==FR_OK;
  ok=(f_close(&file)==FR_OK)&&ok;
  if(!ok)error="stream patch failed";
  return ok;
#else
  FILE *f = fopen(path.c_str(), "rb+");
  if (!f)
    return false;
  bool ok = fseeko(f, 0, SEEK_END) == 0;
  auto length = ftello(f);
  ok = ok && length >= 0 && offset + data.size() <= uint64_t(length) &&
       fseeko(f, off_t(offset), SEEK_SET) == 0;
  ok = ok && fwrite(data.data(), 1, data.size(), f) == data.size() &&
       fflush(f) == 0 && fsync(fileno(f)) == 0;
  if (fclose(f))
    ok = false;
  if (!ok)
    error = "stream patch failed";
  return ok;
#endif
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
