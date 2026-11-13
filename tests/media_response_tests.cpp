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
static int checks = 0;
static void check(bool ok, const std::string &why) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << why << "\n";
    exit(1);
  }
}
static bool wait(const std::function<bool()> &test, unsigned ms = 4000) {
  auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (!test()) {
    if (std::chrono::steady_clock::now() > until)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
  }
  return true;
}
static std::string get(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
static unsigned big(const std::string &s, size_t at) {
  unsigned n = 0;
  for (int i = 0; i < 4; ++i)
    n = (n << 8) | uint8_t(s[at + i]);
  return n;
}
static std::string wave() {
  std::string s(44 + 64000, '\0');
  auto put = [&](int at, unsigned n, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      s[at + i] = n & 255;
      n >>= 8;
    }
  };
  s.replace(0, 4, "RIFF");
  put(4, s.size() - 8, 4);
  s.replace(8, 8, "WAVEfmt ");
  put(16, 16, 4);
  put(20, 1, 2);
  put(22, 1, 2);
  put(24, 16000, 4);
  put(28, 32000, 4);
  put(32, 2, 2);
  put(34, 16, 2);
  s.replace(36, 4, "data");
  put(40, 64000, 4);
  return s;
}
int main(int argc, char **argv) {
  check(argc == 4, "test arguments");
  auto base = fs::temp_directory_path() /
              ("tab5-responsive-" + std::to_string(getpid()));
  auto root = base / "SymbianOS";
  fs::create_directories(base);
  fs::copy(argv[1], root, fs::copy_options::recursive);
  {
    Storage disk(root.string());
    Runtime runtime(disk);
    auto idleCloses = std::make_shared<std::atomic<unsigned>>(0);
    runtime.media().device.closeCamera = [idleCloses] { ++*idleCloses; };
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    check(idleCloses->load() == 0,
          "idle camera does not access device callbacks during startup");
    check(runtime.boot(), "boot revised runtime");
    check(disk.write("/home/Music/a.wav", wave()) &&
              disk.write("/home/Music/b.wav", wave()),
          "audio tracks");
    auto entered = std::make_shared<std::atomic<bool>>(false),
         release = std::make_shared<std::atomic<bool>>(false),
         finished = std::make_shared<std::atomic<bool>>(false);
    runtime.media().device.write = [entered, release,
                                    finished](const int16_t *, size_t, unsigned,
                                              unsigned) {
      *entered = true;
      auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
      while (!*release && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      *finished = true;
      return true;
    };
    auto lastTag = std::make_shared<std::atomic<uint64_t>>(~uint64_t(0));
    auto deviceWrite = runtime.media().device.write;
    runtime.media().device.writeTagged =
        [lastTag, deviceWrite](const int16_t *p, size_t n, unsigned r,
                               unsigned c, uint64_t tag) {
          *lastTag = tag;
          return deviceWrite(p, n, r, c);
        };
    check(runtime.media().queue({"/home/Music/a.wav", "/home/Music/b.wav"}),
          "play queued audio");
    check(wait([&] { return entered->load(); }), "device I/O in progress");
    auto start = std::chrono::steady_clock::now();
    check(runtime.open("music") && runtime.dispatch("home"),
          "home navigation during blocked device write");
    check(std::chrono::steady_clock::now() - start <
              std::chrono::milliseconds(150),
          "device I/O never holds UI state mutex");
    start = std::chrono::steady_clock::now();
    runtime.media().skip(1);
    check(runtime.media().status().find("b.wav") != std::string::npos,
          "next track works while old I/O is in progress");
    runtime.media().stop();
    check(!runtime.media().playbackGeneration(lastTag->load()),
          "stop invalidates queued PCM generation");
    check(std::chrono::steady_clock::now() - start <
              std::chrono::milliseconds(150),
          "stop and next return promptly");
    *release = true;
    check(wait([&] { return finished->load(); }), "old device block returned");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(runtime.media().status().rfind("idle", 0) == 0,
          "stale audio completion cannot restart playback");
    check(runtime.open("converter") &&
              runtime.dispatch("convert", "1\nkm\nm") &&
              runtime.view().result == "1000 m",
          "converter has a dedicated result field");
    check(runtime.open("shell") && runtime.dispatch("menu") &&
              runtime.view().buttons.size() == 12 && runtime.view().columns == 4 &&
              runtime.view().tools.size() >= 2,
          "twelve menu apps and separate paging control");
    check(runtime.open("notes") && runtime.dispatch("save", "Nota original"),
          "save legacy note");
    check(runtime.dispatch("new", "Nota original") &&
              runtime.dispatch("save", "Trabalho") &&
              runtime.dispatch("save", "Conteúdo da primeira nota"),
          "create and save another note");
    check(runtime.dispatch("new", "Conteúdo da primeira nota") &&
              runtime.dispatch("save", "Compras") &&
              runtime.dispatch("save", "Leite e pão"),
          "create a third note");
    check(runtime.dispatch("list", "Leite e pão") &&
              runtime.view().buttons.size() == 3,
          "notes list contains separate documents");
    auto rows = runtime.view().buttons;
    check(runtime.dispatch(rows[1].id) &&
              runtime.view().text == "Conteúdo da primeira nota",
          "open saved note without losing another");
    check(get(root / "data/notes/document.txt") == "Nota original",
          "legacy note preserved");
    check(runtime.view().tools.size() >= 2 && !runtime.view().footer.empty() &&
              runtime.view().footer[0].icon == "ui_save",
          "note editor has icon actions");
    check(runtime.open("music") && runtime.view().layout == "player" &&
              runtime.view().tools.size() >= 4,
          "player uses compact transport controls");
  }
  {
    Storage disk(root.string());
    Runtime runtime(disk);
    check(runtime.boot(), "recording test boot");
    std::atomic<int> blocks{0};
    runtime.media().device.read = [&](std::vector<int16_t> &pcm) {
      std::this_thread::sleep_for(std::chrono::milliseconds(32));
      pcm.assign(512, 4096);
      ++blocks;
      return true;
    };
    runtime.media().device.write = [](const int16_t *pcm, size_t n,
                                      unsigned rate, unsigned channels) {
      return n > 0 && pcm[0] > 3000 && rate == 8000 && channels == 1;
    };
    check(runtime.media().record("/home/Recordings/voice.au"),
          "start compact AU voice recording");
    check(wait([&] { return runtime.media().info().seconds >= 1.0; }),
          "recording timer advances with captured audio");
    check(runtime.media().info().level > 0,
          "recording level follows PCM samples");
    check(!runtime.media().prepareSleep(),"sleep refuses unfinished audio recording");
    runtime.media().pause();
    check(!runtime.media().prepareSleep(),"sleep refuses paused recording");
    auto start = std::chrono::steady_clock::now();
    runtime.media().stop();
    check(std::chrono::steady_clock::now() - start <
              std::chrono::milliseconds(100),
          "record stop does not wait for microphone read");
    check(wait([&] { return runtime.media().status().rfind("idle", 0) == 0; }),
          "recording finalized asynchronously");
    check(runtime.media().prepareSleep(),"sleep waits for audio/video workers to park");
    runtime.storage().pause();check(runtime.storage().mount(),"SD remount after parked workers");runtime.media().resumeSleep();
    auto au = get(root / "home/Recordings/voice.au");
    check(au.size() > 8000 && au.substr(0, 4) == ".snd" && big(au, 12) == 1 &&
              big(au, 16) == 8000 && big(au, 20) == 1,
          "standard AU G.711 format");
    check(big(au, 8) == au.size() - 24, "AU header has actual data length");
    check(au.size() < size_t(blocks.load()) * 512 * 2 / 3,
          "compressed recording is substantially smaller than PCM16");
    check(runtime.media().play("/home/Recordings/voice.au") && wait([&] {
            return runtime.media().status().rfind("idle", 0) == 0;
          }),
          "compact voice recording decodes and plays");
    check(runtime.media().status().find("Falha") == std::string::npos,
          "decoded voice has nonzero samples");
    fs::create_directories(argv[3]);
    fs::copy_file(root / "home/Recordings/voice.au",
                  fs::path(argv[3]) /
                      ("voice-" + std::to_string(getpid()) + ".au"),
                  fs::copy_options::overwrite_existing);
  }
  {
    Storage disk(root.string());
    Runtime runtime(disk);
    check(runtime.boot(), "camera test boot");
    std::atomic<int> captured{0};
    runtime.media().device.capture = [&](Picture &pic) {
      pic.width = 640;
      pic.height = 360;
      pic.pixels.assign(640 * 360 * 2, '\x60');
      ++captured;
      return true;
    };
    check(runtime.media().camera(true) && wait([&] {
            return captured.load() > 0 && bool(runtime.media().picture());
          }),
          "continuous camera preview publishes frames");
    int beforePreview = captured.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    check(captured.load() - beforePreview >= 8,
          "live preview exceeds the former 5 fps software cap");
    int beforeRecording = captured.load();
    check(runtime.media().video("/home/Pictures/test.avi"),
          "start actual MJPEG AVI recording");
    check(wait([&] {
            return runtime.media().cameraStatus().find("1 s") !=
                   std::string::npos;
          }),
          "video frames and duration advance");
    check(!runtime.media().prepareSleep(),"sleep refuses unfinished video recording");
    runtime.media().stopVideo();
    runtime.media().camera(false);
    check(wait([&] { return fs::exists(root / "home/Pictures/test.avi"); }),
          "video finalized to AVI");
    auto avi = get(root / "home/Pictures/test.avi");
    check(avi.size() > 224 && avi.substr(0, 4) == "RIFF" &&
              avi.substr(8, 4) == "AVI " &&
              avi.find("00dc") != std::string::npos,
          "AVI contains real MJPEG frames");
    size_t videoFramesAt = avi.find("avih") + 8 + 16;
    unsigned videoFrames = 0;
    for (int i = 3; i >= 0; --i)
      videoFrames = (videoFrames << 8) | uint8_t(avi[videoFramesAt + i]);
    check(videoFrames >= 3 && videoFrames <= 8,
          "MJPEG recording remains bounded to 5 fps cadence");
    check(unsigned(captured.load() - beforeRecording) > videoFrames * 2,
          "preview continues faster than recording while filming");
    check(bool(runtime.media().thumbnail("/home/Pictures/test.avi")),
          "gallery can decode a video thumbnail");
    check(runtime.open("gallery"), "gallery opens");
    bool thumbnail = false;
    for (auto &button : runtime.view().buttons)
      if (button.id == "view:/home/Pictures/test.avi")
        thumbnail = bool(button.thumbnail);
    check(runtime.view().layout == "gallery" && thumbnail,
          "gallery uses image grid with actual thumbnails");
    check(runtime.media().video("/home/Pictures/test.avi", true) && wait([&] {
            return runtime.media().cameraStatus() == "Câmera pronta";
          }),
          "recorded AVI plays back through the viewer");
    fs::copy_file(root / "home/Pictures/test.avi",
                  fs::path(argv[3]) /
                      ("video-" + std::to_string(getpid()) + ".avi"),
                  fs::copy_options::overwrite_existing);
  }
  fs::remove_all(base);
  std::cout << checks << " responsiveness/media/UI-model checks passed\n";
}
