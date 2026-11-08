// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
namespace tab5 {
struct PowerInfo {
  bool available = false, charging = false, enabled = false, fast = false;
  int percent = -1, millivolts = 0, milliamps = 0;
  std::string error;
};
inline int batteryPercent(int mv) {
  const int volts[] = {6000, 6600, 7000, 7300, 7500, 7700, 7900, 8100, 8400};
  const int levels[] = {0, 5, 10, 25, 40, 55, 70, 85, 100};
  if (mv <= volts[0])
    return 0;
  for (unsigned i = 1; i < 9; ++i)
    if (mv <= volts[i])
      return levels[i - 1] + (mv - volts[i - 1]) * (levels[i] - levels[i - 1]) /
                                 (volts[i] - volts[i - 1]);
  return 100;
}
} // namespace tab5
