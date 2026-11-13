// SPDX-License-Identifier: MIT
#pragma once
#include "executor.hpp"
#include "storage.hpp"
#include <array>
#include <atomic>
#include <memory>
namespace tab5 {
// Only this service's worker invokes slow backend operations. UI reads
// snapshots.
class Network {
public:
  static constexpr size_t maxUploadBytes = 64u * 1024u * 1024u;
  struct PageStatus {bool busy=false,ok=false;uint64_t bytes=0;std::string url,message;};
  struct AccessPoint {
    std::string ssid, security;
    int signal = 0, channel = 0;
  };
  struct SavedNetwork {
    std::string ssid;
    bool automatic = true;
  };
  struct Status {
    bool busy = false, connected = false, sharing = false;
    uint64_t bytes = 0;
    int signal = -127;
    bool signalValid = false, restartRequired = false;
    std::string message = "Offline", address, transferAddress, token, ssid;
    std::vector<AccessPoint> networks;
  };
  using Sink = std::function<bool(const char *, size_t)>;
  struct BrowserRequest {
    std::string method="GET",body,initiator;bool legacyTls=false;
    std::function<std::string(const std::string&,bool)> cookies;
    std::function<void(const std::string&,const std::string&)> setCookie;
  };
  struct Backend {
    std::function<bool(const std::string &, const std::string &, std::string &,
                       const std::atomic<bool> &)>
        connect;
    std::function<bool(std::vector<AccessPoint> &, const std::atomic<bool> &)>
        scan;
    std::function<std::string()> diagnostic;
    std::function<void()> disconnect;
    std::function<bool(const std::atomic<bool> &, std::string &)> recover;
    std::function<bool(bool, std::string &, std::string &)> share;
    std::function<std::string()> shareDiagnostic;
    std::function<bool(const std::string &, const Sink &,
                       const std::atomic<bool> &, std::string &)>
        fetch;
    std::function<bool(const std::string &, const Sink &, const std::atomic<bool> &, std::string &, std::string &)> fetchDocument;
    decltype(fetch) fetchBrowserAsset;
    std::function<bool(const std::string&,const BrowserRequest&,const Sink&,const std::atomic<bool>&,std::string&,std::string&)> browserFetch;
    // Nonblocking snapshot: native transport cannot be recovered in place.
    std::function<bool()> restartRequired;
    std::function<bool()> linked;
    std::function<int()> signal;
  } backend;

private:
  Storage &disk_;
  std::unique_ptr<Executor> worker_;
  mutable std::mutex mutex_;
  std::mutex transferMutex_;
  std::string uploadId_, uploadPath_, uploadHash_;
  size_t uploadOffset_ = 0, uploadTotal_ = 0, uploadLastOffset_ = 0;
  uint64_t uploadTime_ = 0;
  uint64_t uploadGeneration_ = 0;
  bool uploadComplete_ = false;
  Status status_;
  PageStatus page_;uint64_t pageSerial_=0;
  bool wantedSharing_ = false;
  std::string lastSsid_, lastPassword_;
  uint64_t lastRecoveryMs_ = 0;
  std::atomic<unsigned> activeTransfers_{0};
  std::atomic<bool> recovering_{false};
  std::atomic<bool> cancel_{false}, completed_{false};
  struct Profile {
    std::string ssid, password;
    bool automatic = true;
  };
  mutable std::mutex profilesMutex_;
  bool loadProfiles(std::vector<Profile> &);
  bool storeProfiles(const std::vector<Profile> &);
  bool remember(const std::string &, const std::string &);
  std::atomic<bool> autoSuppressed_{false};
  std::atomic<bool> automaticJob_{false};
  uint64_t nextAutoMs_ = 0, nextSignalMs_ = 0;
  void pollSignal(uint64_t);
  bool start(std::function<void()>, bool recovery = false);
  void finish(bool, const std::string &);

public:
  explicit Network(Storage &s) : disk_(s) {}
  ~Network();
  Status status() const;
  PageStatus pageStatus(const std::string &url)const {std::lock_guard<std::mutex> lock(mutex_);return page_.url==url?page_:PageStatus{};}
  bool connect(const std::string &, const std::string &, bool remember = false);
  std::vector<SavedNetwork> saved();
  bool recover(bool automatic = false);
  bool connectSaved(const std::string &);
  bool forget(const std::string &);
  bool setAutomatic(const std::string &, bool);
  bool autoConnect(uint64_t nowMs, bool refreshSignal = true);
  void resumeAutomatic() {
    autoSuppressed_ = false;
    nextAutoMs_ = 0;
  }
  static std::string transferToken(const std::array<uint8_t, 8> &);
  bool share(bool);
  bool scan();
  bool documentAssets(const std::vector<std::pair<std::string,bool>>&,bool mobile=false);
  bool images(const std::string &html,const std::string &base);
  static std::string imageCachePath(const std::string &url,bool mobile=false);
  static std::string pageCachePath(const std::string &url);
  bool browse(const std::string &,const std::string &method="GET",const std::string &body={},const std::string &initiator={});
  bool download(const std::string &, const std::string &, bool feed = false);
  void cancel() { cancel_ = true; }
  void disconnect();
  static bool validUrl(const std::string &);
  static bool sharedPath(const std::string &);
  bool receive(const std::string &, const std::function<int(char *, size_t)> &,
               size_t,
               const std::function<void(const char *, size_t)> &progress = {});
  bool removeShared(const std::string &, const std::string &session, std::string &error);
  bool send(const std::string &, const Sink &);
  // Acknowledged, idempotent 64 KiB blocks; interrupted blocks never reach SD.
  bool receivePart(const std::string &id, const std::string &path,
                   size_t offset, size_t total, size_t length,
                   const std::function<int(char *, size_t)> &,
                   std::string &error);
};
} // namespace tab5
