// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <filesystem>
#include <iostream>
#include <unistd.h>
using namespace tab5;
namespace fs = std::filesystem;
void check(bool ok, const std::string &message) {
  if (!ok) {
    std::cerr << "FAIL: " << message << '\n';
    exit(1);
  }
}
struct Fixture {
  fs::path root;
  Storage disk;
  Runtime runtime;
  Installer installer;
  Fixture(const std::string &source)
      : root(fs::temp_directory_path() /
             ("tab5-sis-lifecycle-" + std::to_string(getpid()))),
        disk(root.string()), runtime(disk), installer(disk) {
    fs::remove_all(root);
    fs::copy(source, root, fs::copy_options::recursive);
    check(runtime.boot(), runtime.error);
  }
  ~Fixture() {
    runtime.shutdown();
    fs::remove_all(root);
  }
  std::string package(const std::string &base, const std::string &name) {
    auto to = "/home/Transfer/" + name + ".sis";
    fs::copy_file(fs::path(base) / (name + ".sis"), root / to.substr(1),
                  fs::copy_options::overwrite_existing);
    check(disk.reconcile(), "reconcile package");
    return to;
  }
  std::string current(const std::string &id) {
    std::vector<Package> all;
    check(installer.catalog(all), installer.error);
    for (auto &p : all)
      if (p.id == id)
        return p.version;
    return {};
  }
  void open(const std::string &id) {
    check(runtime.boot(), runtime.error);
    check(runtime.open(id), "launch " + id + ": " + runtime.error);
  }
};
int main(int argc, char **argv) {
  check(argc == 3, "arguments");
  Fixture f(argv[1]);
  auto &i = f.installer;
  auto &d = f.disk;
  auto path = [&](const std::string &name) { return f.package(argv[2], name); };
  auto base = path("01-base-1.0.0"), v1 = path("02-counter-1.0.0"),
       v11 = path("03-counter-1.1.0"), v2 = path("04-counter-2.0.0"),
       bad = path("05-counter-2.1.0-broken"), down = path("06-counter-0.9.0"),
       incompatible = path("07-base-2.0.0-incompatible"),
       missing = path("08-missing-dependency");
  // Legacy catalog entries cannot bypass the pending-trial serialization.
  std::string legacySource =
      "function event() return {title='Legacy',body='fixture'} end";
  std::string legacyManifest =
      "format=1\nid=legacy_test\nversion=1.0.0\ntitle=Legacy\nauthor="
      "Tests\nkind=app\napi_min=4\napi_max=4\norientation=landscape\nmemory="
      "1048576\ninstructions=100000\npermissions=\nfile=main.lua|" +
      sha256(legacySource) + "\n";
  check(d.prepare("/apps/legacy_test/1.0.0") &&
            d.write("/apps/legacy_test/1.0.0/main.lua", legacySource) &&
            d.write("/apps/legacy_test/1.0.0/manifest.txt", legacyManifest),
        "legacy fixture");
  check(d.write("/apps/registry.tsv", "T5APPS1\nlegacy_test\t1.0.0\t" +
                                          sha256(legacyManifest) + "\n"),
        "legacy catalog fixture");
  check(!i.install(v1), "missing dependency blocked");
  check(!i.install(missing), "unknown dependency blocked");
  check(i.install(base), i.error);
  check(!i.remove("legacy_test"), "legacy removal blocked during SIS trial");
  check(!i.install(v1), "pending first launch blocks install");
  f.open("sis_base");
  check(i.install(v1), i.error);
  f.open("sis_counter");
  check(f.runtime.dispatch("increment") && f.runtime.dispatch("increment"),
        "counter writes");
  std::string data;
  check(d.read("/data/sis_counter/counter.txt", data) && data == "2",
        "initial user data");
  check(!i.remove("sis_base"), "reverse dependency blocks removal");
  check(!i.install(incompatible), "upgrade cannot break dependents");
  check(i.install(v11), i.error);
  f.open("sis_counter");
  check(f.current("sis_counter") == "1.1.0", "upgrade version");
  check(d.read("/data/sis_counter/counter.txt", data) && data == "2",
        "upgrade preserves user data");
  check(f.runtime.dispatch("export"), "new permission dispatch");
  check(d.read("/home/Documents/sis-counter.txt", data) && data == "2",
        "new documents permission granted after reviewed update");
  check(i.install(v2), i.error);
  check(d.read("/data/sis_counter/counter-v2.txt", data) && data == "2",
        "migration renames data");
  check(d.read("/data/sis_counter/note.txt", data),
        "migration creates default");
  check(d.read("/data/sis_counter/empty.txt", data) && data.empty(),
        "migration supports empty defaults");
  f.open("sis_counter");
  Package previous;
  check(i.previousSis("sis_counter", previous) && previous.version == "1.1.0",
        "previous version recorded");
  check(i.install(bad), i.error);
  check(f.runtime.boot(), f.runtime.error);
  check(!f.runtime.open("sis_counter"), "broken first launch rejected");
  check(f.current("sis_counter") == "2.0.0",
        "automatic rollback restores version");
  check(d.read("/data/sis_counter/counter-v2.txt", data) && data == "2",
        "automatic rollback restores data after malicious write");
  auto conflict = path("14-migration-conflict");
  check(!i.install(conflict), "migration conflict rejected before activation");
  check(f.current("sis_counter") == "2.0.0" &&
            d.read("/data/sis_counter/counter-v2.txt", data) && data == "2",
        "failed migration leaves original untouched");
  check(i.install(bad), i.error);
  check(i.beginSisTrial("sis_counter"), i.error);
  check(d.write("/data/sis_counter/counter-v2.txt", "999"),
        "simulate interrupted first launch");
  check(f.runtime.boot(), f.runtime.error);
  check(f.current("sis_counter") == "2.0.0" &&
            d.read("/data/sis_counter/counter-v2.txt", data) && data == "2",
        "boot reverts interrupted first launch");
  check(i.rollbackSis("sis_counter"), i.error);
  f.open("sis_counter");
  check(f.current("sis_counter") == "1.1.0" &&
            d.read("/data/sis_counter/counter.txt", data) && data == "2",
        "manual rollback restores previous schema and data");
  check(!i.install(down), "downgrade needs explicit consent");
  Package reviewed;
  check(i.inspect(down, reviewed), i.error);
  check(i.installDowngrade(down, nullptr, reviewed.archiveHash), i.error);
  f.open("sis_counter");
  check(f.current("sis_counter") == "0.9.0", "explicit downgrade succeeds");
  auto collision = path("12-uid-collision"),
       publisher = path("13-publisher-change");
  check(!i.install(collision), "UID collision rejected");
  check(!i.install(publisher), "declared publisher change rejected");
  auto a1 = path("09-cycle-a-1.0.0"), b1 = path("10-cycle-b-1.0.0"),
       a2 = path("11-cycle-a-2.0.0");
  check(i.install(a1), i.error);
  f.open("sis_cycle_a");
  check(i.install(b1), i.error);
  f.open("sis_cycle_b");
  check(!i.install(a2), "dependency cycle rejected");
  // Fresh fixture for each interruption boundary: old or new catalog, complete
  // data.
  std::vector<std::string> steps;
  check(i.install(v11), i.error);
  f.open("sis_counter");
  i.transactionCheckpoint = [&](const std::string &step) {
    steps.push_back(step);
    return true;
  };
  check(i.install(v2), i.error);
  i.transactionCheckpoint = {};
  check(i.rollbackSis("sis_counter"), i.error);
  check(f.current("sis_counter") == "1.1.0",
        "cancel trial returns old version");
  for (size_t fail = 0; fail < steps.size(); ++fail) {
    size_t n = 0;
    i.transactionCheckpoint = [&](const std::string &) { return n++ != fail; };
    check(!i.install(v2), "inject update interruption");
    i.transactionCheckpoint = {};
    check(d.mount() && i.recoverSisBoot(), i.error);
    check(i.recoverSisBoot(), "idempotent lifecycle recovery");
    auto active = f.current("sis_counter");
    check(active == "1.1.0" || active == "2.0.0",
          "complete old or new version after recovery");
    if (active == "2.0.0")
      check(i.rollbackSis("sis_counter"), i.error);
    check(d.read("/data/sis_counter/counter.txt", data) && data == "2",
          "interruption preserves old data");
  }
  // Failure after first focus must still finish the retained snapshot on
  // reboot.
  check(i.install(v2), i.error);
  check(i.beginSisTrial("sis_counter"), i.error);
  std::vector<std::string> finishing;
  i.transactionCheckpoint = [&](const std::string &step) {
    finishing.push_back(step);
    return true;
  };
  check(i.finishSisTrial("sis_counter", true), i.error);
  i.transactionCheckpoint = {};
  check(i.rollbackSis("sis_counter"), i.error);
  f.open("sis_counter");
  for (size_t failure = 0; failure < finishing.size(); ++failure) {
    check(i.install(v2) && i.beginSisTrial("sis_counter"), i.error);
    size_t n = 0;
    i.transactionCheckpoint = [&](const std::string &) {
      return n++ != failure;
    };
    check(!i.finishSisTrial("sis_counter", true),
          "inject snapshot finalization interruption");
    i.transactionCheckpoint = {};
    check(d.mount() && i.recoverSisBoot(), i.error);
    check(f.current("sis_counter") == "2.0.0",
          "finalization resumes candidate");
    check(i.previousSis("sis_counter", previous), i.error);
    auto backup = "/meta/sis/sis_counter/snapshot-" +
                  sha256(previous.manifest) + "/data/counter.txt";
    check(d.read(backup, data) && data == "2",
          "retained snapshot survives finalization interruption");
    check(i.rollbackSis("sis_counter"), i.error);
    f.open("sis_counter");
  }
  // A corrupted snapshot cannot overwrite current user data.
  check(i.install(v2), i.error);
  f.open("sis_counter");
  check(i.previousSis("sis_counter", previous), i.error);
  auto backup = "/meta/sis/sis_counter/snapshot-" + sha256(previous.manifest) +
                "/data/counter.txt";
  check(d.write(backup, "corrupt"), "corrupt snapshot fixture");
  check(!i.rollbackSis("sis_counter"), "corrupted snapshot rejected");
  check(f.current("sis_counter") == "2.0.0" &&
            d.read("/data/sis_counter/counter-v2.txt", data) && data == "2",
        "bad snapshot leaves current data intact");
  check(d.write(backup, "2") && i.rollbackSis("sis_counter"), i.error);
  f.open("sis_counter");
  check(i.remove("sis_counter", false), i.error);
  check(d.read("/data/sis_counter/counter.txt", data) && data == "2",
        "remove preserves data by default");
  auto impersonation = path("20-retained-identity-change");
  check(!i.install(impersonation), "retained data refuses another UID");
  check(i.install(v11), i.error);
  f.open("sis_counter");
  check(d.read("/data/sis_counter/counter.txt", data) && data == "2",
        "reinstall preserves retained data");
  i.transactionCheckpoint = [](const std::string &s) {
    return s != "remove_activated";
  };
  check(!i.remove("sis_counter", true), "interrupt committed removal");
  i.transactionCheckpoint = {};
  check(d.mount() && i.recoverSisBoot(), i.error);
  bool present = false;
  check(d.pathExists("/data/sis_counter", present) && !present,
        "committed removal erases only selected app data");
  check(d.read("/home/Documents/sis-counter.txt", data),
        "exported user document survives app removal");
  check(i.remove("sis_base", true), i.error);
  check(f.runtime.boot() && f.runtime.open("installer") &&
            f.runtime.dispatch("inspect:" + base),
        "installer review handles profile 2");
  std::cout << "SIS dependencies/cycles, permissions, schemas/migration, "
               "first-launch rollback, downgrade, retention/removal and "
            << steps.size() << " update interruption points passed\n";
}
