// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace tab5 {
struct MailAccount {
  std::string address, name, protocol = "imap", inHost, inUser, inPassword,
                             inSecurity = "tls", outHost, outUser, outPassword,
                             outSecurity = "tls";
  unsigned inPort = 993, outPort = 465;
  bool automatic = false;
};
struct MailAttachment {std::string name,type,path;uint64_t size=0;};
struct MailImage {std::string cid,type,data;};
struct MailMessage {
  std::string id, from, to, subject, body, date, uid, messageId,
      folder = "inbox", status, account;
  std::string html;
  std::vector<MailImage> images;
  std::vector<MailAttachment> attachments;
  bool seen = false;
  uint64_t order = 0;
};
class MailWire {
public:
  const std::atomic<bool> *cancellation = nullptr;
  std::function<void(const std::string &)> progress;
  bool cancelled() const { return cancellation && cancellation->load(); }
  unsigned protocolTimeoutMs = 30000;
  virtual std::string diagnostic() const { return {}; }
  virtual bool retryableConnectFailure() const { return false; }
  virtual ~MailWire() = default;
  virtual bool open(const std::string &, unsigned, bool, std::string &) = 0;
  virtual bool upgrade(const std::string &, std::string &) = 0;
  virtual int
  read(char *,
       size_t) = 0; // -2 = bounded timeout; protocol checks cancellation
  virtual int write(const char *, size_t) = 0;
};
namespace mail {
constexpr size_t maxRaw = 1024 * 1024, maxBody = 48 * 1024, maxMessages = 100;
bool validAccount(const MailAccount &, std::string &);
bool address(const std::string &);
std::string base64(const std::string &);
std::string unbase64(const std::string &);
std::string hex(const std::string &);
std::string unhex(const std::string &);
using AttachmentSink=std::function<bool(const std::string&,const std::string&,const std::string&,const std::string&,MailAttachment&)>;
using AttachmentReader=std::function<bool(const std::string&,uint64_t,size_t,std::string&)>;
MailMessage decodeMessage(const std::string &,const AttachmentSink &sink = {});
std::string encodeMessage(const MailAccount &, const MailMessage &);
using Known = std::function<bool(const std::string &)>;
using Delivery = std::function<bool(const std::string &, const std::string &)>;
bool receive(MailWire &, const MailAccount &, const std::string &,
             const Known &, const Delivery &, std::vector<std::string> &,
             const std::atomic<bool> &, std::string &, const std::string &target = {});
enum class SendResult { accepted, rejected, uncertain };
SendResult send(MailWire &, const MailAccount &, const MailMessage &,
                const std::atomic<bool> &, std::string &,const AttachmentReader &reader = {});
} // namespace mail
} // namespace tab5
