// SPDX-License-Identifier: MIT
#pragma once
#include "installer.hpp"
#include "media.hpp"
#include "network.hpp"
#include "power.hpp"
#include "storage.hpp"
#include <chrono>
#include <ctime>
#include <deque>
#include <functional>
#include <map>
#include <memory>
struct lua_State;
struct lua_Debug;
namespace tab5 {
struct Button {
  std::string id, label, subtitle, icon;
  std::shared_ptr<const Picture> thumbnail;
};
struct ScheduledNotice {
  std::string id, text;
  bool alarm = false, sound = false;
  int64_t due = 0;
};
struct Notice {
  std::string id, text;
  bool alarm = false;
};
struct FormControl {
  std::string label, type, options, keys;
};
struct View {
  std::string left, right, home, menu, switcher, keyboard;
  std::string layout, fields, result;
  std::string title, body, text, background = "292b2f", accent = "454a52",
                                 foreground = "eeeeee", fieldColor = "34373c";
  std::vector<Button> buttons, tools;
  std::vector<FormControl> controls;
  std::shared_ptr<const Picture> picture;
  bool field = false, classic = false, list = false, readonly = false,
       mediaStatus = false;
};
class Runtime {
  struct VM {
    lua_State *L = nullptr;
    size_t bytes = 0;
    std::string id;
    Runtime *owner = nullptr;
    unsigned budget = 100000;
    bool faulted = false;
    uint64_t touched = 0, generation = 0;
    View last;
    ~VM();
  };
  Storage &store_;
  Media media_;
  Network network_;
  std::map<std::string, Package> installed_;
  std::map<std::string, std::shared_ptr<const Picture>> thumbnails_;
  std::map<std::string, std::shared_ptr<const Picture>> pictures_;
  std::string reviewedPath_, reviewedHash_, pictureOwner_;
  bool refreshInstalled();
  std::map<std::string, std::unique_ptr<VM>> apps_;
  std::map<std::string, std::string> entries_;
  std::map<std::string, std::vector<std::string>> grants_;
  std::map<std::string, std::string> caps_;
  std::vector<File> apiEntries_;
  unsigned moduleDepth_ = 0;
  size_t luaBytes_ = 0;
  std::string current_, release_, apiPath_, apiScratch_;
  std::map<std::string, std::string> verified_;
  View view_;
  bool ready_ = false;
  uint64_t serial_ = 0;
  std::atomic<bool> cancelled_{false};
  std::vector<Notice> notices_;
  struct Message {
    std::string target, event, value;
    uint64_t serial;
  };
  std::deque<Message> messages_;
  mutable std::mutex clockMutex_, timerMutex_, audioMutex_;
  struct Timer {
    std::string text;
    int64_t due = 0, repeat = 0, last = 0;
    bool alarm = false, sound = false;
  };
  std::map<std::string, Timer> timers_, stagedTimers_;
  std::atomic<bool> timersEnabled_{false};
  int64_t clockBase_ = 0;
  std::chrono::steady_clock::time_point clockStart_ =
      std::chrono::steady_clock::now();
  std::function<bool(int64_t)> clockWriter_;
  std::function<void(bool)> signal_;
  std::string lastAudioResult_;
  std::function<bool(bool)> offlineWriter_;
  bool invoke(VM &, const std::string &, const std::string &,
              bool render = true);
  bool load(const std::string &);
  bool background(const std::string &, const std::string &,
                  const std::string &);
  static void *allocate(void *, void *, size_t, size_t);
  static void hook(lua_State *, lua_Debug *);
  static int resourceApi(lua_State *);
  static int mediaApi(lua_State *);
  static int powerApi(lua_State *);
  static int networkApi(lua_State *);
  static int packageApi(lua_State *);
  static int catalogApi(lua_State *);
  static int themeApi(lua_State *);
  static int appInfoApi(lua_State *);
  static int appsApi(lua_State *);
  static int moduleApi(lua_State *);
  static int readApi(lua_State *);
  static int writeApi(lua_State *);
  static int timeApi(lua_State *);
  static int scheduleApi(lua_State *);
  static int dismissApi(lua_State *);
  static int offlineApi(lua_State *);
  static int idApi(lua_State *);
  static int fsApi(lua_State *);
  static int usageApi(lua_State *);
  static int closeApi(lua_State *);
  static int notifyApi(lua_State *);
  static int noticesApi(lua_State *);
  static int sendApi(lua_State *);
  static int epochApi(lua_State *);
  static int dateApi(lua_State *);
  static int setClockApi(lua_State *);
  static int zoneApi(lua_State *);
  bool parseView(lua_State *);
  bool permission(const std::string &, bool write = false) const;
  bool capability(const std::string &) const;

  mutable std::mutex powerMutex_;
  PowerInfo power_;
  std::function<bool(const std::string &)> powerWriter_;

public:
  PowerInfo powerInfo() const {
    std::lock_guard<std::mutex> lock(powerMutex_);
    return power_;
  }
  void setPowerInfo(const PowerInfo &p) {
    std::lock_guard<std::mutex> lock(powerMutex_);
    power_ = p;
  }
  void setPowerWriter(std::function<bool(const std::string &)> fn) {
    powerWriter_ = std::move(fn);
  }
  std::string error;
  explicit Runtime(Storage &s) : store_(s), media_(s), network_(s) {
    setClock(std::time(nullptr));
  }
  ~Runtime();
  bool asset(const std::string &, std::string &, size_t maximum = 1024 * 1024);
  bool boot();
  bool dispatch(const std::string &, const std::string &value = "");
  bool open(const std::string &);
  bool close(const std::string &);
  bool tick();
  void shutdown();
  uint64_t beginOperation() {
    cancelled_ = false;
    return ++serial_;
  }
  void cancel() { cancelled_ = true; }
  std::vector<ScheduledNotice> pollTimers();
  bool timerTriggered(const std::string &, int64_t) const;
  bool acknowledgeTimer(const std::string &, int64_t);
  void emitSignal(bool);
  std::string audioResult() {
    std::lock_guard<std::mutex> lock(audioMutex_);
    return lastAudioResult_;
  }
  std::string clockText() const;
  void applyZone(const char *);
  int64_t now() const;
  void setClock(int64_t);
  void setClockWriter(std::function<bool(int64_t)> fn) { clockWriter_ = fn; }
  void setSignal(std::function<void(bool)> fn) { signal_ = fn; }
  void setOfflineWriter(std::function<bool(bool)> fn) { offlineWriter_ = fn; }
  const std::vector<Notice> &notices() const { return notices_; }
  const View &view() const { return view_; }
  const std::string &current() const { return current_; }
  bool ready() const { return ready_; }
  bool faulted() const {
    auto it = apps_.find(current_);
    return it != apps_.end() && it->second->faulted;
  }
  Media &media() { return media_; }
  Storage &storage() { return store_; }
  Network &network() { return network_; }
};
} // namespace tab5
