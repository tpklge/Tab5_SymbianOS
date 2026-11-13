// SPDX-License-Identifier: MIT
#include "http_fetch.hpp"
#include "http_body.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>
using namespace tab5;
static int checks = 0;
static void check(bool ok, const char *why) {
  ++checks;
  if (!ok) {
    std::cerr << "FAIL: " << why << '\n';
    exit(1);
  }
}
struct Fake final : HttpStream {
  struct Response {
    int status;
    std::string location, body;
  };
  std::map<std::string, Response> routes;
  std::vector<std::string> opened;
  std::string body;
  size_t offset = 0;
  unsigned waits=0;
  bool active = false;
  std::atomic<bool> *cancelOnOpen = nullptr;
  bool open(const std::string &url, int &status, std::string &location,
            std::string &error) override {
    check(!active, "previous HTTP handle closed before next hop");
    opened.push_back(url);
    auto i = routes.find(url);
    if (i == routes.end()) {
      error = "connection failure";
      return false;
    }
    active = true;
    status = i->second.status;
    location = i->second.location;
    body = i->second.body;
    offset = 0;
    if (cancelOnOpen)
      *cancelOnOpen = true;
    return true;
  }
  int read(char *data, size_t n) override {
    if(waits) {--waits;return HttpBody::again;}
    n = std::min(n, body.size() - offset);
    body.copy(data, n, offset);
    offset += n;
    return int(n);
  }
  void close() override { active = false; }
};
int main(int argc, char **argv) {
  std::string error, next, result;
  std::atomic<bool> cancel{false};
  check(resolveHttpRedirect("https://a.test/path/start?old=1",
                            "../feed?x=1#part", next, error) &&
            next == "https://a.test/feed?x=1",
        "relative Location path/query/fragment");
  check(resolveHttpRedirect("https://a.test/path/start", "//b.test/rss", next,
                            error) &&
            next == "https://b.test/rss",
        "protocol-relative Location");
  check(resolveHttpRedirect("http://a.test/path/start", "https://b.test/rss",
                            next, error) &&
            next == "https://b.test/rss",
        "HTTP upgrade");
  for (auto location :
       {"http://a.test/rss", "file:///secret", "https://user:pass@a.test/rss",
        "/rss\r\nInjected:1", ""})
    check(!resolveHttpRedirect("https://a.test/a", location, next, error),
          "unsafe/downgrade Location rejected");
  for (int code : {301, 302, 303, 307, 308}) {
    Fake f;
    f.routes = {{"https://a.test/", {code, "/feed", "never-save-this-body"}},
                {"https://a.test/feed", {200, "", "<rss>ok</rss>"}}};
    result.clear();
    check(fetchHttp(
              f, "https://a.test",
              [&](const char *p, size_t n) {
                result.append(p, n);
                return true;
              },
              cancel, error) &&
              result == "<rss>ok</rss>" && !f.active,
          "redirect GET streams only final response");
  }
  Fake delayed;
  delayed.routes={{"https://a.test/",{200,"","<rss>complete</rss>"}}};delayed.waits=3;result.clear();
  check(fetchHttp(delayed,"https://a.test/",[&](const char *p,size_t n){result.append(p,n);return true;},cancel,error) && result=="<rss>complete</rss>","temporary reads retry without truncating or duplicating body");
  Fake gizmodo;
  gizmodo.routes = {
      {"https://gizmodo.com.br/feed/",
       {301, "https://www.gizmodo.com.br/feed/", ""}},
      {"https://www.gizmodo.com.br/feed/",
       {301, "https://www.gizmodo.com.br/feed", ""}},
      {"https://www.gizmodo.com.br/feed",
       {200, "", "<rss><channel><title>Teste</title></channel></rss>"}}};
  result.clear();
  check(fetchHttp(
            gizmodo, "https://gizmodo.com.br/feed/",
            [&](const char *p, size_t n) {
              result.append(p, n);
              return true;
            },
            cancel, error) &&
            gizmodo.opened.size() == 3,
        "Gizmodo www and trailing-slash chain");
  Fake loop;
  loop.routes = {{"https://a.test/", {302, "https://A.TEST/#fragment", ""}}};
  check(!fetchHttp(
            loop, "https://a.test", [](auto, size_t) { return true; }, cancel,
            error) &&
            error.find("Ciclo") != std::string::npos,
        "canonical redirect cycle detected");
  Fake many;
  for (int i = 0; i < 7; ++i)
    many.routes["https://a.test/" + std::to_string(i)] = {
        302, "/" + std::to_string(i + 1), ""};
  check(!fetchHttp(
            many, "https://a.test/0", [](auto, size_t) { return true; }, cancel,
            error) &&
            many.opened.size() == 6 &&
            error.find("5 redirecionamentos") != std::string::npos,
        "redirect count bounded");
  Fake interrupted;
  interrupted.routes = {{"https://a.test/", {302, "/feed", ""}}};
  interrupted.cancelOnOpen = &cancel;
  check(!fetchHttp(
            interrupted, "https://a.test", [](auto, size_t) { return true; },
            cancel, error) &&
            !interrupted.active && interrupted.opened.size() == 1,
        "cancel between redirect hops");
  cancel = false;
  Fake missing;
  missing.routes = {{"https://a.test/", {301, "", ""}}};
  check(!fetchHttp(
            missing, "https://a.test", [](auto, size_t) { return true; },
            cancel, error),
        "missing Location fails");
  check(argc == 2, "SD argument");
  namespace fs = std::filesystem;
  auto root =
      fs::temp_directory_path() / ("tab5-http-" + std::to_string(getpid()));
  fs::copy(argv[1], root, fs::copy_options::recursive);
  {
    Storage disk(root.string());
    check(disk.mount() && disk.prepare("/data/feeds") &&
              disk.prepare("/home/Transfer"),
          "mount feed fixture");
    Network net(disk);
    net.backend.fetch = [&](auto &url, const auto &sink, const auto &stop,
                            auto &why) {
      return fetchHttp(gizmodo, url, sink, stop, why);
    };
    check(net.download("https://gizmodo.com.br/feed/", "/data/feeds/latest.xml",
                       true),
          "redirected feed accepted");
    for (int i = 0; i < 1500 && net.status().busy; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::string xml;
    check(!net.status().busy && disk.read("/data/feeds/latest.xml", xml) &&
              xml.find("<title>Teste") != std::string::npos,
          "redirected RSS snapshot persisted");
    bool read = false;
    check(!net.receive(
              "/home/Transfer/100mb.bin",
              [&](char *, size_t) {
                read = true;
                return 0;
              },
              100 * 1000 * 1000) &&
              !read && !fs::exists(root / "home/Transfer/100mb.bin.part"),
          "100 MB upload refused before reading or creating file");
    bool exact = false;
    check(!net.receive(
              "/home/Transfer/limit.bin",
              [&](char *, size_t) {
                exact = true;
                return 0;
              },
              Network::maxUploadBytes) &&
              exact,
          "exactly 64 MiB reaches streaming without allocating entire payload");
  }
  fs::remove_all(root);
  std::cout << checks << " HTTP redirect/upload checks passed\n";
}
