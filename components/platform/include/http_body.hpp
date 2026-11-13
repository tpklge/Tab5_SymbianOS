// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace tab5 {
struct HttpBody {
  static constexpr int again=-2;
  int64_t length=-1;
  uint64_t bytes=0,lastProgress=0;
  bool chunked=false;
  int accept(int n,bool complete,bool transient,bool cleanClose,uint64_t now) {
    if(chunked)length=-1; // Chunk framing takes precedence over Content-Length.
    if(n>0) {
      bytes+=unsigned(n);lastProgress=now;
      return length>=0 && bytes>uint64_t(length)?-1:n;
    }
    if(length>=0 && bytes==uint64_t(length)) return 0;
    if(complete) return length<0?0:-1;
    if(!chunked && length<0 && cleanClose) return 0;
    if(transient && now-lastProgress<30000) return again;
    return -1;
  }
};
}
