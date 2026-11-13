// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
namespace tab5 {
struct RadioFrame {uint16_t size=0;std::array<char,512> data{};};
class RadioInbox {
 RadioFrame *frames_=nullptr;size_t capacity_=0,head_=0,count_=0;
public:
 uint64_t dropped=0;
 void attach(RadioFrame *frames,size_t capacity) {frames_=frames;capacity_=capacity;clear();}
 void clear() {head_=count_=0;dropped=0;}
 bool push(const char *data,size_t n) {
  if(!n || n>512 || !frames_ || count_==capacity_) {++dropped;return false;}
  auto &frame=frames_[(head_+count_)%capacity_];frame.size=uint16_t(n);memcpy(frame.data.data(),data,n);++count_;return true;
 }
 bool pop(std::string &out) {if(!count_)return false;auto &frame=frames_[head_];out.assign(frame.data.data(),frame.size);head_=(head_+1)%capacity_;--count_;return true;}
 size_t size() const {return count_;}
};
class SerialFrames {
 unsigned state_=0,expected_=0,used_=0;
 std::array<char,512> data_{};
public:
 uint64_t invalid=0,frames=0;
 void reset() {state_=expected_=used_=0;invalid=frames=0;}
 template<class Sink> void feed(const char *data,size_t n,Sink sink) {
  for(size_t i=0;i<n;++i) {
   auto byte=uint8_t(data[i]);
   if(state_==0) {if(byte==0x94)state_=1;}
   else if(state_==1) {state_=byte==0xc3?2:byte==0x94?1:0;}
   else if(state_==2) {expected_=unsigned(byte)<<8;state_=3;}
   else if(state_==3) {expected_|=byte;used_=0;if(!expected_ || expected_>512) {++invalid;state_=byte==0x94?1:0;}else state_=4;}
   else {data_[used_++]=char(byte);if(used_==expected_) {sink(data_.data(),used_);++frames;state_=0;}}
  }
 }
};
}
