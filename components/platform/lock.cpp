// SPDX-License-Identifier: MIT
#include "lock.hpp"
#include <algorithm>
#include <cstdlib>
namespace tab5 {
static bool pinOK(const std::string &p) {
  return p.size()==4 && p.find_first_not_of("0123456789")==std::string::npos;
}
std::string Lock::derive(const std::string &pin,const std::string &salt) {
  auto result=salt+":"+pin;
  for(unsigned i=0;i<4096;++i) result=sha256(result+salt);
  return result;
}
void Lock::load() {
  std::string data;
  enabled_=disk_.read("/config/security/pin",data,160);
  invalid_=false; salt_.clear();verifier_.clear();attempts_=0;
  if(!enabled_ && disk_.lastError()!="not found") enabled_=invalid_=true;
  if(enabled_) {
    if(data.size()!=104 || data.substr(0,7)!="T5PIN1\n" || data[39]!='\n') invalid_=true;
    else { salt_=data.substr(7,32);verifier_=data.substr(40,64); }
    std::string failures;
    if(disk_.read("/config/security/attempts",failures,5)) attempts_=unsigned(atoi(failures.c_str()));
    if(attempts_>=5) until_=std::chrono::steady_clock::now()+std::chrono::seconds(30);
  }
  locked_=enabled_.load();
}
unsigned Lock::wait() const {
  auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(until_-std::chrono::steady_clock::now()).count();
  return remaining>0?unsigned((remaining+999)/1000):0;
}
bool Lock::unlock(const std::string &pin) {
  if(!enabled_) { locked_=false; return true; }
  if(invalid_ || wait() || !pinOK(pin)) return false;
  const auto hash=derive(pin,salt_);
  unsigned mismatch=0;
  for(size_t i=0;i<hash.size();++i) mismatch|=hash[i]^verifier_[i];
  if(mismatch) {
    ++attempts_;
    disk_.write("/config/security/attempts",std::to_string(std::min(999u,attempts_)));
    if(attempts_>=5) until_=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    return false;
  }
  if(attempts_ && !disk_.write("/config/security/attempts","0")) return false;
  attempts_=0;locked_=false;return true;
}
bool Lock::configure(const std::string &oldPin,const std::string &newPin,const std::string &salt) {
  if((enabled_ && !unlock(oldPin)) || (!newPin.empty() && (!pinOK(newPin) || salt.size()!=32))) return false;
  if(!disk_.prepare("/config/security")) return false;
  if(newPin.empty()) {
    if(enabled_ && !disk_.remove("/config/security/pin")) return false;
    enabled_=false;locked_=false;return true;
  }
  auto hash=derive(newPin,salt);
  if(!disk_.write("/config/security/pin","T5PIN1\n"+salt+"\n"+hash)) return false;
  enabled_=true;invalid_=false;salt_=salt;verifier_=hash;return true;
}
}
