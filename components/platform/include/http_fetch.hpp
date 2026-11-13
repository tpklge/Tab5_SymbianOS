// SPDX-License-Identifier: MIT
#pragma once
#include "network.hpp"
namespace tab5 {
class HttpStream {
public:
  virtual ~HttpStream() = default;
  virtual bool open(const std::string &, int &, std::string &,
                    std::string &) = 0;
  virtual int read(char *, size_t) = 0;
  virtual void close() = 0;
  virtual uint64_t nowMs() const {return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
  virtual std::string diagnostic() const { return {}; }
};
bool resolveHttpRedirect(const std::string &base, const std::string &location,
                         std::string &next, std::string &error);
bool fetchHttp(HttpStream &, const std::string &, const Network::Sink &,
               const std::atomic<bool> &, std::string &, std::string *resolved = nullptr,unsigned totalTimeoutMs=90000);
} // namespace tab5
