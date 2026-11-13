// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace tab5;
static std::atomic<int> checks{0};
static void check(bool ok, const std::string &message) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << message << "\n";
    exit(1);
  }
}
static std::string get(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
static bool wait(const std::function<bool()> &predicate) {
  auto start = std::chrono::steady_clock::now();
  while (!predicate()) {
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(3))
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return true;
}
static unsigned le(const std::string &s, size_t at) {
  return uint8_t(s[at]) | (uint8_t(s[at + 1]) << 8) |
         (uint8_t(s[at + 2]) << 16) | (uint8_t(s[at + 3]) << 24);
}
static std::string wav() {
  std::string s(44 + 32000, '\0');
  auto put = [&](size_t at, unsigned n, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      s[at + i] = n & 255;
      n >>= 8;
    }
  };
  s.replace(0, 4, "RIFF");
  s.replace(8, 8, "WAVEfmt ");
  put(4, s.size() - 8, 4);
  put(16, 16, 4);
  put(20, 1, 2);
  put(22, 1, 2);
  put(24, 16000, 4);
  put(28, 32000, 4);
  put(32, 2, 2);
  put(34, 16, 2);
  s.replace(36, 4, "data");
  put(40, 32000, 4);
  return s;
}
int main(int argc, char **argv) {
  check(argc == 4, "fixture arguments");
  setenv("TZ", "<-04>4", 1);
  tzset();
  auto base = fs::temp_directory_path() /
              ("tab5-media-sdk-" + std::to_string(getpid()));
  auto root = base / "SymbianOS";
  fs::create_directories(base);
  fs::copy(argv[1], root, fs::copy_options::recursive);
  {
    Storage disk(root.string());
    Runtime r(disk);
    check(r.boot(), "boot API4: " + r.error);
    auto open = [&](const std::string &id) {
      r.beginOperation();
      check(r.open(id), "open " + id + ": " + r.error);
    };
    auto action = [&](const std::string &id, const std::string &text = "") {
      r.beginOperation();
      check(r.dispatch(id, text), "event " + id + ": " + r.error);
    };
    open("converter");
    check(r.view().controls.size() == 3 &&
              r.view().controls[1].type == "select" &&
              r.view().controls[2].keys.find("km") != std::string::npos,
          "unit dropdown schema");
    action("convert", "1,5\nkm\nm");
    check(r.view().body.find("1500 m") != std::string::npos,
          "Brazilian decimal conversion");
    open("clock");
    action("adjust");
    check(
        r.view().controls.size() == 2 && r.view().controls[0].type == "date" &&
            r.view().controls[1].type == "time" && r.view().text.find('/') == 2,
        "date/time split with Brazilian format");
    auto before = r.now();
    action("set_time", "31/02/2026\n14:30");
    check(r.view().body == "Data inválida." && r.now() - before < 2,
          "invalid calendar date does not change clock");
    action("set_time", "08/11/2026\n14:30");
    check(r.clockText().find("08/11/2026  14:30") != std::string::npos,
          "Brazilian clock adjustment");
    action("back");
    action("new");
    action("save", "Alarme BR\n09/11/2026\n07:15\ndiário");
    check(get(root / "data/organizer/alarms.tsv").find("86400") !=
              std::string::npos,
          "Brazilian alarm with daily dropdown");
    open("files");
    check(r.view().layout == "files", "file manager list layout");
    std::atomic<int> writes{0}, reads{0}, signals{0};
    std::atomic<uint64_t> samples{0};
    r.media().device.write = [&](const int16_t *, size_t n, unsigned rate,
                                 unsigned channels) {
      check(rate >= 8000 && rate <= 48000 && channels >= 1 && channels <= 2,
            "decoded audio format");
      samples += n;
      ++writes;
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      return true;
    };
    r.media().device.read = [&](std::vector<int16_t> &pcm) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      if (reads++ >= 5)
        return false;
      pcm.assign(512, 123);
      return true;
    };
    r.media().device.capture = [](Picture &p) {
      p.width = 64;
      p.height = 48;
      p.pixels.assign(64 * 48 * 2, '\x80');
      return true;
    };
    r.setSignal([&](bool) {
      ++signals;
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
      auto count = writes.load();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      check(writes == count, "alarm interrupts music output");
    });
    check(disk.write("/home/Music/test.wav", wav()), "WAV fixture");
    check(r.media().play("/home/Music/test.wav"), "start WAV stream");
    check(wait([&] { return writes.load() > 0; }), "WAV reached output");
    open("notes");
    action("save", "Música continua durante a nota");
    check(get(root / "data/notes/document.txt").find("Música") !=
              std::string::npos,
          "editing while audio worker runs");
    r.media().pause();
    auto paused = writes.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
    check(writes == paused, "pause stops output");
    r.media().pause();
    r.emitSignal(true);
    check(signals == 1, "alarm callback delivered");
    check(wait([&] { return r.media().status().rfind("idle", 0) == 0; }),
          "WAV finishes after resume");
    check(
        disk.write("/home/Music/test.mp3", get(fs::path(argv[3]) / "tone.mp3")),
        "MP3 fixture");
    auto old = samples.load();
    check(r.media().play("/home/Music/test.mp3"), "start MP3");
    check(wait([&] { return r.media().status().rfind("idle", 0) == 0; }),
          "MP3 completes");
    check(samples > old &&
              r.media().status().find("invalid") == std::string::npos,
          "MP3 decoded including ID3");
    old = samples;
    check(r.media().queue({"/home/Music/test.wav", "/home/Music/test.wav"}),
          "start playlist");
    check(wait([&] { return samples >= old + 32000; }),
          "playlist advances to second track");
    r.media().stop();
    check(r.media().record("/home/Recordings/test.wav"),
          "start microphone recording");
    check(wait([&] { return r.media().status().rfind("idle", 0) == 0; }),
          "recording stops on input failure");
    auto recording = get(root / "home/Recordings/test.wav");
    check(recording.size() == 44 + 5 * 1024 &&
              recording.substr(0, 4) == "RIFF" && le(recording, 40) == 5 * 1024,
          "recording header finalized with actual PCM length");
    reads = 0;
    check(r.media().record("/home/Recordings/full.wav"),
          "recording before ENOSPC");
    disk.setFreeSpaceProvider([] { return 0; });
    check(wait([&] { return r.media().status().rfind("idle", 0) == 0; }),
          "ENOSPC stops recording");
    disk.setFreeSpaceProvider([] { return 1000000000; });
    check(get(root / "home/Recordings/full.wav").substr(0, 4) == "RIFF",
          "full-card recording remains WAV");
    check(disk.write("/home/Recordings/recovered.wav.part", recording),
          "interrupted recording fixture");
    r.media().recover();
    check(fs::exists(root / "home/Recordings/recovered.wav") &&
              !fs::exists(root / "home/Recordings/recovered.wav.part"),
          "interrupted recording recovered");
    for (const auto &name : {"picture.jpg", "picture.png"}) {
      check(disk.write("/home/Pictures/" + std::string(name),
                       get(fs::path(argv[3]) / name)),
            "image fixture");
      check(r.media().image("/home/Pictures/" + std::string(name)),
            "JPEG/PNG decode");
      auto picture = r.media().picture();
      check(picture && picture->width == 64 && picture->height == 48 &&
                picture->pixels.size() == 64 * 48 * 2,
            "bounded RGB565 preview");
    }
    check(disk.write("/home/Pictures/inflate.png",
                     get(fs::path(argv[3]) / "png-inflate-limit.png")),
          "PNG inflate budget fixture");
    check(!r.media().image("/home/Pictures/inflate.png") &&
              r.media().image("/home/Pictures/picture.jpg"),
          "malformed PNG cannot exceed codec heap budget and recovery works");
    check(r.media().photograph("/home/Pictures/camera.jpg", true),
          "mock camera JPEG capture");
    check(get(root / "home/Pictures/camera.jpg").substr(0, 2) == "\xff\xd8",
          "camera saved actual JPEG");
    check(!r.media().photograph("/outside.jpg", true),
          "camera destination confined");
    Installer installer(disk);
    Package metadata;
    auto package = [&](const std::string &name) {
      auto dest = "/update/packages/" + name;
      check(disk.write(dest, get(fs::path(argv[2]) / name)),
            "copy package " + name);
      return dest;
    };
    auto hello = package("hello.t5pkg");
    check(installer.inspect(hello, metadata) && metadata.id == "hello" &&
              metadata.hashes.count("main.lua"),
          "inspect package metadata and hashes");
    auto reviewed = metadata.archiveHash;
    for (const auto &name :
         {"compressed.t5pkg", "traversal.t5pkg", "alias.t5pkg",
          "bad-hash.t5pkg", "incompatible.t5pkg", "permission.t5pkg",
          "bytecode.t5pkg", "overflow.t5pkg"}) {
      auto path = package(name);
      check(!installer.inspect(path, metadata),
            "reject hostile/incompatible package " + std::string(name));
    }
    std::atomic<bool> cancel{true};
    check(!installer.install(hello, &cancel) &&
              !fs::exists(root / "apps/registry.tsv"),
          "cancel before activation preserves registry");
    disk.setFreeSpaceProvider([] { return 0; });
    check(!installer.install(hello),
          "installation checks peak physical occupancy");
    disk.setFreeSpaceProvider([] { return 1000000000; });
    check(!installer.install(hello, nullptr, "changed-review"),
          "changed package review rejected");
    check(installer.install(hello, nullptr, reviewed), "install local app");
    check(r.boot(), "boot discovers installed app");
    open("hello");
    check(r.view().text.find("Recurso externo") != std::string::npos,
          "installed app reads verified resource");
    action("save", "Dados persistentes do SDK");
    check(get(root / "data/hello/document.txt") == "Dados persistentes do SDK",
          "installed app owns persistent data");
    open("shell");
    check(r.close("hello"), "checkpoint before upgrade");
    auto v2 = package("hello-v2.t5pkg");
    cancel = true;
    auto registry = get(root / "apps/registry.tsv");
    check(!installer.install(v2, &cancel) &&
              get(root / "apps/registry.tsv") == registry,
          "interrupted upgrade keeps old activation");
    check(installer.install(v2), "upgrade version atomically");
    check(r.boot(), "boot upgraded app");
    open("hello");
    check(r.view().text == "Dados persistentes do SDK",
          "upgrade preserves app data");
    open("shell");
    check(r.close("hello"), "close app before removal");
    check(installer.remove("hello") &&
              fs::exists(root / "data/hello/document.txt"),
          "uninstall preserves data by default");
    check(installer.install(hello) && installer.remove("hello", true) &&
              !fs::exists(root / "data/hello"),
          "explicit uninstall erases own data");
    auto guard = package("guard.t5pkg");
    check(installer.install(guard) && r.boot(),
          "permission guard app installed");
    check(!r.open("guard") && !fs::exists(root / "data/notes/protected.txt"),
          "installed app cannot write another app's data");
    check(r.open("shell") && installer.remove("guard"),
          "permission failure is recoverable");
    auto pictureApp = package("picture-app.t5pkg");
    check(installer.install(pictureApp) && r.boot() && r.open("picture_app"),
          "own image asset needs no global media permission");
    check(r.view().layout == "image" && r.view().picture &&
              r.view().picture->width == 64,
          "installed image source is verified and decoded");
    check(r.open("shell") && r.close("picture_app") &&
              installer.remove("picture_app"),
          "remove image app");
    auto theme = package("theme.t5pkg");
    check(installer.install(theme), "install external theme");
    check(r.boot(), "boot with installed theme catalog");
    open("installer");
    action("theme:graphite");
    check(r.view().background == "202329",
          "apply theme without firmware update");
    action("default_theme");
    check(r.view().background == "17171d", "restore original theme");
  }
  fs::remove_all(base);
  std::cout << checks.load() << " media/installer checks passed\n";
}
