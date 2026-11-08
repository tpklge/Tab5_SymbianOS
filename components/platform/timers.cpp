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
namespace tab5 {
std::vector<ScheduledNotice> Runtime::pollTimers() {
  std::vector<ScheduledNotice> events;
  if (!timersEnabled_ || !store_.online())
    return events;
  int64_t epoch = now();
  if (epoch < 946684800)
    return events;
  std::lock_guard<std::mutex> lock(timerMutex_);
  for (auto &entry : timers_) {
    auto &t = entry.second;
    if (t.due > epoch)
      continue;
    int64_t due = t.due;
    if (t.repeat > 0)
      due += ((epoch - due) / t.repeat) * t.repeat;
    if (due <= t.last)
      continue;
    t.last = due;
    events.push_back({entry.first, t.text, t.alarm, t.sound, due});
  }
  return events;
}
bool Runtime::timerTriggered(const std::string &id, int64_t due) const {
  std::lock_guard<std::mutex> lock(timerMutex_);
  auto found = timers_.find(id);
  return found != timers_.end() && found->second.last >= due;
}
bool Runtime::acknowledgeTimer(const std::string &id, int64_t due) {
  std::lock_guard<std::mutex> lock(timerMutex_);
  auto active = timers_.find(id);
  bool already = active != timers_.end() && active->second.last >= due;
  if (active != timers_.end())
    active->second.last = std::max(active->second.last, due);
  auto stage = stagedTimers_.find(id);
  if (stage != stagedTimers_.end())
    stage->second.last = std::max(stage->second.last, due);
  return already;
}
void Runtime::emitSignal(bool alarm) {
  std::lock_guard<std::mutex> lock(audioMutex_);
  media_.interrupt(true);
  if (signal_)
    signal_(alarm);
  lastAudioResult_ = media_.device.diagnostic
                         ? media_.device.diagnostic()
                         : (signal_ ? "sent (unverified)" : "unavailable");
  if (media_.device.diagnostic)
    store_.write("/logs/last-audio.txt", lastAudioResult_ + "\n");
  media_.interrupt(false);
}
int Runtime::scheduleApi(lua_State *L) {
  auto *r = static_cast<Runtime *>(lua_touserdata(L, lua_upvalueindex(1)));
  if (!r->capability("notify"))
    return luaL_error(L, "scheduler capability denied");
  const char *op = luaL_checkstring(L, 1);
  if (strcmp(op, "begin") == 0) {
    r->stagedTimers_.clear();
    return 0;
  }
  if (strcmp(op, "commit") == 0) {
    std::lock_guard<std::mutex> lock(r->timerMutex_);
    for (auto &entry : r->stagedTimers_) {
      auto old = r->timers_.find(entry.first);
      if (old != r->timers_.end())
        entry.second.last = std::max(entry.second.last, old->second.last);
    }
    r->timers_ = std::move(r->stagedTimers_);
    return 0;
  }
  if (strcmp(op, "add") != 0)
    return luaL_error(L, "scheduler operation");
  size_t a, b;
  const char *id = luaL_checklstring(L, 2, &a),
             *text = luaL_checklstring(L, 3, &b);
  int64_t due = luaL_checkinteger(L, 4), repeat = luaL_checkinteger(L, 5),
          last = luaL_optinteger(L, 8, 0);
  bool alarm = lua_toboolean(L, 6), sound = lua_toboolean(L, 7);
  if (a > 100 || b > 240 || r->stagedTimers_.size() >= 512 || due < 946684800 ||
      due > 4102444799LL || (repeat != 0 && repeat != 86400) || last < 0)
    return luaL_error(L, "scheduler limit");
  r->stagedTimers_[std::string(id, a)] = {
      std::string(text, b), due, repeat, last, alarm, sound};
  return 0;
}
} // namespace tab5
