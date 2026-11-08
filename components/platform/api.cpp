// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <climits>
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-__LONG_LONG_MAX__ - 1LL)
#define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)
#endif
extern "C" {
#include "lauxlib.h"
#include "lua.h"
}
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
namespace tab5 {
static Runtime *self(lua_State *L) {
  return static_cast<Runtime *>(lua_touserdata(L, lua_upvalueindex(1)));
}
bool Runtime::capability(const std::string &name) const {
  auto it = caps_.find(current_);
  return it != caps_.end() &&
         ("," + it->second + ",").find("," + name + ",") != std::string::npos;
}
int Runtime::appInfoApi(lua_State *L) {
  auto *r = self(L);
  const char *id = luaL_checkstring(L, 1);
  auto it = r->apps_.find(id);
  if (it == r->apps_.end()) {
    lua_pushnil(L);
    return 1;
  }
  lua_newtable(L);
  lua_pushinteger(L, it->second->bytes);
  lua_setfield(L, -2, "memory");
  lua_pushboolean(L, it->second->faulted);
  lua_setfield(L, -2, "faulted");
  lua_pushboolean(L, r->current_ == id);
  lua_setfield(L, -2, "active");
  return 1;
}
int Runtime::fsApi(lua_State *L) {
  auto *r = self(L);
  size_t n = 0, m = 0;
  const char *op = luaL_checkstring(L, 1), *path = luaL_checklstring(L, 2, &n),
             *other = luaL_optlstring(L, 3, "", &m);
  int offset = int(luaL_optinteger(L, 4, 0));
  const char *filter = luaL_optstring(L, 5, "");
  if (strlen(filter) > 16)
    return luaL_error(L, "invalid list filter");
  if (n > 240 || m > 240 || memchr(path, 0, n) || memchr(other, 0, m) ||
      offset < 0)
    return luaL_error(L, "invalid file arguments");
  r->error.clear();
  bool ok = false, isList = false, isStat = false;
  size_t total = 0;
  {
    std::string p(path, n), dst(other, m), operation(op);
    if (operation == "list") {
      isList = true;
      if (!r->permission((p == "/" ? "" : p) + "/x")) {
        r->error = "file permission denied";
      } else {
        ok = r->store_.listDir(p, r->apiEntries_);
        if (ok && *filter) {
          std::string kind = filter;
          r->apiEntries_.erase(
              std::remove_if(
                  r->apiEntries_.begin(), r->apiEntries_.end(),
                  [&](const File &f) {
                    std::string path = f.path;
                    std::transform(
                        path.begin(), path.end(), path.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    auto dot = path.find_last_of('.');
                    auto ext = dot == std::string::npos ? "" : path.substr(dot);
                    if (kind == "visual")
                      return f.directory || (ext != ".jpg" && ext != ".jpeg" &&
                                             ext != ".png" && ext != ".avi");
                    if (kind == "audio")
                      return f.directory ||
                             (ext != ".wav" && ext != ".mp3" && ext != ".au");
                    if (kind == "recordings")
                      return f.directory || (ext != ".wav" && ext != ".au");
                    return false;
                  }),
              r->apiEntries_.end());
        }
        total = r->apiEntries_.size();
        if (ok) {
          size_t start = std::min<size_t>(offset, total),
                 end = std::min(start + 12, total);
          std::vector<File> page(r->apiEntries_.begin() + start,
                                 r->apiEntries_.begin() + end);
          r->apiEntries_ = std::move(page);
        }
      }
    } else if (operation == "stat") {
      isStat = true;
      if (!r->permission(p == "/" ? "/x" : p)) {
        r->error = "file permission denied";
      } else if (p == "/") {
        r->apiEntries_ = {{"/", 0, true}};
        ok = true;
      } else {
        auto slash = p.rfind('/');
        std::string parent = slash == 0 ? "/" : p.substr(0, slash);
        ok = r->store_.listDir(parent, r->apiEntries_);
        if (ok) {
          auto found =
              std::find_if(r->apiEntries_.begin(), r->apiEntries_.end(),
                           [&](auto &e) { return e.path == p; });
          if (found == r->apiEntries_.end()) {
            ok = false;
            r->error = "file not found";
          } else {
            auto entry = *found;
            r->apiEntries_ = {entry};
          }
        }
      }
    } else if (operation == "backup" || operation == "restore") {
      if (!r->capability("backup"))
        r->error = "backup capability denied";
      else {
        bool saved = true;
        for (auto &app : r->apps_)
          if (app.first != r->current_ && app.first != "services")
            if (!r->background(app.first, "checkpoint",
                               app.second->last.text)) {
              saved = false;
              break;
            }
        if (saved) {
          ok = operation == "backup" ? r->store_.backup(p, &r->cancelled_)
                                     : r->store_.restore(p, &r->cancelled_);
        }
      }
    } else if (!r->permission(p, operation != "copy") ||
               (operation == "copy" && !r->permission(dst, true)) ||
               (operation == "move" && !r->permission(dst, true))) {
      r->error = "file permission denied";
    } else if (operation == "mkdir")
      ok = r->store_.mkdir(p);
    else if (operation == "remove")
      ok = r->store_.remove(p);
    else if (operation == "copy")
      ok = r->store_.copy(p, dst, &r->cancelled_);
    else if (operation == "move")
      ok = r->store_.move(p, dst);
    else
      r->error = "unknown filesystem operation";
    if (!ok && r->error.empty())
      r->error = r->store_.lastError();
  }
  if (!ok) {
    lua_pushnil(L);
    lua_pushstring(L, r->error.c_str());
    return 2;
  }
  if (isStat) {
    auto &e = r->apiEntries_.front();
    lua_newtable(L);
    lua_pushstring(L, e.path.c_str());
    lua_setfield(L, -2, "path");
    lua_pushinteger(L, e.size);
    lua_setfield(L, -2, "size");
    lua_pushboolean(L, e.directory);
    lua_setfield(L, -2, "directory");
    return 1;
  }
  if (isList) {
    lua_newtable(L);
    int i = 1;
    for (auto &entry : r->apiEntries_) {
      lua_newtable(L);
      lua_pushlstring(L, entry.path.data(), entry.path.size());
      lua_setfield(L, -2, "path");
      lua_pushinteger(L, entry.size);
      lua_setfield(L, -2, "size");
      lua_pushboolean(L, entry.directory);
      lua_setfield(L, -2, "directory");
      lua_rawseti(L, -2, i++);
    }
    lua_pushinteger(L, total);
    return 2;
  }
  lua_pushboolean(L, true);
  return 1;
}
int Runtime::dismissApi(lua_State *L) {
  auto *r = self(L);
  if (!r->capability("notify"))
    return luaL_error(L, "notification capability denied");
  const char *id = luaL_checkstring(L, 1);
  r->notices_.erase(std::remove_if(r->notices_.begin(), r->notices_.end(),
                                   [&](auto &n) { return n.id == id; }),
                    r->notices_.end());
  return 0;
}
int Runtime::offlineApi(lua_State *L) {
  auto *r = self(L);
  if (!r->capability("profile"))
    return luaL_error(L, "profile capability denied");
  bool offline = lua_toboolean(L, 1);
  bool ok = !r->offlineWriter_ || r->offlineWriter_(offline);
  if (ok && !offline)
    r->network_.resumeAutomatic();
  lua_pushboolean(L, ok);
  return 1;
}
int Runtime::idApi(lua_State *L) {
  auto *r = self(L);
  uint64_t id = 1;
  if (r->store_.read("/meta/sequence.txt", r->apiScratch_, 64)) {
    char *end = nullptr;
    id = strtoull(r->apiScratch_.c_str(), &end, 10);
    if (!end || end == r->apiScratch_.c_str() || *end || id == 0 ||
        id >= 9007199254740990ULL)
      return luaL_error(L, "invalid ID sequence");
    ++id;
  } else if (r->store_.lastError() != "not found")
    return luaL_error(L, "ID sequence unavailable");
  r->apiScratch_ = std::to_string(id);
  if (!r->store_.write("/meta/sequence.txt", r->apiScratch_, &r->cancelled_)) {
    lua_pushnil(L);
    lua_pushstring(L, r->store_.lastError().c_str());
    return 2;
  }
  lua_pushstring(L, r->apiScratch_.c_str());
  return 1;
}
int Runtime::usageApi(lua_State *L) {
  auto *r = self(L);
  if (!r->store_.reconcile()) {
    lua_pushnil(L);
    lua_pushstring(L, r->store_.lastError().c_str());
    return 2;
  }
  r->store_.list(r->apiEntries_);
  lua_newtable(L);
  const std::pair<const char *, uint64_t> counters[] = {
      {"used", r->store_.used()},
      {"limit", r->store_.limit()},
      {"reserve", r->store_.reserve()},
      {"physical_free", r->store_.physicalFree()}};
  for (auto &p : counters) {
    lua_pushinteger(L, p.second);
    lua_setfield(L, -2, p.first);
  }
  for (auto *category : {"system", "data", "home", "backups", "cache", "tmp",
                         "logs", "apps", "config"}) {
    uint64_t sum = 0;
    for (auto &e : r->apiEntries_)
      if (e.path.rfind(std::string("/") + category + "/", 0) == 0)
        sum += e.size;
    lua_pushinteger(L, sum);
    lua_setfield(L, -2, category);
  }
  return 1;
}
int Runtime::closeApi(lua_State *L) {
  auto *r = self(L);
  const char *id = luaL_checkstring(L, 1);
  bool ok = r->close(id);
  lua_pushboolean(L, ok);
  if (!ok || !r->error.empty()) {
    lua_pushstring(L, r->error.c_str());
    return 2;
  }
  return 1;
}
int Runtime::notifyApi(lua_State *L) {
  auto *r = self(L);
  if (!r->capability("notify"))
    return luaL_error(L, "notification capability denied");
  size_t a = 0, b = 0;
  const char *id = luaL_checklstring(L, 1, &a),
             *text = luaL_checklstring(L, 2, &b);
  if (a > 100 || b > 240)
    return luaL_error(L, "notification limit");
  bool alarm = lua_toboolean(L, 3), sound = lua_toboolean(L, 4);
  {
    auto found =
        std::find_if(r->notices_.begin(), r->notices_.end(),
                     [&](auto &n) { return n.id == std::string(id, a); });
    if (found == r->notices_.end()) {
      if (r->notices_.size() >= 32)
        r->notices_.erase(r->notices_.begin());
      r->notices_.push_back({std::string(id, a), std::string(text, b), alarm});
    } else
      found->text.assign(text, b);
  }
  int64_t due = luaL_optinteger(L, 5, 0);
  bool already = due ? r->acknowledgeTimer(std::string(id, a), due) : false;
  if (sound && !already)
    r->emitSignal(alarm);
  return 0;
}
int Runtime::noticesApi(lua_State *L) {
  auto *r = self(L);
  lua_newtable(L);
  int i = 1;
  for (auto &notice : r->notices_) {
    lua_newtable(L);
    lua_pushstring(L, notice.id.c_str());
    lua_setfield(L, -2, "id");
    lua_pushstring(L, notice.text.c_str());
    lua_setfield(L, -2, "text");
    lua_pushboolean(L, notice.alarm);
    lua_setfield(L, -2, "alarm");
    lua_rawseti(L, -2, i++);
  }
  return 1;
}
int Runtime::sendApi(lua_State *L) {
  auto *r = self(L);
  size_t n;
  const char *target = luaL_checkstring(L, 1), *e = luaL_checkstring(L, 2),
             *value = luaL_optlstring(L, 3, "", &n);
  if (n > 512 || strlen(e) > 32 || strlen(target) > 32)
    return luaL_error(L, "event limit");
  if (r->messages_.size() >= 32) {
    lua_pushboolean(L, false);
    return 1;
  }
  if (r->apps_.count(target))
    r->messages_.push_back(
        {target, e, std::string(value, n), r->apps_[target]->generation});
  else {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, true);
  return 1;
}
int Runtime::epochApi(lua_State *L) {
  auto *r = self(L);
  if (lua_isnoneornil(L, 1)) {
    lua_pushinteger(L, r->now());
    return 1;
  }
  size_t length = 0;
  const char *s = luaL_checklstring(L, 1, &length);
  bool format = length == 10 || length == 16 || length == 19;
  for (size_t i = 0; format && i < length; ++i) {
    if (i == 4 || i == 7)
      format = s[i] == '-';
    else if (i == 10)
      format = s[i] == ' ';
    else if (i == 13 || i == 16)
      format = s[i] == ':';
    else
      format = s[i] >= '0' && s[i] <= '9';
  }
  if (!format) {
    lua_pushnil(L);
    return 1;
  }
  int y, m, d, h = 0, min = 0, sec = 0;
  int count = sscanf(s, "%d-%d-%d %d:%d:%d", &y, &m, &d, &h, &min, &sec);
  if (count < 3 || y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31 ||
      h < 0 || h > 23 || min < 0 || min > 59 || sec < 0 || sec > 59) {
    lua_pushnil(L);
    return 1;
  }
  tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = min;
  t.tm_sec = sec;
  t.tm_isdst = -1;
  time_t epoch = mktime(&t);
  tm check{};
  localtime_r(&epoch, &check);
  if (check.tm_year != y - 1900 || check.tm_mon != m - 1 ||
      check.tm_mday != d) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, epoch);
  return 1;
}
int Runtime::dateApi(lua_State *L) {
  auto *r = self(L);
  time_t epoch = luaL_optinteger(L, 1, r->now());
  const char *mode = luaL_optstring(L, 2, "datetime");
  if (epoch < 946684800 || epoch > 4102444799LL) {
    lua_pushliteral(L, "Hora não ajustada");
    return 1;
  }
  tm t{};
  localtime_r(&epoch, &t);
  const char *fmt = strcmp(mode, "day") == 0       ? "%Y-%m-%d"
                    : strcmp(mode, "month") == 0   ? "%Y-%m"
                    : strcmp(mode, "clock") == 0   ? "%H:%M"
                    : strcmp(mode, "weekday") == 0 ? "%w"
                                                   : "%Y-%m-%d %H:%M";
  char b[64];
  strftime(b, sizeof(b), fmt, &t);
  lua_pushstring(L, b);
  return 1;
}
int Runtime::setClockApi(lua_State *L) {
  auto *r = self(L);
  if (!r->capability("clock"))
    return luaL_error(L, "clock capability denied");
  int64_t epoch = luaL_checkinteger(L, 1);
  if (epoch < 946684800 || epoch > 4102444799LL)
    return luaL_error(L, "clock range");
  bool persistent = r->clockWriter_ && r->clockWriter_(epoch);
  r->setClock(epoch);
  lua_pushboolean(L, true);
  lua_pushboolean(L, persistent);
  return 2;
}
int Runtime::zoneApi(lua_State *L) {
  auto *r = self(L);
  if (!r->capability("clock"))
    return luaL_error(L, "timezone capability denied");
  size_t n;
  const char *zone = luaL_checklstring(L, 1, &n);
  if (!n || n > 64 || memchr(zone, 0, n))
    return luaL_error(L, "timezone format");
  for (size_t i = 0; i < n; ++i)
    if (!strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
                "<>+-:,./",
                zone[i]))
      return luaL_error(L, "timezone format");
  r->applyZone(zone);
  return 0;
}
} // namespace tab5
