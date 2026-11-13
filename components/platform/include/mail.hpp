// SPDX-License-Identifier: MIT
#pragma once
#include "executor.hpp"
#include "mail_protocol.hpp"
#include "storage.hpp"
#include <map>
#include <mutex>
namespace tab5 {
class Mail {
  Storage &disk_;
  mutable std::mutex mutex_;
  std::unique_ptr<Executor> worker_;
  std::atomic<bool> cancel_{false}, completed_{false};
  bool loaded_ = false, busy_ = false, accountLoaded_ = false;
  MailAccount accountCache_;
  std::map<std::string, MailMessage> index_;
  std::vector<std::string> folders_{"INBOX"};
  std::string foldersAccount_;
  std::string message_ = "Offline; mensagens e rascunhos no SD";
  uint64_t nextAuto_ = 0;
  bool load();
  bool put(const MailMessage &);
  // Caller holds mutex_; does not mark seen or acquire the account lock.
  bool readStored(const std::string &, MailMessage &);
  std::string deletedKeys_;bool deletionsLoaded_=false;
  bool loadDeletions();
  bool isDeleted(const std::string&,const std::string&);
  bool deleted(const std::string&,const std::string&,bool);
  bool saveAttachment(const std::string&,const std::string&,const std::string&,const std::string&,const std::string&,MailAttachment&,std::string&);
  bool start(std::function<void()>);
  void finish(const std::string &);
  void report(const std::string &);
  void sendQueue(const MailAccount &,bool);

public:
  std::function<std::unique_ptr<MailWire>()> wire;
  std::function<bool()> recoverTransport;
  explicit Mail(Storage &d) : disk_(d) {}
  ~Mail() { stop(); }
  void stop();
  void cancel() { cancel_ = true; }
  void poll(uint64_t, bool online);
  struct Status {
    bool busy = false;
    std::string message;
    size_t queued = 0;
  };
  Status status();
  MailAccount account();
  bool configure(const MailAccount &, std::string &);
  std::vector<MailMessage> list(const std::string &);
  std::vector<std::string> folders();
  bool read(const std::string &, MailMessage &);
  bool draft(MailMessage &, bool queue, std::string &);
  bool move(const std::string &, const std::string &, std::string &);
  bool synchronize(const std::string &, std::string &, const std::string &target = {});
  bool refresh(const std::string &id,std::string &error);
  bool flush(std::string &,bool retryRejected=false);
  bool attach(const std::string&,const std::string&,std::string&);
  bool detach(const std::string&,unsigned,std::string&);
  bool exportAttachment(const std::string&,unsigned,std::string&);
};
} // namespace tab5
