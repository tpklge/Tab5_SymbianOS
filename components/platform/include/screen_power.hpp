// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace tab5 {
inline int screenBrightness(unsigned minutes, uint32_t inactiveMs) {
  if(!minutes) return 100;
  const uint32_t deadline=minutes*60000u;
  if(inactiveMs>=deadline) return 0;
  return inactiveMs>=deadline-15000u ? 20 : 100;
}
inline bool validScreenMinutes(unsigned minutes) {
  return minutes==0 || minutes==1 || minutes==2 || minutes==5 || minutes==10 || minutes==30;
}
}
