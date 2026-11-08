// SPDX-License-Identifier: MIT
#include "input.hpp"
#include "runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace tab5;
static int checks = 0;
static void check(bool b, const char *s) {
  ++checks;
  if (!b) {
    std::cerr << "FAIL: " << s << '\n';
    exit(1);
  }
}
static void put(const fs::path &p, const std::string &s) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f << s;
}
static std::string get(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char **argv) {
  check(argc == 2, "installation path");
  auto base =
      fs::temp_directory_path() / ("tab5-tests-" + std::to_string(getpid()));
  fs::create_directories(base);
  auto root = base / "SymbianOS";
  fs::create_directories(root / "data");
  Storage s(root.string(), 1000, 100);
  check(s.mount(), "empty mount");
  for (auto *p : {"../outside", "/../outside", "/data/../../outside",
                  "/data//x", "/data/x.", "/data/x ", "/data/a:b", "/data/a\\b",
                  "/data/A~1", "/data/x.t5-new"})
    check(!s.write(p, "x"), "reject path escape/alias");
  check(s.write("/data/a", "ação"), "UTF-8 write");
  std::string text;
  check(s.read("/data/a", text) && text == "ação", "UTF-8 read");
  check(s.write("/data/a", "replaced"), "atomic replace");
  check(s.mount() && s.read("/data/a", text) && text == "replaced",
        "reboot persistence");
  check(!s.write("/data/b", std::string(900, 'x')),
        "quota includes existing and reserve");
  check(s.remove("/data/a"), "delete");
  bool a = false, b = false;
  std::thread t1([&] { a = s.write("/data/one", std::string(600, '1')); });
  std::thread t2([&] { b = s.write("/data/two", std::string(600, '2')); });
  t1.join();
  t2.join();
  check(a != b && s.used() == 600, "concurrent peak reservations");
  fs::remove_all(root);
  fs::create_directories(root / "data");
  put(root / "data/a.t5-old", "previous");
  put(root / "data/a.t5-new", "partial");
  check(s.mount() && s.read("/data/a", text) && text == "previous",
        "recover interrupted replacement");
  check(!fs::exists(root / "data/a.t5-new"), "discard incomplete temp");
  put(base / "outside", "untouched");
  fs::create_symlink(base / "outside", root / "data/link");
  check(!s.mount(), "reject symlink at audit");
  check(get(base / "outside") == "untouched", "outside untouched");
  fs::remove(root / "data/link");
  check(s.mount(), "remount");
  fs::create_symlink(base, root / "data/escape");
  check(!s.write("/data/escape/outside", "bad"), "reject late symlink");
  fs::remove(root / "data/escape");
  fs::remove_all(root);
  fs::create_directories(root);
  put(root / "big", std::string(1100, 'b'));
  check(s.mount() && s.maintenance(), "over quota maintenance");
  check(!s.write("/x", "x"), "maintenance blocks growth");
  check(s.remove("/big") && !s.maintenance(), "maintenance permits deletion");
  fs::remove_all(root);
  fs::copy(argv[1], root, fs::copy_options::recursive);
  Storage sd(root.string());
  Runtime runtime(sd);
  check(runtime.boot(), runtime.error.c_str());
  check(runtime.current() == "shell", "external shell");
  check(runtime.dispatch("open:notes"), "open external notes");
  check(runtime.dispatch("save", "Olá, coração!"), "save note");
  check(get(root / "data/notes/document.txt") == "Olá, coração!",
        "saved within root");
  check(runtime.open("calculator") && runtime.dispatch("calculate", "2+3*4"),
        "calculator arithmetic");
  check(runtime.view().body == "14", "operator precedence");
  check(runtime.open("notes") && runtime.view().text == "Olá, coração!",
        "state across app switch");
  runtime.shutdown();
  check(runtime.boot() && runtime.open("notes") &&
            runtime.view().text == "Olá, coração!",
        "data survives runtime restart");
  check(runtime.open("calculator") && runtime.dispatch("calculate", "1/0") &&
            !runtime.faulted() && runtime.view().body == "Divisão por zero",
        "division by zero controlled failure");
  check(runtime.open("shell"), "calculator error does not stop shell");
  auto chromePath = root / "system/releases/0.8.0/chrome.lua";
  auto chrome = get(chromePath);
  auto updatedChrome = chrome;
  auto color = updatedChrome.find("26292e");
  updatedChrome.replace(color, 6, "eadfca");
  auto oldManifest = get(root / "boot/manifest.txt"),
       updatedManifest = oldManifest;
  auto hashAt = updatedManifest.find(sha256(chrome));
  updatedManifest.replace(hashAt, 64, sha256(updatedChrome));
  put(chromePath, updatedChrome);
  put(root / "boot/manifest.txt", updatedManifest);
  check(runtime.boot() && runtime.view().background == "eadfca",
        "external theme changes without firmware rebuild");
  put(chromePath, chrome);
  put(root / "boot/manifest.txt", oldManifest);
  std::string original = get(root / "system/releases/0.8.0/notes.lua");
  put(root / "system/releases/0.8.0/notes.lua", original + "\n-- modified");
  check(!runtime.boot(), "hash mismatch prevents boot");
  put(root / "system/releases/0.8.0/notes.lua", original);
  auto manifest = get(root / "boot/manifest.txt");
  auto incompatible = manifest;
  auto apiAt = incompatible.find("api_min=8");
  incompatible.replace(apiAt, 9, "api_min=9");
  put(root / "boot/manifest.txt", incompatible);
  check(!runtime.boot(), "incompatible API rejected");
  put(root / "boot/manifest.txt", manifest);
  auto addApp = [&](const std::string &script) {
    put(root / "system/releases/0.8.0/test.lua", script);
    put(root / "boot/manifest.txt",
        manifest + "app.test=test.lua\nfile=test.lua|" + sha256(script) + "\n");
  };
  addApp(std::string("\x1bLua", 4) + "binary");
  check(runtime.boot() && !runtime.open("test"), "external bytecode rejected");
  addApp("function event() while true do end end");
  check(runtime.boot(), "test fixture boot");
  check(!runtime.open("test") &&
            runtime.error.find("budget") != std::string::npos,
        "instruction watchdog");
  check(runtime.open("shell"), "recover after script error");
  addApp("function event() return {body=(pcall and 'unsafe' or "
         "'safe')..(setmetatable and 'unsafe' or 'safe')} end");
  check(runtime.boot() && runtime.open("test") &&
            runtime.view().body == "safesafe",
        "sandbox hides hook bypass and metatables");
  addApp("function event() return {body=string.rep('x',2000000)} end");
  check(runtime.boot() && !runtime.open("test"), "Lua memory limit");
  addApp("function event() return {body=sys.read('/../outside')} end");
  check(runtime.boot() && !runtime.open("test"), "script path confinement");
  fs::remove(root / "boot/manifest.txt");
  check(!runtime.boot(), "missing manifest recovery");
  Storage absent((base / "missing").string());
  Runtime missing(absent);
  check(!missing.boot(), "absent SD no fallback");
  fs::remove_all(root);
  fs::create_directories(root);
  put(root / "case", "x");
  Storage aliases(root.string());
  check(aliases.mount() && !aliases.write("/CASE", "wrong"),
        "reject case alias on host");
  fs::remove_all(root);
  fs::create_directories(root);
  {
    std::ofstream sparse(root / "large");
  }
  fs::resize_file(root / "large", 4000000001ULL);
  Storage large(root.string());
  check(large.mount() && large.used() == 4000000001ULL && large.maintenance(),
        "64-bit decimal quota audit");
  fs::create_directory_symlink(root, base / "rootlink");
  Storage linkedRoot((base / "rootlink").string() + "/");
  check(!linkedRoot.mount(), "root symlink with trailing separator rejected");
  Keyboard kb;
  check(kb.configure(
            get(fs::path(argv[1]) / "system/releases/0.8.0/keyboard.map")),
        "external keymap");
  check(kb.matrix(2, 1, true, 0)[0].text == "q", "matrix decoding");
  check(kb.matrix(2, 1, true, 1).empty(), "ignore duplicate press");
  check(kb.tick(499).empty() && kb.tick(500)[0].repeat, "repeat delay");
  check(!kb.matrix(2, 1, false, 501)[0].down && kb.tick(900).empty(),
        "release stops repeat");
  kb.matrix(3, 1, true, 1000);
  check(kb.matrix(3, 2, true, 1001)[0].text == "A", "shift");
  kb.reset();
  kb.matrix(3, 0, true, 0);
  check(kb.matrix(2, 3, true, 1)[0].text == "é", "Sym accents");
  kb.reset();
  check(kb.tick(10000).empty(), "disconnect resets keys");
  fs::remove_all(base);
  std::cout << checks << " checks passed\n";
}
