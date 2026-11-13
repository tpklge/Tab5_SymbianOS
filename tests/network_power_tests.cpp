// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;
namespace fs = std::filesystem;
static unsigned checks = 0;
static void check(bool ok, const char *why) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << why << '\n';
    exit(1);
  }
}
static bool wait(const std::function<bool()> &f) {
  for (int i = 0; i < 1000; ++i) {
    if (f())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
  }
  return false;
}
int main(int argc, char **argv) {
  check(argc == 2, "source argument");
  auto temp =
      fs::temp_directory_path() / ("tab5-network-" + std::to_string(getpid()));
  auto root = temp / "SymbianOS";
  fs::create_directories(temp);
  fs::copy(argv[1], root, fs::copy_options::recursive);
  {
    Storage disk(root.string());
    Runtime runtime(disk);
    check(runtime.boot(), "boot API6");
    check(batteryPercent(5900) == 0 && batteryPercent(8400) == 100 &&
              batteryPercent(7700) == 55,
          "2S battery estimate bounds and curve");
    PowerInfo p;
    p.available = true;
    p.percent = 55;
    p.millivolts = 7700;
    p.milliamps = -120;
    p.charging = true;
    runtime.setPowerInfo(p);
    auto modes = std::make_shared<std::string>();
    runtime.setPowerWriter([modes](const std::string &s) {
      *modes = s;
      return true;
    });
    check(runtime.open("energy") &&
              !runtime.view().statusFields.empty() && runtime.view().statusFields[0].value.find("55%") != std::string::npos,
          "power UI uses cached measurement");
    check(runtime.dispatch("fast") && *modes == "fast",
          "QC requested through power capability");
    check(runtime.open("music") && runtime.dispatch("volume_up") &&
              runtime.media().volume() == 70,
          "music volume control");
    check(runtime.open("recorder") && runtime.dispatch("volume_down") &&
              runtime.media().volume() == 60,
          "recorder shares master volume");
    runtime.media().setVolume(500);
    check(runtime.media().volume() == 100, "volume upper bound");
    runtime.media().setVolume(-2);
    check(runtime.media().volume() == 0, "volume lower bound");
    auto &net = runtime.network();
    check(Network::sharedPath("/home/Transfer/test.txt"), "shared filename");
    for (auto path : {"/config/public/profile", "/home/Transfer/../Music/a.wav",
                      "/home/Transfer/sub/a", "/home/Transfer/%2e%2e",
                      "/home/Transfer/test.part"})
      check(!Network::sharedPath(path), "outside transfer root rejected");
    for (auto url : {"file:///etc/passwd", "https://u:p@example.test/a",
                     "http://example.test/a\r\nBad: header", "https:///path",
                     "ftp://example.test/a"})
      check(!Network::validUrl(url), "unsafe URL rejected");
    check(Network::validUrl("https://example.test/feed.xml?lang=pt"),
          "HTTP URL accepted");
    net.backend.connect = [](auto &, auto &, std::string &ip,
                             const std::atomic<bool> &cancel) {
      for (int i = 0; i < 30 && !cancel; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      if (cancel)
        return false;
      ip = "192.0.2.1";
      return true;
    };
    check(net.connect("test", "password"), "connect queued");
    check(!net.connect("another", ""), "parallel network operation refused");
    net.cancel();
    check(wait([&] { return !net.status().busy; }),
          "connect cancellation returns snapshot");
    check(wait([&] { return net.connect("test", ""); }) &&
              wait([&] { return !net.status().busy; }),
          "connection after cancelled request");
    check(net.status().connected, "connected status");
    net.backend.fetch = [](const std::string &, const Network::Sink &sink,
                           const std::atomic<bool> &,
                           std::string &) { return sink("hello", 5); };
    check(wait([&] {
            return net.download("https://example.test/a",
                                "/home/Transfer/a.txt");
          }),
          "download queued");
    check(wait([&] { return !net.status().busy; }), "download finishes");
    std::string data;
    check(disk.read("/home/Transfer/a.txt", data) && data == "hello",
          "download publishes exact bytes");
    check(wait([&] {
            return net.download("https://example.test/a",
                                "/home/Transfer/a.txt");
          }) &&
              wait([&] { return !net.status().busy; }),
          "existing download handled");
    check(disk.read("/home/Transfer/a.txt", data) && data == "hello",
          "existing destination preserved");
    net.backend.fetch = [](auto &, const Network::Sink &sink, const auto &,
                           std::string &) {
      sink("part", 4);
      return false;
    };
    check(wait([&] {
            return net.download("http://example.test/fail",
                                "/home/Transfer/b.txt");
          }) &&
              wait([&] { return !net.status().busy; }),
          "failed HTTP handled");
    check(!fs::exists(root / "home/Transfer/b.txt") &&
              !fs::exists(root / "home/Transfer/b.txt.part"),
          "failed download cleans partial in root");
    check(disk.write("/data/feeds/latest.xml",
                     "<rss><title>Anterior</title></rss>"),
          "previous feed");
    net.backend.fetch = [](auto &, const Network::Sink &sink, const auto &,
                           std::string &) {
      std::string xml = "<!DOCTYPE rss><rss><title>Bad</title></rss>";
      return sink(xml.data(), xml.size());
    };
    check(wait([&] {
            return net.download("https://example.test/rss",
                                "/data/feeds/latest.xml", true);
          }) &&
              wait([&] { return !net.status().busy; }),
          "DTD feed handled");
    check(disk.read("/data/feeds/latest.xml", data) &&
              data.find("Anterior") != std::string::npos,
          "unsafe feed preserves previous snapshot");
    net.backend.fetch = [](auto &, const Network::Sink &sink, const auto &,
                           std::string &) {
      std::string xml = "<rss><channel><title>Canal</title><item><title>Olá "
                        "&amp; mundo</title></item></channel></rss>";
      return sink(xml.data(), xml.size());
    };
    check(wait([&] {
            return net.download("https://example.test/rss",
                                "/data/feeds/latest.xml", true);
          }) &&
              wait([&] { return !net.status().busy; }),
          "RSS downloaded");
    check(disk.read("/data/feeds/latest.xml", data) && disk.write("/data/feeds/cache-gizmodo.xml",data),"feed-specific cached snapshot");
    check(runtime.open("feeds") && runtime.dispatch("source:gizmodo") &&
              !runtime.view().buttons.empty() && runtime.view().buttons[0].label.find("Olá & mundo") != std::string::npos,
          "feed titles shown without markup");
    std::string upload(12000, 'x');
    size_t offset = 0;
    check(net.receive(
              "/home/Transfer/upload.bin",
              [&](char *out, size_t n) {
                n = std::min(n, upload.size() - offset);
                memcpy(out, upload.data() + offset, n);
                offset += n;
                return int(n);
              },
              upload.size()),
          "streamed upload");
    data.clear();
    check(net.send("/home/Transfer/upload.bin",
                   [&](const char *p, size_t n) {
                     data.append(p, n);
                     return true;
                   }) &&
              data == upload,
          "streamed download roundtrip");
    std::vector<std::pair<std::string, size_t>> uploadStages;
    size_t generated = 0;
    check(net.receive(
              "/home/Transfer/large-upload.bin",
              [&](char *out, size_t n) {
                check(n <= 65536, "upload read size stays bounded");
                memset(out, 'u', n);
                generated += n;
                return int(n);
              },
              1024 * 1024 + 17,
              [&](const char *stage, size_t bytes) {
                uploadStages.emplace_back(stage, bytes);
              }),
          "upload larger than one MiB streams successfully");
    check(generated == 1024 * 1024 + 17 &&
              uploadStages.front().first == "checking" &&
              uploadStages.back().first == "complete" &&
              uploadStages.back().second == generated,
          "upload diagnostics cover preflight through exact completion");
    bool progress = false;
    for (auto &stage : uploadStages)
      if (stage.first == "receiving" && stage.second >= 1024 * 1024)
        progress = true;
    check(progress,
          "upload publishes progress without retaining whole payload");
    check(!net.receive(
              "/home/Transfer/bad-reader.bin",
              [](char *, size_t n) { return int(n + 1); }, 8192),
          "reader cannot exceed supplied buffer");
    check(!fs::exists(root / "home/Transfer/bad-reader.bin.part"),
          "invalid reader leaves no published file or partial");
    check(!net.receive(
              "/home/Transfer/partial.bin", [](char *, size_t) { return 0; },
              100),
          "short upload refused");
    check(!fs::exists(root / "home/Transfer/partial.bin.part"),
          "short upload removes partial");
    check(!net.receive(
              "/home/Transfer/full.bin", [](char *, size_t) { return 0; },
              65 * 1024 * 1024),
          "oversized upload refused");
    auto reading = std::make_shared<std::atomic<bool>>(false);
    auto release = std::make_shared<std::atomic<bool>>(false);
    bool uploaded = false;
    std::thread firstUpload([&] {
      uploaded = net.receive(
          "/home/Transfer/concurrent.bin",
          [&](char *out, size_t n) {
            *reading = true;
            while (!*release)
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            memset(out, 'q', n);
            return int(n);
          },
          100);
    });
    check(wait([&] { return reading->load(); }),
          "first upload occupies writer");
    check(
        !net.receive(
            "/home/Transfer/second.bin", [](char *, size_t) { return 0; }, 100),
        "parallel upload refused");
    *release = true;
    firstUpload.join();
    check(uploaded, "first concurrent upload succeeds");
    check(disk.reconcile(), "reconcile quota fixture");
    {
      Storage limited(root.string(), disk.used() + 256, 0);
      check(limited.mount(), "mount near-quota installation");
      Network constrained(limited);
      check(!constrained.receive(
                "/home/Transfer/overquota.bin",
                [](char *, size_t) { return 0; }, 512),
            "upload cannot exceed shared quota");
      check(!fs::exists(root / "home/Transfer/overquota.bin.part"),
            "quota rejection creates no partial");
    }
    net.backend.scan = [](std::vector<Network::AccessPoint> &list,
                          const std::atomic<bool> &cancel) {
      if (cancel)
        return false;
      list = {{"Minha rede", "WPA2/WPA3", -75, 1},
              {"Minha rede", "WPA2/WPA3", -35, 6},
              {"Convidados", "Aberta", -55, 11},
              {"bad\nssid", "Aberta", -10, 1}};
      return true;
    };
    check(wait([&] { return net.scan(); }) &&
              wait([&] { return !net.status().busy; }),
          "asynchronous SSID scan");
    auto scanned = net.status();
    check(scanned.networks.size() == 2 &&
              scanned.networks[0].ssid == "Minha rede" &&
              scanned.networks[0].signal == -35 &&
              scanned.networks[0].channel == 6,
          "SSID deduplication keeps strongest and filters line injection");
    check(runtime.open("network") && runtime.dispatch("scan"),
          "SSID browser opens");
    check(wait([&] { return !net.status().busy; }) &&
              runtime.dispatch("refresh"),
          "SSID browser reads completed snapshot");
    check(runtime.view().buttons[0].label == "Minha rede" &&
              runtime.view().buttons[0].subtitle.find("WPA2/WPA3") !=
                  std::string::npos,
          "scan shows security and signal");
    check(runtime.dispatch("ssid:1") && runtime.view().text == "Minha rede\n" &&
              runtime.view().controls[1].type == "password",
          "select SSID fills masked connection form");
    auto scanning = std::make_shared<std::atomic<bool>>(false);
    net.backend.scan = [scanning](std::vector<Network::AccessPoint> &,
                                  const std::atomic<bool> &cancel) {
      *scanning = true;
      while (!cancel)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return false;
    };
    check(wait([&] { return net.scan(); }) &&
              wait([&] { return scanning->load(); }),
          "background scan in progress");
    auto navStart = std::chrono::steady_clock::now();
    check(runtime.open("notes") && std::chrono::steady_clock::now() - navStart <
                                       std::chrono::milliseconds(150),
          "navigation remains responsive during scan");
    net.cancel();
    check(wait([&] { return !net.status().busy; }) &&
              net.status().message == "Cancelado",
          "scan cancellation completes without replacing previous networks");
    check(net.status().networks.size() == 2,
          "cancelled scan preserves last valid snapshot");
    net.backend.diagnostic = [] {
      return "Falha de autenticação • motivo=202";
    };
    net.backend.connect = [](auto &, auto &, std::string &,
                             const std::atomic<bool> &) { return false; };
    check(wait([&] { return net.connect("test", "password"); }) &&
              wait([&] { return !net.status().busy; }) &&
              net.status().message.find("motivo=202") != std::string::npos,
          "connection preserves backend error instead of guessing C6 or "
          "credentials");
    check(runtime.open("network") && runtime.dispatch("wifi") &&
              runtime.view().controls.size() == 2 &&
              runtime.view().controls[1].type == "password",
          "Wi-Fi password field masked");
    check(disk.write("/config/public/profile", "offline"), "offline profile");
    check(runtime.dispatch("connect", "test\npassword") &&
              runtime.view().body.find("Não foi possível") != std::string::npos,
          "offline profile refuses connection");
  }
  fs::remove_all(temp);
  std::cout << checks << " network/power checks passed\n";
}
