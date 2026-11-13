// SPDX-License-Identifier: MIT
#pragma once
namespace tab5::browserProfile {
// Compatibility hints only: the renderer does not implement Android or JS.
inline constexpr char userAgent[]="Mozilla/5.0 (Linux; Android 13) AppleWebKit/537.36 (KHTML, like Gecko) Mobile Safari/537.36 Tab5SymbianOS/1.0.0";
inline constexpr char genericAgent[]="Mozilla/5.0 (compatible; Tab5SymbianOS/1.0.0)";
inline constexpr char cacheSalt[]="browser-web-v3\n";
inline constexpr char build[]="20261113-R3";
inline constexpr unsigned wireLimit=4*1024*1024;
inline constexpr unsigned width=592,height=176;
}
