// SPDX-License-Identifier: MIT
#pragma once
#include "lvgl.h"
#include <algorithm>
namespace tab5 {
// Resident fallback only: no SD, filesystem, Lua or external font dependency.
class BootScreen {
 lv_obj_t *root_=nullptr,*stage_=nullptr,*bar_=nullptr;
public:
 void show(unsigned percent,const char *stage) {
  if(!root_) {
   root_=lv_obj_create(nullptr);lv_obj_set_style_bg_color(root_,lv_color_hex(0x242428),0);lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
   auto *title=lv_label_create(root_);lv_label_set_text(title,"Carregando...");lv_obj_set_style_text_color(title,lv_color_hex(0xf0f2f5),0);
   lv_obj_set_style_transform_pivot_x(title,LV_PCT(50),0);lv_obj_set_style_transform_pivot_y(title,LV_PCT(50),0);lv_obj_set_style_transform_scale_x(title,512,0);lv_obj_set_style_transform_scale_y(title,512,0);lv_obj_align(title,LV_ALIGN_CENTER,0,-42);
   bar_=lv_bar_create(root_);lv_obj_set_size(bar_,420,8);lv_obj_align(bar_,LV_ALIGN_CENTER,0,10);lv_bar_set_range(bar_,0,100);lv_obj_set_style_bg_color(bar_,lv_color_hex(0x42424a),LV_PART_MAIN);lv_obj_set_style_bg_color(bar_,lv_color_hex(0x2acfe0),LV_PART_INDICATOR);
   stage_=lv_label_create(root_);lv_obj_set_style_text_color(stage_,lv_color_hex(0xbfc4cd),0);lv_screen_load(root_);
  }
  if(lv_screen_active()!=root_)lv_screen_load(root_);
  lv_label_set_text(stage_,stage);lv_obj_align(stage_,LV_ALIGN_CENTER,0,42);lv_bar_set_value(bar_,int(std::min(percent,100u)),LV_ANIM_OFF);lv_refr_now(nullptr);
 }
 void release() {if(root_ && lv_obj_is_valid(root_) && root_!=lv_screen_active())lv_obj_delete(root_);root_=stage_=bar_=nullptr;}
};
}
