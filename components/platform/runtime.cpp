// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
#include <climits>
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-__LONG_LONG_MAX__ - 1LL)
#define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)
#endif
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
namespace tab5 {
Runtime::VM::~VM() {
  if (L)
    lua_close(L);
}
Runtime::~Runtime() { shutdown(); }
void Runtime::shutdown() {
  media_.stop();
  media_.stopVideo();
  media_.camera(false);
  installed_.clear();
  pictures_.clear();
  thumbnails_.clear();
  pictureOwner_.clear();
  timersEnabled_ = false;
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    timers_.clear();
  }
  stagedTimers_.clear();
  moduleDepth_ = 0;
  apps_.clear();
  grants_.clear();
  caps_.clear();
  messages_.clear();
  notices_.clear();
  entries_.clear();
  verified_.clear();
  ready_ = false;
  current_.clear();
}
void *Runtime::allocate(void *ud, void *p, size_t old, size_t n) {
  auto *v = static_cast<VM *>(ud);
  if (!p)
    old = 0;
  if (!n) {
    free(p);
    v->bytes -= old;
    v->owner->luaBytes_ -= old;
    return nullptr;
  }
  if (n > 1024 * 1024 || v->bytes - old > 1024 * 1024 - n ||
      v->owner->luaBytes_ - old > 4 * 1024 * 1024 - n)
    return nullptr;
#ifdef ESP_PLATFORM
  void *q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  void *q = realloc(p, n);
#endif
  if (q) {
    v->bytes = v->bytes - old + n;
    v->owner->luaBytes_ = v->owner->luaBytes_ - old + n;
  }
  return q;
}
void Runtime::hook(lua_State *L, lua_Debug *) {
  auto *v = *static_cast<VM **>(lua_getextraspace(L));
  if (v->owner->cancelled_)
    luaL_error(L, "cancelled");
  if (v->budget <= 1000)
    luaL_error(L, "instruction budget exhausted");
  v->budget -= 1000;
}
static Runtime *self(lua_State *L) {
  return static_cast<Runtime *>(lua_touserdata(L, lua_upvalueindex(1)));
}
bool Runtime::permission(const std::string &p, bool writing) const {
  if (!Storage::validPath(p))
    return false;
  if (!writing && p.rfind("/config/public/", 0) == 0)
    return true;
  if (writing && (p == "/system" || p == "/boot" || p == "/apps" ||
                  p == "/meta" || p.rfind("/meta/", 0) == 0))
    return false;
  if (writing && (p.rfind("/boot/", 0) == 0 || p.rfind("/system/", 0) == 0 ||
                  p.rfind("/apps/", 0) == 0 || p == "/meta/restore.plan"))
    return false;
  std::string own = "/data/" + current_ + "/";
  if (p.rfind(own, 0) == 0)
    return true;
  auto grant = grants_.find(current_);
  if (grant != grants_.end())
    for (auto &prefix : grant->second)
      if (p.rfind(prefix, 0) == 0)
        return true;
  return false;
}
int Runtime::readApi(lua_State *L) {
  auto *r = self(L);
  size_t n = 0;
  const char *p = luaL_checklstring(L, 1, &n);
  if (n > 240 || memchr(p, 0, n))
    return luaL_error(L, "invalid path");
  r->apiPath_.assign(p, n);
  if (!r->permission(r->apiPath_))
    return luaL_error(L, "app data permission denied");
  r->apiPath_.assign(p, n);
  if (!r->store_.read(r->apiPath_, r->apiScratch_, 65536)) {
    lua_pushnil(L);
    lua_pushstring(L, r->store_.lastError().c_str());
    return 2;
  }
  lua_pushlstring(L, r->apiScratch_.data(), r->apiScratch_.size());
  return 1;
}
int Runtime::writeApi(lua_State *L) {
  auto *r = self(L);
  size_t n = 0;
  const char *p = luaL_checklstring(L, 1, &n);
  if (n > 240 || memchr(p, 0, n))
    return luaL_error(L, "invalid path");
  r->apiPath_.assign(p, n);
  if (!r->permission(r->apiPath_, true))
    return luaL_error(L, "app data permission denied");
  r->apiPath_.assign(p, n);
  const char *data = luaL_checklstring(L, 2, &n);
  if (n > 65536)
    return luaL_error(L, "write limit");
  r->apiScratch_.assign(data, n);
  bool ok = r->store_.write(r->apiPath_, r->apiScratch_, &r->cancelled_);
  lua_pushboolean(L, ok);
  if (ok)
    lua_pushnil(L);
  else
    lua_pushstring(L, r->store_.lastError().c_str());
  return 2;
}
int Runtime::appsApi(lua_State *L) {
  auto *r = self(L);
  lua_newtable(L);
  int index = 1;
  for (auto &entry : r->apps_) {
    if (entry.first == "shell" || entry.first == "services")
      continue;
    lua_pushlstring(L, entry.first.data(), entry.first.size());
    lua_rawseti(L, -2, index++);
  }
  return 1;
}
int Runtime::moduleApi(lua_State *L) {
  auto *r = self(L);
  size_t n = 0;
  const char *name = luaL_checklstring(L, 1, &n);
  if (!n || n > 32)
    return luaL_error(L, "invalid module");
  for (size_t i = 0; i < n; ++i)
    if (!(name[i] >= 'a' && name[i] <= 'z') && name[i] != '_')
      return luaL_error(L, "invalid module");
  r->apiPath_.assign(name, n);
  r->apiPath_ += ".lua";
  if (r->installed_.count(r->current_) &&
      r->installed_[r->current_].hashes.count(r->apiPath_)) {
    Installer installer(r->store_);
    if (!installer.read(r->installed_[r->current_], r->apiPath_,
                        r->apiScratch_))
      return luaL_error(L, "installed module integrity");
    if (luaL_loadbufferx(L, r->apiScratch_.data(), r->apiScratch_.size(), name,
                         "t") != LUA_OK)
      return lua_error(L);
    if (r->moduleDepth_ >= 4)
      return luaL_error(L, "module recursion limit");
    ++r->moduleDepth_;
    lua_call(L, 0, 1);
    --r->moduleDepth_;
    return 1;
  }
  if (!r->verified_.count(r->apiPath_))
    return luaL_error(L, "module not in manifest");
  if (!r->store_.read(r->release_ + "/" + r->apiPath_, r->apiScratch_, 65536) ||
      sha256(r->apiScratch_) != r->verified_[r->apiPath_])
    return luaL_error(L, "module integrity failed");
  if (luaL_loadbufferx(L, r->apiScratch_.data(), r->apiScratch_.size(), name,
                       "t") != LUA_OK)
    return lua_error(L);
  if (r->moduleDepth_ >= 4)
    return luaL_error(L, "module recursion limit");
  ++r->moduleDepth_;
  lua_call(L, 0, 1);
  --r->moduleDepth_;
  return 1;
}
int Runtime::timeApi(lua_State *L) {
  time_t now = self(L)->now();
  if (now < 1700000000) {
    lua_pushliteral(L, "Hora não ajustada");
    return 1;
  }
  char b[80];
  tm t{};
  localtime_r(&now, &t);
  strftime(b, sizeof(b), "%d/%m/%Y  %H:%M", &t);
  lua_pushstring(L, b);
  return 1;
}
static std::string field(lua_State *L, const char *name, size_t limit = 4096) {
  lua_getfield(L, -1, name);
  size_t n = 0;
  const char *s =
      lua_type(L, -1) == LUA_TSTRING ? lua_tolstring(L, -1, &n) : nullptr;
  std::string v = s && n <= limit ? std::string(s, n) : "";
  lua_pop(L, 1);
  return v;
}
bool Runtime::parseView(lua_State *L) {
  if (!lua_istable(L, -1)) {
    error = "event must return a view table";
    return false;
  }
  View v;
  v.layout = field(L, "layout", 32);
  v.result = field(L, "result", 512);
  if (v.layout == "camera" && !capability("camera")) {
    error = "camera view denied";
    return false;
  }
  if (v.layout == "video_view" && !capability("media")) {
    error = "video view denied";
    return false;
  }
  v.fields = field(L, "fields", 512);
  if (pictures_.count(current_) &&
      (v.layout == "image" || v.layout == "image_zoom" || v.layout == "photo"))
    v.picture = pictures_[current_];
  lua_getfield(L, -1, "mediaStatus");
  v.mediaStatus = lua_toboolean(L, -1);
  lua_pop(L, 1);
  if (v.mediaStatus && !capability("media")) {
    error = "media status capability denied";
    return false;
  }
  if ((v.layout == "image" || v.layout == "image_zoom" ||
       v.layout == "photo") &&
      !capability("media") && !capability("camera") &&
      !pictures_.count(current_)) {
    error = "image view capability denied";
    return false;
  }
  lua_getfield(L, -1, "controls");
  if (lua_istable(L, -1)) {
    if (lua_rawlen(L, -1) > 6) {
      lua_pop(L, 1);
      error = "too many controls";
      return false;
    }
    for (size_t i = 1; i <= lua_rawlen(L, -1); ++i) {
      lua_rawgeti(L, -1, i);
      if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        error = "invalid control";
        return false;
      }
      FormControl c{field(L, "label", 128), field(L, "type", 16),
                    field(L, "options", 4096), field(L, "keys", 1024)};
      if (c.type != "text" && c.type != "password" && c.type != "date" &&
          c.type != "time" && c.type != "select") {
        lua_pop(L, 2);
        error = "invalid control type";
        return false;
      }
      if (c.type == "select") {
        auto count = [](const std::string &s) {
          return 1 + std::count(s.begin(), s.end(), '\n');
        };
        if (c.options.empty() || count(c.options) > 64 ||
            (!c.keys.empty() && count(c.options) != count(c.keys))) {
          lua_pop(L, 2);
          error = "dropdown option/key limit";
          return false;
        }
      }
      v.controls.push_back(std::move(c));
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  v.left = field(L, "left", 32);
  v.right = field(L, "right", 32);
  v.home = field(L, "home", 32);
  v.menu = field(L, "menu", 32);
  v.switcher = field(L, "switcher", 32);
  v.keyboard = field(L, "keyboard", 32);
  v.title = field(L, "title", 384);
  v.body = field(L, "body");
  v.text = field(L, "text", 65536);
  auto bg = field(L, "background", 6), ac = field(L, "accent", 6);
  auto fg = field(L, "foreground", 6), fc = field(L, "fieldColor", 6);
  if (fg.size() == 6)
    v.foreground = fg;
  if (fc.size() == 6)
    v.fieldColor = fc;
  lua_getfield(L, -1, "readonly");
  v.readonly = lua_toboolean(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, -1, "list");
  v.list = lua_toboolean(L, -1);
  lua_pop(L, 1);
  if (bg.size() == 6)
    v.background = bg;
  if (ac.size() == 6)
    v.accent = ac;
  if (fg.empty()) {
    auto rgb = strtoul(v.background.c_str(), nullptr, 16);
    unsigned luminance = ((rgb >> 16) & 255) * 299 + ((rgb >> 8) & 255) * 587 +
                         (rgb & 255) * 114;
    if (luminance > 128000) {
      v.foreground = "152438";
      if (fc.empty())
        v.fieldColor = "ffffff";
    }
  }
  lua_getfield(L, -1, "field");
  v.field = lua_toboolean(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, -1, "classic");
  v.classic = lua_toboolean(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, -1, "buttons");
  if (lua_istable(L, -1)) {
    size_t n = lua_rawlen(L, -1);
    if (n > 32) {
      lua_pop(L, 1);
      error = "too many buttons";
      return false;
    }
    for (size_t i = 1; i <= n; ++i) {
      lua_rawgeti(L, -1, i);
      if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        error = "bad button";
        return false;
      }
      Button button{field(L, "id", 512), field(L, "label", 512),
                    field(L, "subtitle", 512), field(L, "icon", 32)};
      auto path = field(L, "thumbnail", 240);
      if (!path.empty() && capability("media") && permission(path)) {
        if (!thumbnails_.count(path)) {
          if (thumbnails_.size() >= 24)
            thumbnails_.erase(thumbnails_.begin());
          thumbnails_[path] = media_.thumbnail(path);
        }
        button.thumbnail = thumbnails_[path];
      }
      v.buttons.push_back(std::move(button));
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  lua_getfield(L, -1, "tools");
  if (lua_istable(L, -1)) {
    if (lua_rawlen(L, -1) > 16) {
      lua_pop(L, 1);
      error = "too many tools";
      return false;
    }
    for (size_t i = 1; i <= lua_rawlen(L, -1); ++i) {
      lua_rawgeti(L, -1, i);
      if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        error = "invalid tool";
        return false;
      }
      v.tools.push_back({field(L, "id", 512), field(L, "label", 128), "",
                         field(L, "icon", 32)});
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  view_ = std::move(v);
  return true;
}
bool Runtime::invoke(VM &v, const std::string &event, const std::string &value,
                     bool render) {
  v.budget = 100000;
  moduleDepth_ = 0;
  lua_State *L = v.L;
  lua_settop(L, 0);
  lua_pushnil(L);
  lua_setglobal(L, "navigation_blocked");
  lua_pushnil(L);
  lua_setglobal(L, "next_app");
  lua_getglobal(L, "event");
  if (!lua_isfunction(L, -1)) {
    v.faulted = true;
    error = "missing event callback";
    return false;
  }
  lua_pushlstring(L, event.data(), event.size());
  lua_pushlstring(L, value.data(), value.size());
  lua_sethook(L, hook, LUA_MASKCOUNT, 1000);
  int rc = lua_pcall(L, 2, 1, 0);
  lua_sethook(L, nullptr, 0, 0);
  moduleDepth_ = 0;
  if (rc != LUA_OK) {
    v.faulted = true;
    error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Lua failure";
    lua_settop(L, 0);
    return false;
  }
  v.faulted = false;
  bool ok = !render || parseView(L);
  v.faulted = !ok;
  if (ok && render)
    v.last = view_;
  lua_settop(L, 0);
  return ok;
}
bool Runtime::load(const std::string &id) {
  moduleDepth_ = 0;
  auto found = entries_.find(id);
  if (found == entries_.end()) {
    error = "unknown app";
    return false;
  }
  std::string script;
  if (installed_.count(id)) {
    Installer installer(store_);
    if (!installer.read(installed_[id], "main.lua", script) ||
        script.size() > 65536) {
      error = "installed entry integrity/size";
      return false;
    }
  } else if (!asset(found->second, script, 65536))
    return false;
  auto vm = std::unique_ptr<VM>(new VM);
  vm->id = id;
  vm->owner = this;
  vm->generation = ++serial_;
  vm->L = lua_newstate(allocate, vm.get());
  if (!vm->L) {
    error = "Lua memory limit";
    return false;
  }
  lua_State *L = vm->L;
  *static_cast<VM **>(lua_getextraspace(L)) = vm.get();
  luaL_requiref(L, "_G", luaopen_base, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "string", luaopen_string, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "table", luaopen_table, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "math", luaopen_math, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "utf8", luaopen_utf8, 1);
  lua_pop(L, 1);
  for (auto *key : {"dofile", "loadfile", "load", "collectgarbage", "print",
                    "pcall", "xpcall", "setmetatable", "getmetatable"}) {
    lua_pushnil(L);
    lua_setglobal(L, key);
  }
  lua_newtable(L);
  for (auto pair : {std::make_pair("asset", resourceApi),
                    std::make_pair("media", mediaApi),
                    std::make_pair("power", powerApi),
                    std::make_pair("net", networkApi),
                    std::make_pair("package", packageApi),
                    std::make_pair("catalog", catalogApi),
                    std::make_pair("theme", themeApi),
                    std::make_pair("app_info", appInfoApi),
                    std::make_pair("apps", appsApi),
                    std::make_pair("module", moduleApi),
                    std::make_pair("read", readApi),
                    std::make_pair("write", writeApi),
                    std::make_pair("time", timeApi),
                    std::make_pair("schedule", scheduleApi),
                    std::make_pair("dismiss", dismissApi),
                    std::make_pair("offline", offlineApi),
                    std::make_pair("new_id", idApi),
                    std::make_pair("fs", fsApi),
                    std::make_pair("usage", usageApi),
                    std::make_pair("close", closeApi),
                    std::make_pair("notify", notifyApi),
                    std::make_pair("notices", noticesApi),
                    std::make_pair("send", sendApi),
                    std::make_pair("epoch", epochApi),
                    std::make_pair("date", dateApi),
                    std::make_pair("set_clock", setClockApi),
                    std::make_pair("zone", zoneApi)}) {
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, pair.second, 1);
    lua_setfield(L, -2, pair.first);
  }
  lua_setglobal(L, "sys");
  if (luaL_loadbufferx(L, script.data(), script.size(), id.c_str(), "t") !=
      LUA_OK) {
    error = lua_tostring(L, -1);
    return false;
  }
  lua_sethook(L, hook, LUA_MASKCOUNT, 1000);
  int rc = lua_pcall(L, 0, 0, 0);
  lua_sethook(L, nullptr, 0, 0);
  moduleDepth_ = 0;
  if (rc != LUA_OK) {
    error = lua_tostring(L, -1);
    return false;
  }
  apps_[id] = std::move(vm);
  return true;
}
bool Runtime::background(const std::string &id, const std::string &e,
                         const std::string &value) {
  if (!apps_.count(id))
    return false;
  auto previous = current_;
  current_ = id;
  bool ok = invoke(*apps_[id], e, value, false);
  if (ok && (e == "checkpoint" || e == "suspend")) {
    auto *L = apps_[id]->L;
    lua_getglobal(L, "navigation_blocked");
    bool blocked = lua_toboolean(L, -1);
    lua_pop(L, 1);
    if (blocked) {
      ok = false;
      error = store_.lastError().empty() ? "checkpoint blocked"
                                         : store_.lastError();
    }
  }

  current_ = previous;
  return ok;
}
bool Runtime::close(const std::string &id) {
  error.clear();
  if (id == "shell" || id == "services" || id == current_ || !apps_.count(id)) {
    error = "cannot close active or system app";
    return false;
  }
  if (apps_[id]->faulted) {
    if (apps_[id]->last.field && !apps_[id]->last.text.empty()) {
      auto path = "/data/" + id + "/recovered-draft.txt";
      error =
          store_.write(path, apps_[id]->last.text)
              ? "Aplicativo com erro encerrado; rascunho recuperado em " + path
              : "Aplicativo com erro encerrado; rascunho não persistido: " +
                    store_.lastError();
    } else
      error = "Aplicativo com erro encerrado; último estado salvo preservado.";
  } else if (!background(id, "checkpoint", apps_[id]->last.text))
    return false;
  apps_.erase(id);
  pictures_.erase(id);
  return true;
}
bool Runtime::open(const std::string &id) {
  if (!entries_.count(id) || id == "services") {
    error = "unknown application";
    return false;
  }
  if (!current_.empty() && current_ != id && apps_.count(current_) &&
      !apps_[current_]->faulted) {
    auto old = view_;
    if (!invoke(*apps_[current_], "suspend", view_.text)) {
      view_ = old;
      return false;
    }
    lua_State *L = apps_[current_]->L;
    lua_getglobal(L, "navigation_blocked");
    bool blocked = lua_toboolean(L, -1);
    lua_pop(L, 1);
    if (blocked) {
      error = view_.body;
      return false;
    }
  }
  if (!apps_.count(id)) {
    size_t regular = 0;
    for (auto &a : apps_)
      if (a.first != "shell" && a.first != "services")
        ++regular;
    if (regular >= 3 && id != "shell") {
      std::string victim;
      uint64_t oldest = UINT64_MAX;
      for (auto &a : apps_)
        if (a.first != "shell" && a.first != "services" &&
            a.first != current_ && a.second->touched < oldest) {
          oldest = a.second->touched;
          victim = a.first;
        }
      if (victim.empty() || !close(victim))
        return false;
    }
  }
  auto previous = current_;
  auto oldView = view_;
  bool created = !apps_.count(id);
  current_ = id;
  if ((created && !load(id)) || !invoke(*apps_[id], "focus", "")) {
    current_ = previous;
    view_ = oldView;
    if (created) {
      apps_.erase(id);
      pictures_.erase(id);
    }
    return false;
  }
  apps_[id]->touched = ++serial_;
  return true;
}
std::string Runtime::clockText() const {
  std::lock_guard<std::mutex> lock(clockMutex_);
  time_t epoch =
      clockBase_ + std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - clockStart_)
                       .count();
  if (epoch < 946684800)
    return "Relógio não ajustado • SD";
  tm parts{};
  char text[80];
  localtime_r(&epoch, &parts);
  strftime(text, sizeof(text), "%d/%m/%Y  %H:%M:%S  • SD", &parts);
  return text;
}
void Runtime::applyZone(const char *zone) {
  std::lock_guard<std::mutex> lock(clockMutex_);
  setenv("TZ", zone, 1);
  tzset();
}
int64_t Runtime::now() const {
  std::lock_guard<std::mutex> lock(clockMutex_);
  return clockBase_ + std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::steady_clock::now() - clockStart_)
                          .count();
}
void Runtime::setClock(int64_t epoch) {
  std::lock_guard<std::mutex> lock(clockMutex_);
  clockBase_ = epoch;
  clockStart_ = std::chrono::steady_clock::now();
}
bool Runtime::tick() {
  if (!ready_)
    return false;
  bool ok = true;
  if (apps_.count("services"))
    ok = background("services", "tick", "");
  unsigned count = 0;
  while (!messages_.empty() && count++ < 8) {
    auto m = messages_.front();
    messages_.pop_front();
    if (apps_.count(m.target) && apps_[m.target]->generation == m.serial)
      ok = background(m.target, m.event, m.value) && ok;
  }
  return ok;
}
bool Runtime::dispatch(const std::string &e, const std::string &value) {
  if (!ready_ || !store_.online()) {
    error = "system offline";
    return false;
  }
  if (view_.field && e != "focus") {
    view_.text = value;
    apps_[current_]->last.text = value;
  }
  if (!invoke(*apps_[current_], e, value))
    return false; // Explicit navigation commands originate in external scripts.
  lua_State *L = apps_[current_]->L;
  lua_getglobal(L, "navigation_blocked");
  bool blocked = lua_toboolean(L, -1);
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setglobal(L, "navigation_blocked");
  if (blocked) {
    error = view_.body;
    return false;
  }
  lua_getglobal(L, "next_app");
  std::string target = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
  lua_pop(L, 1);
  lua_pushnil(L);
  lua_setglobal(L, "next_app");
  if (!target.empty())
    return open(target);
  return true;
}
bool Runtime::asset(const std::string &name, std::string &out, size_t maximum) {
  if (!verified_.count(name) ||
      !store_.read(release_ + "/" + name, out, maximum) ||
      sha256(out) != verified_[name]) {
    error = "asset integrity failed: " + name;
    return false;
  }
  return true;
}
bool Runtime::boot() {
  shutdown();
  error.clear();
  if (!store_.mount()) {
    error = store_.lastError();
    return false;
  }
  if (store_.maintenance()) {
    error = "installation exceeds 4 GB: maintenance required";
    return false;
  }
  std::string manifest;
  if (!store_.read("/boot/manifest.txt", manifest, 32768)) {
    error = "boot manifest missing: " + store_.lastError();
    return false;
  }
  std::istringstream in(manifest);
  std::string line;
  std::map<std::string, std::string> values;
  std::vector<std::pair<std::string, std::string>> hashes;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) {
      error = "invalid manifest";
      return false;
    }
    auto key = line.substr(0, eq), val = line.substr(eq + 1);
    if (key == "file") {
      auto bar = val.find('|');
      if (bar == std::string::npos) {
        error = "invalid hash entry";
        return false;
      }
      hashes.push_back({val.substr(0, bar), val.substr(bar + 1)});
    } else {
      if (values.count(key)) {
        error = "duplicate manifest key";
        return false;
      }
      values[key] = val;
    }
  }
  if (values["format"] != "1" ||
      (values["api_min"] != "1" && values["api_min"] != "2" &&
       values["api_min"] != "3" && values["api_min"] != "4" &&
       values["api_min"] != "5" && values["api_min"] != "6" &&
       values["api_min"] != "7") ||
      (values["api_max"] != "1" && values["api_max"] != "2" &&
       values["api_max"] != "3" && values["api_max"] != "4" &&
       values["api_max"] != "5" && values["api_max"] != "6" &&
       values["api_max"] != "7") ||
      values["api_min"] > values["api_max"] || values["schema"] != "1") {
    error = "incompatible release";
    return false;
  }
  std::string rel = values["release"];
  if (rel.empty() ||
      rel.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyz.-") !=
          std::string::npos ||
      rel == "." || rel == "..") {
    error = "invalid release";
    return false;
  }
  release_ = "/system/releases/" + rel;
  std::map<std::string, bool> verified;
  for (auto &f : hashes) {
    std::string data, p = release_ + "/" + f.first;
    if (verified.count(f.first) || !Storage::validPath(p) ||
        !store_.read(p, data) || sha256(data) != f.second) {
      error = "release integrity failed: " + f.first;
      return false;
    }
    verified[f.first] = true;
    verified_[f.first] = f.second;
  }
  for (auto &v : values)
    if (v.first.rfind("cap.", 0) == 0)
      caps_[v.first.substr(4)] = v.second;
  for (auto &v : values)
    if (v.first.rfind("grant.", 0) == 0) {
      std::string id = v.first.substr(6);
      std::istringstream prefixes(v.second);
      std::string prefix;
      while (std::getline(prefixes, prefix, ',')) {
        if (prefix != "/" && !Storage::validPath(prefix + "x")) {
          error = "invalid permission grant";
          return false;
        }
        grants_[id].push_back(prefix);
      }
    }
  for (auto &v : values)
    if (v.first.compare(0, 4, "app.") == 0) {
      std::string id = v.first.substr(4);
      if (id.empty() ||
          id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") !=
              std::string::npos ||
          !verified.count(v.second)) {
        error = "unverified app entry";
        return false;
      }
      entries_[id] = v.second;
    }
  if (!verified.count("theme.ttf") || !verified.count("keyboard.map") ||
      !verified.count("onscreen.keys") || !verified.count("chrome.lua") ||
      !entries_.count("shell") || !entries_.count("notes") ||
      !entries_.count("calculator")) {
    error = "essential release files missing";
    return false;
  }
  // Provision only missing directories after root, manifest and assets
  // validated. Existing documents/preferences are never replaced by this
  // migration.
  for (auto &app : entries_)
    if (!store_.prepare("/data/" + app.first)) {
      error = store_.lastError();
      return false;
    }
  for (auto &grant : grants_)
    for (auto prefix : grant.second)
      if (prefix != "/") {
        if (prefix.back() == '/')
          prefix.pop_back();
        if (!store_.prepare(prefix)) {
          error = store_.lastError();
          return false;
        }
      }
  for (auto *dir : {"/meta", "/backups", "/config/public", "/apps",
                    "/home/Music", "/home/Recordings", "/home/Pictures",
                    "/home/Transfer", "/update/packages"})
    if (!store_.prepare(dir)) {
      error = store_.lastError();
      return false;
    }
  media_.recover();
  if (!refreshInstalled())
    return false;
  if (entries_.count("services")) {
    current_ = "services";
    if (!load("services") || !invoke(*apps_["services"], "start", "", false)) {
      error = "background service boot failed: " + error;
      return false;
    }
    current_.clear();
  }
  ready_ = open("shell");
  timersEnabled_ = ready_;
  return ready_;
}
} // namespace tab5
