// SPDX-License-Identifier: MIT
#pragma once
#include "lvgl.h"
#include <string>
namespace tab5 {
// LVGL's setter inserts one character at a time when max_length is nonzero,
// doing repeated length/layout work. Pre-truncate once and use its bulk path.
// User input still uses the original limit; accepted-character masks are kept.
inline void setEditorText(lv_obj_t *object,const std::string &input) {
  const auto limit=lv_textarea_get_max_length(object);
  if(lv_textarea_get_accepted_chars(object)) {lv_textarea_set_text(object,input.c_str());return;}
  auto text=input;
  if(limit){size_t end=0;uint32_t count=0;while(end<text.size() && count++<limit){++end;while(end<text.size() && (static_cast<unsigned char>(text[end])&0xc0)==0x80)++end;}if(end<text.size())text.resize(end);}
  lv_textarea_set_max_length(object,0);
  lv_textarea_set_text(object,text.c_str());
  lv_textarea_set_max_length(object,limit);
}
}
