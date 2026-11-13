// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace tab5 {
// Host-only cookies intentionally reject Domain= to avoid public-suffix ambiguity.
class BrowserCookies {
 struct Cookie {std::string host,path,name,value,sameSite;int64_t expiry=0;bool secure=false,httpOnly=false;};
 std::vector<Cookie> cookies_;
public:
 bool set(const std::string &url,const std::string &header,int64_t now,bool script=false);
 std::string header(const std::string &url,int64_t now,bool script=false,const std::string &initiator={},bool safe=true)const;
 std::string save()const;
 bool load(const std::string &,int64_t now);
 static std::string origin(const std::string&);
};
}
