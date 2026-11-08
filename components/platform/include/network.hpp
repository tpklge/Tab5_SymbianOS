// SPDX-License-Identifier: MIT
#pragma once
#include "executor.hpp"
#include "storage.hpp"
#include <atomic>
#include <memory>
namespace tab5 {
// Only this service's worker invokes slow backend operations. UI reads
// snapshots.
class Network {
public:
  struct AccessPoint {
    std::string ssid, security;
    int signal = 0, channel = 0;
  };
  struct Status {
    bool busy = false, connected = false, sharing = false;
    uint64_t bytes = 0;
    std::string message = "Offline", address, token;
    std::vector<AccessPoint> networks;
  };
  using Sink = std::function<bool(const char *, size_t)>;
  struct Backend {
    std::function<bool(const std::string &, const std::string &, std::string &,
                       const std::atomic<bool> &)>
        connect;
    std::function<bool(std::vector<AccessPoint> &, const std::atomic<bool> &)>
        scan;
    std::function<std::string()> diagnostic;
    std::function<void()> disconnect;
    std::function<bool(bool, std::string &, std::string &)> share;
    std::function<bool(const std::string &, const Sink &,
                       const std::atomic<bool> &, std::string &)>
        fetch;
    std::function<bool()> linked;
  } backend;

private:
  Storage &disk_;
  std::unique_ptr<Executor> worker_;
  mutable std::mutex mutex_;
  std::mutex transferMutex_;
  Status status_;
  std::atomic<bool> cancel_{false}, completed_{false};
  bool start(std::function<void()>);
  void finish(bool, const std::string &);

public:
  explicit Network(Storage &s) : disk_(s) {}
  ~Network();
  Status status() const;
  bool connect(const std::string &, const std::string &);
  bool share(bool);
  bool scan();
  bool download(const std::string &, const std::string &, bool feed = false);
  void cancel() { cancel_ = true; }
  void disconnect();
  static bool validUrl(const std::string &);
  static bool sharedPath(const std::string &);
  bool receive(const std::string &, const std::function<int(char *, size_t)> &,
               size_t);
  bool send(const std::string &, const Sink &);
};
} // namespace tab5
