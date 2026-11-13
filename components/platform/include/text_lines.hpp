// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <utility>
namespace tab5 {
// Bounded manifest/TSV splitting without pulling locale/iostream into firmware.
class TextLines {
  std::string value_;
  size_t position_=0;
public:
  explicit TextLines(std::string value):value_(std::move(value)) {}
  bool read(std::string &line,char delimiter='\n') {
    line.clear();if(position_>=value_.size()) return false;
    auto end=value_.find(delimiter,position_);
    if(end==std::string::npos) end=value_.size();
    line.assign(value_,position_,end-position_);position_=end<value_.size()?end+1:end;return true;
  }
};
inline bool nextLine(TextLines &input,std::string &line,char delimiter='\n') {return input.read(line,delimiter);}
}
