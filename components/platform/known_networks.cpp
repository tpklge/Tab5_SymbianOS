// SPDX-License-Identifier: MIT
#include "network.hpp"
#include <algorithm>
#include <sstream>
namespace tab5 {
namespace {
constexpr const char *profilePath = "/config/network/known-wifi.tsv";
std::string encode(const std::string &s) {
  std::string out;
  const char *hex = "0123456789abcdef";
  for (unsigned char c : s) {
    out += hex[c >> 4];
    out += hex[c & 15];
  }
  return out;
}
bool decode(const std::string &s, std::string &out) {
  if (s.size() % 2)
    return false;
  out.clear();
  auto nib = [](char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
  };
  for (size_t i = 0; i < s.size(); i += 2) {
    int a = nib(s[i]), b = nib(s[i + 1]);
    if (a < 0 || b < 0)
      return false;
    out += char(a * 16 + b);
  }
  return out.find('\0') == std::string::npos;
}
} // namespace
bool Network::loadProfiles(std::vector<Profile> &list) {
  list.clear();
  std::string data;
  if (!disk_.read(profilePath, data, 8192))
    return disk_.lastError() == "not found";
  if (data.rfind("T5WIFI1\n", 0) != 0 || data.back() != '\n')
    return false;
  std::istringstream input(data.substr(8));
  std::string row;
  while (std::getline(input, row)) {
    auto a = row.find('\t'),
         b = row.find('\t', a == std::string::npos ? 0 : a + 1);
    if (a == std::string::npos || b == std::string::npos ||
        row.find('\t', b + 1) != std::string::npos)
      return false;
    Profile p;
    auto flag = row.substr(b + 1);
    if (!decode(row.substr(0, a), p.ssid) ||
        !decode(row.substr(a + 1, b - a - 1), p.password) || p.ssid.empty() ||
        p.ssid.size() > 32 ||
        p.ssid.find_first_of("\r\n") != std::string::npos ||
        p.password.size() > 63 || (flag != "0" && flag != "1") ||
        list.size() >= 16)
      return false;
    for (auto &existing : list)
      if (existing.ssid == p.ssid)
        return false;
    p.automatic = flag == "1";
    list.push_back(std::move(p));
  }
  return true;
}
bool Network::storeProfiles(const std::vector<Profile> &profiles) {
  std::string data = "T5WIFI1\n";
  for (auto &p : profiles)
    data += encode(p.ssid) + "\t" + encode(p.password) + "\t" +
            (p.automatic ? "1" : "0") + "\n";
  return profiles.size() <= 16 && disk_.write(profilePath, data);
}
bool Network::remember(const std::string &ssid, const std::string &password) {
  std::lock_guard<std::mutex> lock(profilesMutex_);
  std::vector<Profile> profiles;
  if (!loadProfiles(profiles))
    return false;
  profiles.erase(
      std::remove_if(profiles.begin(), profiles.end(),
                     [&](const Profile &p) { return p.ssid == ssid; }),
      profiles.end());
  if (profiles.size() >= 16)
    return false;
  profiles.insert(profiles.begin(), {ssid, password, true});
  return storeProfiles(profiles);
}
std::vector<Network::SavedNetwork> Network::saved() {
  std::lock_guard<std::mutex> lock(profilesMutex_);
  std::vector<Profile> profiles;
  std::vector<SavedNetwork> result;
  if (loadProfiles(profiles))
    for (auto &p : profiles)
      result.push_back({p.ssid, p.automatic});
  return result;
}
bool Network::connectSaved(const std::string &ssid) {
  std::vector<Profile> profiles;
  {
    std::lock_guard<std::mutex> lock(profilesMutex_);
    if (!loadProfiles(profiles))
      return false;
  }
  for (auto &p : profiles)
    if (p.ssid == ssid)
      return connect(p.ssid, p.password, false);
  return false;
}
bool Network::forget(const std::string &ssid) {
  if (status().busy)
    return false;
  std::lock_guard<std::mutex> lock(profilesMutex_);
  std::vector<Profile> profiles;
  if (!loadProfiles(profiles))
    return false;
  auto size = profiles.size();
  profiles.erase(
      std::remove_if(profiles.begin(), profiles.end(),
                     [&](const Profile &p) { return p.ssid == ssid; }),
      profiles.end());
  return profiles.size() != size && storeProfiles(profiles);
}
bool Network::setAutomatic(const std::string &ssid, bool enabled) {
  std::lock_guard<std::mutex> lock(profilesMutex_);
  std::vector<Profile> profiles;
  if (!loadProfiles(profiles))
    return false;
  for (auto &p : profiles)
    if (p.ssid == ssid) {
      p.automatic = enabled;
      return storeProfiles(profiles);
    }
  return false;
}
bool Network::autoConnect(uint64_t nowMs) {
  if (autoSuppressed_ || nowMs < nextAutoMs_)
    return false;
  auto state = status();
  if (state.busy || state.connected)
    return false;
  nextAutoMs_ = nowMs + 30000;
  std::vector<Profile> profiles;
  {
    std::lock_guard<std::mutex> lock(profilesMutex_);
    if (!loadProfiles(profiles))
      return false;
  }
  profiles.erase(std::remove_if(profiles.begin(), profiles.end(),
                                [](const Profile &p) { return !p.automatic; }),
                 profiles.end());
  if (profiles.empty())
    return false;
  automaticJob_ = true;
  bool accepted = start([this, profiles] {
    std::vector<AccessPoint> visible;
    std::vector<Profile> candidates;
    bool scanned = !backend.scan || backend.scan(visible, cancel_);
    if (scanned && !cancel_) {
      for (auto &p : profiles)
        for (auto &ap : visible)
          if (ap.ssid == p.ssid) {
            candidates.push_back(p);
            break;
          }
      // A remembered hidden SSID may not appear in a scan: try the most recent.
      if (candidates.empty())
        candidates.push_back(profiles.front());
    }
    bool ok = false;
    std::string address, ssid;
    for (auto &p : candidates) {
      if (cancel_)
        break;
      if (backend.connect &&
          backend.connect(p.ssid, p.password, address, cancel_) && !cancel_) {
        ok = true;
        ssid = p.ssid;
        break;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      status_.connected = ok;
      status_.address = ok ? address : "";
      status_.ssid = ssid;
    }
    finish(ok, ok ? "Conectado automaticamente"
               : backend.diagnostic
                   ? backend.diagnostic()
                   : "Rede conhecida indisponível; nova tentativa em 30 s");
    automaticJob_ = false;
  });
  if (!accepted)
    automaticJob_ = false;
  return accepted;
}
std::string Network::transferToken(const std::array<uint8_t, 8> &random) {
  constexpr const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  static_assert(sizeof(alphabet) == 33, "uniform token alphabet");
  std::string result;
  for (auto b : random)
    result += alphabet[b & 31];
  return result;
}
} // namespace tab5
