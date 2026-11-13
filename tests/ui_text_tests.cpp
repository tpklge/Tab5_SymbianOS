// SPDX-License-Identifier: MIT
#include "ui_text.hpp"
#include <cassert>
#include <iostream>
using namespace tab5;
void count(lv_event_t *e){++*static_cast<unsigned*>(lv_event_get_user_data(e));}
int main(){
 lv_init();auto *display=lv_display_create(1280,720);auto *area=lv_textarea_create(lv_screen_active());lv_obj_set_size(area,800,300);
 std::string message;for(unsigned i=0;i<2000;++i)message+="á";
 unsigned events=0;lv_textarea_set_max_length(area,4096);lv_obj_add_event_cb(area,count,LV_EVENT_VALUE_CHANGED,&events);
 setEditorText(area,message);assert(events<=2 && message==lv_textarea_get_text(area) && lv_textarea_get_max_length(area)==4096);
 events=0;lv_textarea_set_max_length(area,1000);setEditorText(area,message);assert(events<=2 && std::string(lv_textarea_get_text(area)).size()==2000 && lv_textarea_get_max_length(area)==1000);
 lv_textarea_add_text(area,"x");assert(std::string(lv_textarea_get_text(area)).size()==2000);
 lv_textarea_set_max_length(area,4);lv_textarea_set_accepted_chars(area,"0123456789");setEditorText(area,"1a2b345");assert(std::string(lv_textarea_get_text(area))=="1234");
 lv_obj_delete(area);lv_display_delete(display);std::cout<<"Bulk UTF-8 textarea updates keep limits/masks and avoid per-character events\n";
}
