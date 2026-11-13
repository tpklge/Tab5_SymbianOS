// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace tab5;
static unsigned checks = 0;
static void check(bool ok, const char *why) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << why << '\n';
    exit(1);
  }
}
static bool wait(const std::function<bool()> &f) {
  for (unsigned i = 0; i < 1500; ++i) {
    if (f())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}
int main(int argc, char **argv) {
  check(argc == 2, "arguments");
  auto temp =
      fs::temp_directory_path() / ("tab5-known-" + std::to_string(getpid()));
  fs::copy(argv[1], temp, fs::copy_options::recursive);
  Storage disk(temp.string());
  check(disk.mount() && disk.prepare("/config/network"),
        "private SD directory");
  {
    Network net(disk);
    net.backend.connect = [](const std::string &, const std::string &,
                             std::string &ip, const auto &) {
      ip = "192.0.2.2";
      return true;
    };
    check(net.saved().empty(), "no default credentials");
    check(net.connect("Minha rede", "senha-teste-123", true) &&
              wait([&] { return !net.status().busy; }),
          "successful connection saves profile");
    check(net.saved().size() == 1 && net.saved()[0].ssid == "Minha rede" &&
              net.saved()[0].automatic,
          "saved SSID enables automatic connection");
    check(wait([&] { return net.connect("Visitante", "temporaria", false); }) &&
              wait([&] { return !net.status().busy; }) &&
              net.saved().size() == 1,
          "connect once does not persist");
    net.backend.connect = [](auto &, auto &, auto &, const auto &) {
      return false;
    };
    check(wait([&] { return net.connect("Falha", "senha errada", true); }) &&
              wait([&] { return !net.status().busy; }) &&
              net.saved().size() == 1,
          "failed authentication not saved");
    check(!net.connect("bad\nssid", "12345678", true),
          "unsupported SSID line break rejected");
  }
  {
    Network net(disk);
    auto credentials = std::make_shared<std::string>();
    auto linked = std::make_shared<std::atomic<bool>>(false);
    net.backend.linked = [linked] { return linked->load(); };
    net.backend.connect = [credentials, linked](const std::string &ssid,
                                                const std::string &password,
                                                std::string &ip,
                                                const auto &cancel) {
      if (cancel)
        return false;
      *credentials = ssid + "|" + password;
      ip = "192.0.2.3";
      *linked = true;
      return true;
    };
    net.backend.disconnect = [linked] { *linked = false; };
    net.backend.scan = [](auto &visible, const auto &) {
      visible.push_back({"Minha rede", "WPA2/WPA3", -40, 6});
      return true;
    };
    check(net.saved().size() == 1, "profiles survive service restart");
    check(net.autoConnect(1000) && wait([&] { return !net.status().busy; }),
          "automatic reconnect after boot");
    check(*credentials == "Minha rede|senha-teste-123" &&
              net.status().ssid == "Minha rede",
          "native saved credential used without Lua password");
    *linked = false;
    check(!net.autoConnect(2000), "reconnection backoff");
    check(wait([&] { return net.autoConnect(32000); }) &&
              wait([&] { return !net.status().busy; }),
          "reconnection after loss");
    net.disconnect();
    check(!net.autoConnect(64000),
          "manual disconnect suppresses automatic reconnect");
    net.resumeAutomatic();
    check(net.setAutomatic("Minha rede", false) && !net.autoConnect(65000),
          "per-network automatic preference");
    check(net.setAutomatic("Minha rede", true),
          "automatic can be enabled again");
    check(wait([&] { return net.connectSaved("Minha rede"); }) &&
              wait([&] { return !net.status().busy; }),
          "manual connect from saved profile");
    check(net.forget("Minha rede") && net.saved().empty(),
          "forget removes credentials");
    check(!net.connectSaved("Minha rede"),
          "forgotten credential cannot connect");
  }
  {
    Network net(disk);
    check(disk.write("/config/network/known-wifi.tsv", "T5WIFI1\ninvalid\n"),
          "malformed profile fixture");
    check(net.saved().empty() && !net.autoConnect(100000),
          "malformed profiles do not start radio");
    net.backend.connect = [](auto &, auto &, std::string &ip, const auto &) {
      ip = "192.0.2.4";
      return true;
    };
    check(net.connect("Nova", "password", true) &&
              wait([&] { return !net.status().busy; }),
          "successful connection handles corrupt profile file");
    std::string raw;
    check(disk.read("/config/network/known-wifi.tsv", raw) &&
              raw == "T5WIFI1\ninvalid\n" &&
              net.status().message.find("não foi possível salvar") !=
                  std::string::npos,
          "corrupt credential file preserved with visible save failure");
  }
  check(disk.write("/config/network/known-wifi.tsv", "T5WIFI1\n"),
        "clear profile test fixture");
  {
    Runtime runtime(disk);
    check(runtime.boot(), "boot saved-network UI");
    runtime.network().backend.connect = [](auto &, auto &, std::string &ip,
                                           const auto &) {
      ip = "192.0.2.5";
      return true;
    };
    check(runtime.network().connect("Rede UI", "segredo-nao-exibir", true) &&
              wait([&] { return !runtime.network().status().busy; }),
          "UI profile saved");
    check(runtime.open("network") && runtime.dispatch("saved") &&
              runtime.view().buttons[0].label == "Rede UI",
          "saved network list");
    check(runtime.dispatch("known:1") && !runtime.view().statusFields.empty() &&
              runtime.view().statusFields[0].value == "Rede UI" &&
              runtime.view().text.find("segredo-nao-exibir") ==
                  std::string::npos,
          "profile detail never exposes password");
    check(runtime.dispatch("auto_off") &&
              !runtime.network().saved()[0].automatic,
          "automatic toggle through Lua");
    check(runtime.dispatch("forget") && runtime.network().saved().empty(),
          "forget through Lua");
    check(runtime.dispatch("back") && runtime.dispatch("recover") &&
              wait([&] { return !runtime.network().status().busy; }),
          "Wi-Fi recovery action available through Lua");
    check(runtime.open("email") && runtime.dispatch("diagnostic"),
          "email recovery diagnostic UI");
    check(
        std::any_of(runtime.view().footer.begin(), runtime.view().footer.end(),
                    [](const Button &b) { return b.id == "recover_network"; }),
        "email offers reconnection footer icon");
    check(runtime.dispatch("recover_network") &&
              wait([&] { return !runtime.network().status().busy; }) &&
              runtime.network().status().connected,
          "email schedules asynchronous network recovery");
  }
  {
    std::atomic<unsigned> connections{0}, disconnects{0}, shareStops{0};
    Network net(disk);
    net.backend.connect = [&](const std::string &ssid,
                              const std::string &password, std::string &ip,
                              const auto &) {
      check(ssid == "RAM only" && password == "ram-secret",
            "recovery uses original unsaved credentials");
      if (++connections > 1)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      ip = "192.0.2.20";
      return true;
    };
    net.backend.disconnect = [&] { ++disconnects; };
    net.backend.share = [&shareStops](bool on, std::string &ip,
                                      std::string &token) {
      if (!on)
        ++shareStops;
      ip = "192.0.2.20";
      token = on ? "ABCDEFGH" : "";
      return true;
    };
    check(net.connect("RAM only", "ram-secret", false) &&
              wait([&] { return !net.status().busy; }),
          "unsaved active connection");
    check(net.share(true) && wait([&] { return !net.status().busy; }),
          "enable OTA before recovery");
    disk.mkdir("/home/Transfer");
    check(disk.write("/home/Transfer/recovery-test.bin", "test"),
          "transfer recovery fixture");
    std::atomic<bool> entered{false}, release{false};
    std::thread download([&] {
      net.send("/home/Transfer/recovery-test.bin", [&](const char *, size_t) {
        entered = true;
        while (!release)
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return true;
      });
    });
    check(wait([&] { return entered.load(); }) && !net.recover(true),
          "automatic recovery cannot interrupt active transfer");
    release = true;
    download.join();
    check(net.recover(true) && net.status().busy,
          "recovery starts asynchronously");
    check(!net.send("/home/Transfer/recovery-test.bin",
                    [](const char *, size_t) { return true; }),
          "new transfers wait until recovery finishes");
    check(wait([&] { return !net.status().busy; }) && connections == 2 &&
              disconnects == 1 && net.status().connected &&
              net.status().sharing && net.status().token == "ABCDEFGH",
          "recovery reconnects and restores OTA/token");
    check(!net.recover(true), "automatic recovery rate limited");
    check(net.recover() && wait([&] { return !net.status().busy; }),
          "manual recovery permitted inside automatic cooldown");
    check(net.connect("RAM only", "ram-secret", true) &&
              wait([&] { return !net.status().busy; }),
          "saved candidate exists for suppression regression");
    auto beforeFailure = connections.load();
    net.backend.recover = [](const std::atomic<bool> &, std::string &error) {
      error = "driver cleanup failed";
      return false;
    };
    check(net.recover() && wait([&] { return !net.status().busy; }),
          "failed driver cleanup completes job");
    check(connections == beforeFailure &&
              net.status().message == "driver cleanup failed",
          "failed cleanup preserves error and prevents reconnect over stale "
          "state");
    check(!net.autoConnect(999999999),
          "failed recovery suppresses automatic scan loop");
    net.backend.recover = [](const std::atomic<bool> &, std::string &) {
      return true;
    };
    check(net.recover() && wait([&] { return !net.status().busy; }) &&
              net.status().connected && net.status().sharing,
          "manual retry rebuilds driver and restores originally requested OTA "
          "after failure");
    net.disconnect();
    check(!net.recover(true),
          "manual disconnect suppresses automatic recovery");
    net.backend.restartRequired=[] {return true;};
    auto blocked=net.status();
    check(blocked.restartRequired && !blocked.connected && !blocked.sharing &&
          blocked.address.empty() && blocked.token.empty(),
          "unrecoverable transport exposes disconnected restart snapshot");
    auto beforeBlocked=connections.load();
    check(!net.recover() && !net.connect("blocked", "") && !net.scan() &&
          !net.autoConnect(999999999) && connections.load()==beforeBlocked,
          "unrecoverable transport blocks manual and automatic backend jobs");
  }
  const std::string alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for (unsigned n = 0; n < 256; ++n) {
    std::array<uint8_t, 8> bytes{};
    bytes.fill(n);
    auto token = Network::transferToken(bytes);
    check(token.size() == 8 &&
              token.find_first_not_of(alphabet) == std::string::npos,
          "eight-character token alphabet");
  }
  fs::remove_all(temp);
  std::cout << checks << " saved-network/token checks passed\n";
}
