// SPDX-License-Identifier: MIT
#pragma once
namespace tab5 {
inline int wifiLevel(bool connected,bool valid,int rssi) {
 if(!connected) return -1;
 if(!valid) return 0;
 return rssi>=-55?4:rssi>=-65?3:rssi>=-75?2:rssi>=-85?1:0;
}
}
