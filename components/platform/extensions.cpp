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
#include <cstring>
#include <sstream>
namespace tab5 {
static Runtime *self(lua_State *L) {
  return static_cast<Runtime *>(lua_touserdata(L, lua_upvalueindex(1)));
}
static void str(lua_State *L, const char *key, const std::string &s) {
  lua_pushlstring(L, s.data(), s.size());
  lua_setfield(L, -2, key);
}
bool Runtime::refreshInstalled() {
  Installer installer(store_);
  std::vector<Package> all;
  if (!installer.catalog(all)) {
    error = installer.error;
    return false;
  }
  for (auto &a : installed_) {
    entries_.erase(a.first);
    grants_.erase(a.first);
    caps_.erase(a.first);
  }
  installed_.clear();
  for (const auto &a : all) {
    if (a.hashes.count("theme.txt"))
      continue;
    if (entries_.count(a.id)) {
      error = "installed app conflicts with system app";
      return false;
    }
    installed_[a.id] = a;
    entries_[a.id] = "@installed";
    auto permits = "," + a.permissions + ",";
    if (permits.find(",documents,") != std::string::npos)
      grants_[a.id].push_back("/home/Documents/");
    if (permits.find(",media,") != std::string::npos) {
      grants_[a.id].push_back("/home/Music/");
      grants_[a.id].push_back("/home/Pictures/");
      grants_[a.id].push_back("/home/Recordings/");
      caps_[a.id] = "media";
    }
    if (permits.find(",camera,") != std::string::npos) {
      caps_[a.id] += ",camera";
      grants_[a.id].push_back("/home/Pictures/");
    }
  }
  return true;
}
int Runtime::catalogApi(lua_State *L) {
  auto *r = self(L);
  Installer installer(r->store_);
  std::vector<Package> all;
  if (!installer.catalog(all)) {
    lua_pushnil(L);
    lua_pushstring(L, installer.error.c_str());
    return 2;
  }
  lua_newtable(L);
  int i = 1;
  for (const auto &p : all) {
    lua_newtable(L);
    str(L, "id", p.id);
    str(L, "title", p.title);
    str(L, "version", p.version);
    str(L, "permissions", p.permissions);
    str(L, "kind", p.hashes.count("theme.txt") ? "theme" : "app");
    lua_rawseti(L, -2, i++);
  }
  return 1;
}
int Runtime::packageApi(lua_State *L) {
  auto *r = self(L);
  const char *op = luaL_checkstring(L, 1);
  size_t length = 0;
  const char *arg = luaL_checklstring(L, 2, &length);
  if (!r->capability("install") || length > 240 || memchr(arg, 0, length))
    return luaL_error(L, "installer capability/path denied");
  std::string operation = op, path(arg, length);
  Installer installer(r->store_);
  Package p;
  bool ok = false;
  if (operation == "inspect" || operation == "install") {
    if (!Storage::validPath(path) || path.rfind("/update/packages/", 0) != 0 ||
        !installer.inspect(path, p)) {
      lua_pushnil(L);
      lua_pushstring(L, installer.error.c_str());
      return 2;
    }
    if (r->entries_.count(p.id) && !r->installed_.count(p.id)) {
      lua_pushnil(L);
      lua_pushliteral(L, "system app ID is reserved");
      return 2;
    }
    if (operation == "inspect") {
      r->reviewedPath_ = path;
      r->reviewedHash_ = p.archiveHash;
      lua_newtable(L);
      str(L, "id", p.id);
      str(L, "title", p.title);
      str(L, "version", p.version);
      str(L, "author", p.author);
      str(L, "permissions", p.permissions);
      lua_pushinteger(L, p.bytes);
      lua_setfield(L, -2, "bytes");
      return 1;
    }
    if (r->apps_.count(p.id) && !r->close(p.id)) {
      lua_pushboolean(L, false);
      lua_pushstring(L, r->error.c_str());
      return 2;
    }
    if (r->reviewedPath_ != path || r->reviewedHash_ != p.archiveHash) {
      lua_pushboolean(L, false);
      lua_pushliteral(L, "review required or package changed");
      return 2;
    }
    ok = installer.install(path, &r->cancelled_, r->reviewedHash_);
    r->reviewedPath_.clear();
    r->reviewedHash_.clear();
  } else if (operation == "remove" || operation == "remove_data") {
    if (r->apps_.count(path) && !r->close(path)) {
      lua_pushboolean(L, false);
      lua_pushstring(L, r->error.c_str());
      return 2;
    }
    ok = installer.remove(path, operation == "remove_data");
  } else if (operation == "theme") {
    std::vector<Package> all;
    if (installer.catalog(all))
      for (const auto &a : all)
        if (a.id == path && a.hashes.count("theme.txt"))
          ok = r->store_.write("/config/public/theme", path, &r->cancelled_);
  } else if (operation == "theme_default")
    ok = r->store_.write("/config/public/theme", "", &r->cancelled_);
  if (ok)
    ok = r->refreshInstalled();
  lua_pushboolean(L, ok);
  lua_pushstring(L, ok ? ""
                       : (installer.error.empty() ? r->error.c_str()
                                                  : installer.error.c_str()));
  return 2;
}
int Runtime::themeApi(lua_State *L) {
  auto *r = self(L);
  lua_newtable(L);
  std::string id;
  if (!r->store_.read("/config/public/theme", id, 32) || id.empty())
    return 1;
  Installer installer(r->store_);
  std::vector<Package> all;
  if (!installer.catalog(all))
    return 1;
  for (auto &p : all)
    if (p.id == id && p.hashes.count("theme.txt")) {
      std::string text;
      if (!installer.read(p, "theme.txt", text) || text.size() > 1024)
        return 1;
      std::istringstream lines(text);
      std::string line;
      while (std::getline(lines, line)) {
        auto at = line.find('=');
        if (at == std::string::npos)
          continue;
        auto key = line.substr(0, at), value = line.substr(at + 1);
        if ((key == "background" || key == "accent" || key == "foreground" ||
             key == "fieldColor") &&
            value.size() == 6 &&
            value.find_first_not_of("0123456789abcdefABCDEF") ==
                std::string::npos)
          str(L, key.c_str(), value);
      }
    }
  return 1;
}
int Runtime::resourceApi(lua_State *L) {
  auto *r = self(L);
  size_t n = 0;
  const char *s = luaL_checklstring(L, 1, &n);
  if (n > 100 || memchr(s, 0, n))
    return luaL_error(L, "invalid resource name");
  std::string name(s, n), data;
  bool ok;
  if (r->installed_.count(r->current_)) {
    Installer installer(r->store_);
    ok = installer.read(r->installed_[r->current_], name, data);
  } else
    ok = r->asset(name, data, 65536);
  if (!ok || data.size() > 65536) {
    lua_pushnil(L);
    lua_pushliteral(L, "asset integrity/size failure");
    return 2;
  }
  lua_pushlstring(L, data.data(), data.size());
  return 1;
}
int Runtime::mediaApi(lua_State *L) {
  auto *r = self(L);
  const char *op = luaL_checkstring(L, 1);
  size_t n = 0;
  const char *s = luaL_optlstring(L, 2, "", &n);
  int zoom = luaL_optinteger(L, 3, 1);
  if (n > (std::string(op) == "queue" ? 16384u : 240u) || memchr(s, 0, n))
    return luaL_error(L, "invalid media path");
  std::string operation = op, path(s, n);
  bool camera = operation == "capture" || operation == "preview" ||
                operation == "camera_on" || operation == "camera_off" ||
                operation == "video_record" || operation == "camera_status" ||
                operation == "video_active";
  bool ownImage = operation == "image" && path.rfind("asset:", 0) == 0 &&
                  r->installed_.count(r->current_);
  if (!ownImage && !r->capability(camera ? "camera" : "media") &&
      !((operation == "video_stop" || operation == "camera_status" ||
         operation == "camera_off") &&
        (r->capability("media") || r->capability("camera"))))
    return luaL_error(L, "media capability denied");
  if (operation == "video_active") {
    lua_pushboolean(L, r->media_.info().filming);
    return 1;
  }
  if (operation == "camera_status") {
    lua_pushstring(L, r->media_.cameraStatus().c_str());
    return 1;
  }
  if (operation == "status") {
    lua_pushstring(L, r->media_.status().c_str());
    return 1;
  }
  bool ok = false;
  if (operation == "volume") {
    if (lua_gettop(L) >= 3)
      r->media_.setVolume(int(luaL_checkinteger(L, 3)));
    lua_pushinteger(L, r->media_.volume());
    return 1;
  }
  if (operation == "next_track" || operation == "prev_track") {
    r->media_.skip(operation == "next_track" ? 1 : -1);
    ok = true;
  } else if (operation == "camera_on" || operation == "camera_off")
    ok = r->media_.camera(operation == "camera_on");
  else if (operation == "video_stop") {
    r->media_.stopVideo();
    ok = true;
  } else if (operation == "video_record" || operation == "video_play") {
    if (!r->permission(path, operation == "video_record")) {
      lua_pushboolean(L, false);
      lua_pushliteral(L, "video path denied");
      return 2;
    }
    ok = r->media_.video(path, operation == "video_play");
  } else if (operation == "pause") {
    r->media_.pause();
    ok = true;
  } else if (operation == "stop") {
    r->media_.stop();
    ok = true;
  } else if (operation == "test") {
    r->emitSignal(true);
    ok = !r->media_.device.diagnostic || r->audioResult() == "ok";
  } else if (operation == "preview")
    ok = r->media_.photograph("", false);
  else if (ownImage) {
    Installer installer(r->store_);
    std::string data;
    ok = installer.read(r->installed_[r->current_], path.substr(6), data) &&
         r->media_.imageData(data, zoom);
  } else if (operation == "queue") {
    std::istringstream lines(path);
    std::string item;
    std::vector<std::string> paths;
    while (std::getline(lines, item)) {
      if (!r->permission(item) || paths.size() >= 64) {
        lua_pushboolean(L, false);
        lua_pushliteral(L, "playlist path denied");
        return 2;
      }
      paths.push_back(item);
    }
    ok = r->media_.queue(paths, zoom > 0 ? zoom - 1 : 0);
  } else if (!r->permission(path,
                            operation == "record" || operation == "capture")) {
    lua_pushboolean(L, false);
    lua_pushliteral(L, "media path permission denied");
    return 2;
  } else if (operation == "play")
    ok = r->media_.play(path);
  else if (operation == "record")
    ok = r->media_.record(path);
  else if (operation == "image")
    ok = r->media_.image(path, zoom);
  else if (operation == "capture")
    ok = r->media_.photograph(path, true);
  if (ok && (operation == "image" || operation == "capture" ||
             operation == "preview")) {
    r->pictureOwner_ = r->current_;
    r->pictures_[r->current_] = r->media_.picture();
  }
  lua_pushboolean(L, ok);
  bool visual = operation == "video_record" || operation == "video_play" ||
                operation == "video_stop" || operation == "camera_on" ||
                operation == "camera_off";
  lua_pushstring(L, visual ? r->media_.cameraStatus().c_str()
                    : ownImage
                        ? (ok ? "Image asset loaded" : "Image asset failed")
                    : operation == "test" && r->media_.device.diagnostic
                        ? r->audioResult().c_str()
                        : r->media_.status().c_str());
  return 2;
}
int Runtime::powerApi(lua_State *L) {
  auto *r = self(L);
  std::string op = luaL_optstring(L, 1, "status");
  if (op != "status") {
    bool ok = r->capability("power") &&
              (op == "normal" || op == "fast" || op == "off") &&
              r->powerWriter_ && r->powerWriter_(op);
    lua_pushboolean(L, ok);
    return 1;
  }
  auto p = r->powerInfo();
  lua_newtable(L);
  auto number = [&](const char *key, int n) {
    lua_pushinteger(L, n);
    lua_setfield(L, -2, key);
  };
  auto flag = [&](const char *key, bool n) {
    lua_pushboolean(L, n);
    lua_setfield(L, -2, key);
  };
  number("percent", p.percent);
  number("millivolts", p.millivolts);
  number("milliamps", p.milliamps);
  flag("available", p.available);
  flag("charging", p.charging);
  flag("enabled", p.enabled);
  flag("fast", p.fast);
  lua_pushstring(L, p.error.c_str());
  lua_setfield(L, -2, "error");
  return 1;
}
int Runtime::networkApi(lua_State *L) {
  auto *r = self(L);
  if (!r->capability("network"))
    return luaL_error(L, "network capability denied");
  std::string op = luaL_optstring(L, 1, "status");
  if (op == "status") {
    auto s = r->network_.status();
    lua_newtable(L);
    str(L, "message", s.message);
    str(L, "address", s.address);
    str(L, "token", s.token);
    str(L, "ssid", s.ssid);
    lua_pushboolean(L, s.busy);
    lua_setfield(L, -2, "busy");
    lua_pushboolean(L, s.connected);
    lua_setfield(L, -2, "connected");
    lua_pushboolean(L, s.sharing);
    lua_setfield(L, -2, "sharing");
    lua_newtable(L);
    for (size_t i = 0; i < s.networks.size(); ++i) {
      const auto &ap = s.networks[i];
      lua_newtable(L);
      str(L, "ssid", ap.ssid);
      str(L, "security", ap.security);
      lua_pushinteger(L, ap.signal);
      lua_setfield(L, -2, "signal");
      lua_pushinteger(L, ap.channel);
      lua_setfield(L, -2, "channel");
      lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "networks");
    lua_pushinteger(L, s.bytes);
    lua_setfield(L, -2, "bytes");
    return 1;
  }
  if (op == "saved") {
    auto profiles = r->network_.saved();
    lua_newtable(L);
    for (size_t i = 0; i < profiles.size(); ++i) {
      lua_newtable(L);
      str(L, "ssid", profiles[i].ssid);
      lua_pushboolean(L, profiles[i].automatic);
      lua_setfield(L, -2, "automatic");
      lua_rawseti(L, -2, i + 1);
    }
    return 1;
  }
  size_t n = 0, m = 0;
  const char *a = luaL_optlstring(L, 2, "", &n),
             *b = luaL_optlstring(L, 3, "", &m);
  if (n > 1024 || m > 240 || memchr(a, 0, n) || memchr(b, 0, m))
    return luaL_error(L, "invalid network arguments");
  bool ok = false;
  std::string profile;
  if (op == "cancel") {
    r->network_.cancel();
    ok = true;
  } else if (op == "disconnect") {
    r->network_.disconnect();
    ok = true;
  } else if (op == "forget")
    ok = r->network_.forget(std::string(a, n));
  else if (op == "automatic")
    ok = r->network_.setAutomatic(std::string(a, n), std::string(b, m) == "on");
  else if (!(r->store_.read("/config/public/profile", profile) &&
             profile == "offline")) {
    if (op == "autoconnect")
      ok = r->network_.autoConnect(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count());
    else if (op == "connect_saved")
      ok = r->network_.connectSaved(std::string(a, n));
    else if (op == "scan")
      ok = r->network_.scan();
    else if (op == "connect")
      ok = r->network_.connect(std::string(a, n), std::string(b, m),
                               lua_toboolean(L, 4));
    else if (op == "share")
      ok = r->network_.share(std::string(a, n) == "on");
    else if (op == "download" || op == "feed")
      ok = r->permission(std::string(b, m), true) &&
           r->network_.download(std::string(a, n), std::string(b, m),
                                op == "feed");
  }
  lua_pushboolean(L, ok);
  return 1;
}
} // namespace tab5
