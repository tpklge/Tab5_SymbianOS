// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace tab5;
static int checks = 0;
static void check(bool value, const std::string &message) {
  ++checks;
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
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
static std::string id(const std::string &data) {
  auto begin = data.find('\n') + 1;
  return data.substr(begin, data.find('\t', begin) - begin);
}
int main(int argc, char **argv) {
  check(argc == 2, "fixture path");
  auto base = fs::temp_directory_path() /
              ("tab5-organizer-" + std::to_string(getpid()));
  auto root = base / "SymbianOS";
  fs::create_directories(base);
  fs::copy(argv[1], root, fs::copy_options::recursive);
  Storage disk(root.string());
  Runtime r(disk);
  int sounds = 0, alarmSounds = 0, radioCalls = 0;
  bool offline = false;
  r.setSignal([&](bool alarm) {
    ++sounds;
    if (alarm)
      ++alarmSounds;
  });
  r.setOfflineWriter([&](bool off) {
    offline = off;
    ++radioCalls;
    return true;
  });
  r.setClock(1800000000);
  auto open = [&](const std::string &app) {
    r.beginOperation();
    check(r.open(app), "open " + app + ": " + r.error);
  };
  auto action = [&](const std::string &event, const std::string &value = "") {
    r.beginOperation();
    check(r.dispatch(event, value), "action " + event + ": " + r.error);
  };
  auto tick = [&] {
    r.beginOperation();
    check(r.tick(), "service tick: " + r.error);
  };
  check(r.boot(), "boot extended release: " + r.error);
  check(r.view().background == "26292e" && r.view().foreground == "f2f3f5",
        "dark theme external");
  check(radioCalls == 1 && !offline, "boot applies radio profile");
  open("notes");
  action("suspend", "rascunho com ação");
  open("calculator");
  action("calculate", "(2+3)*4");
  check(r.view().body == "20", "calculator real result");
  open("contacts");
  action("new");
  action("save", "Ana Café\n+55 65 90000\nana@example.org");
  auto contactId = id(get(root / "data/organizer/contacts.tsv"));
  check(!contactId.empty(), "contact ID persistent sequence");
  action("edit:" + contactId);
  check(r.view().text.find("Ana Café") != std::string::npos, "contact read");
  action("save", "Ana Silva\n+55 65 91111\nana@example.org");
  action("search", "Silva");
  check(r.view().buttons[0].label == "Ana Silva", "contact search");
  action("export");
  check(get(root / "home/Documents/contacts.vcf").find("FN:Ana Silva") !=
            std::string::npos,
        "vCard export");
  action("new");
  action("save", "Contato temporário\n123\nt@x.org");
  action("import");
  check(get(root / "data/organizer/contacts.tsv").find("Ana Silva") !=
            std::string::npos,
        "vCard import");
  open("notes");
  check(r.view().text == "rascunho com ação",
        "LRU eviction restores note draft");
  open("converter");
  action("convert", "1\nkm\nm");
  check(r.view().body.find("1000 m") != std::string::npos, "unit conversion");
  action("convert", "32\nf\nc");
  check(r.view().body.find("= 0 c") != std::string::npos,
        "temperature conversion");
  action("convert", "1\nkg\nm");
  check(r.view().body.find("incompat") != std::string::npos,
        "incompatible dimensions rejected");
  open("shell");
  check(r.close("converter"), "explicit close saves state");
  open("converter");
  check(r.view().text == "1\nkg\nm", "closed app resumes session");
  open("agenda");
  action("new");
  action("save", "Reunião\n2027-01-15 10:30\nSala 1\ncompromisso");
  auto eventId = id(get(root / "data/organizer/events.tsv"));
  action("edit:" + eventId);
  check(r.view().text.find("Reunião") != std::string::npos, "agenda CRUD read");
  action("done");
  check(get(root / "data/organizer/events.tsv").find("\t1\n") !=
            std::string::npos,
        "task/event completion");
  action("month");
  check(r.view().readonly && r.view().text.find("D   S") != std::string::npos,
        "monthly calendar view");
  action("day");
  action("export");
  check(get(root / "home/Documents/agenda.tsv").rfind("T5DATA1\n", 0) == 0,
        "agenda export versioned");
  open("clock");
  action("new");
  action("save", "Despertador\n2027-01-15 09:00\núnico");
  check(get(root / "data/organizer/alarms.tsv").find("Despertador") !=
            std::string::npos,
        "alarm CRUD");
  action("zone_utc");
  check(get(root / "config/public/timezone") == "UTC0",
        "timezone stored only on SD");
  action("adjust");
  action("set_time", "2027-01-15 08:00");
  check(r.view().body.find("sessão") != std::string::npos,
        "host clock adjustment does not claim RTC persistence");
  open("profiles");
  action("silent");
  check(get(root / "config/public/profile") == "silent",
        "profile persisted SD");
  auto now = r.now();
  check(disk.write("/data/organizer/events.tsv",
                   "T5DATA1\nmeeting\tAgenda fora de foco\t" +
                       std::to_string(now - 1) + "\ttexto\tcompromisso\t0\n"),
        "schedule agenda fixture");
  check(disk.write("/data/organizer/alarms.tsv",
                   "T5DATA1\nwake\tAlarme fora de foco\t" +
                       std::to_string(now - 1) + "\t0\t1\n"),
        "schedule alarm fixture");
  open("notes");
  tick();
  check(r.current() == "notes" && r.view().text == "rascunho com ação",
        "background service does not redraw foreground");
  check(r.notices().size() == 2 && sounds == 1 && alarmSounds == 1,
        "silent suppresses ordinary sound but alarm fires");
  tick();
  check(sounds == 1 && r.notices().size() == 2,
        "no duplicate on repeated ticks");
  r.setClock(now - 3600);
  tick();
  r.setClock(now + 1);
  tick();
  check(sounds == 1, "clock adjustment cannot duplicate delivered alarms");
  open("shell");
  action("notices");
  action("snooze:alarm:wake");
  tick();
  r.setClock(now + 302);
  tick();
  check(alarmSounds == 2, "five-minute snooze fires independently");
  action("ack:alarm:wake");
  tick();
  bool remains = false;
  for (auto &n : r.notices())
    if (n.id == "alarm:wake")
      remains = true;
  check(!remains, "dismissal updates native notifications");
  r.shutdown();
  check(r.boot(), "reboot after delivery");
  tick();
  check(alarmSounds == 2, "dedup ledger survives reboot");
  open("profiles");
  action("offline");
  check(offline && get(root / "config/public/profile") == "offline",
        "offline profile invokes real platform control");
  open("files");
  check(r.view().body.find("4000 MB") != std::string::npos, "quota indicator");
  action("mkdir", "/home/Documents/sub");
  action("copy", "/home/Documents/contacts.vcf\n/home/Documents/sub/copy.vcf");
  check(get(root / "home/Documents/sub/copy.vcf") ==
            get(root / "home/Documents/contacts.vcf"),
        "file copy");
  action("move",
         "/home/Documents/sub/copy.vcf\n/home/Documents/sub/renamed.vcf");
  check(fs::exists(root / "home/Documents/sub/renamed.vcf") &&
            !fs::exists(root / "home/Documents/sub/copy.vcf"),
        "file move/rename");
  action("delete", "/home/Documents/sub/renamed.vcf");
  check(fs::exists(root / "home/Documents/sub/renamed.vcf"),
        "deletion requires confirmation");
  action("confirm_delete");
  check(!fs::exists(root / "home/Documents/sub/renamed.vcf"),
        "confirmed deletion");
  auto outside = base / "outside";
  put(outside, "untouched");
  check(!disk.copy("/home/Documents/contacts.vcf", "/../outside"),
        "file copy confines destination");
  check(get(outside) == "untouched", "external file preserved");
  disk.setFreeSpaceProvider([] { return 0; });
  check(!disk.write("/home/Documents/full", "x") &&
            !disk.copy("/home/Documents/contacts.vcf", "/home/Documents/full"),
        "injected physical ENOSPC before modification");
  disk.setFreeSpaceProvider({});
  std::atomic<bool> cancel{true};
  check(!disk.copy("/home/Documents/contacts.vcf", "/home/Documents/cancelled",
                   &cancel) &&
            !fs::exists(root / "home/Documents/cancelled"),
        "cancelled copy releases reservation");
  check(!disk.backup("/backups/cancelled.t5bak", &cancel) &&
            !fs::exists(root / "backups/cancelled.t5bak"),
        "cancelled backup releases temporary space");
  open("backup");
  action("create", "/backups/full.t5bak");
  check(fs::exists(root / "backups/full.t5bak"), "real backup produced");
  auto contacts = get(root / "data/organizer/contacts.tsv");
  check(disk.write("/data/organizer/contacts.tsv",
                   "T5DATA1\nchanged\tOther\t1\tx@y.org\n"),
        "mutate live contacts after snapshot");
  action("restore", "/backups/full.t5bak");
  check(get(root / "data/organizer/contacts.tsv") != contacts,
        "restore waits for confirmation");
  action("confirm_restore", "/backups/full.t5bak");
  check(get(root / "data/organizer/contacts.tsv") == contacts,
        "restore actually recovers contact records");
  check(!fs::exists(root / "meta/restore.plan"),
        "completed restore cleans journal");
  auto data = get(root / "backups/full.t5bak");
  data[data.size() / 2] ^= 1;
  put(root / "backups/corrupt.t5bak", data);
  check(!disk.restore("/backups/corrupt.t5bak") &&
            get(root / "data/organizer/contacts.tsv") == contacts,
        "corrupt backup rejected before live changes");
  // Simulate a power interruption after the journal was durably committed.
  put(root / "home/Documents/recover.txt", "old");
  put(root / "home/Documents/recover.txt.t5-new", "new");
  put(root / "meta/restore.plan",
      "T5RESTORE1\n/home/Documents/recover.txt|3|" + sha256("new") + "\n");
  check(disk.mount() && get(root / "home/Documents/recover.txt") == "new",
        "restore journal rolls forward after restart");
  // Host-side edit must be reflected by the next audit and includes backups.
  uint64_t before = disk.used();
  put(root / "cache/from-pc", std::string(500, 'p'));
  check(disk.mount() && disk.used() == before + 500,
        "external changes reconciled");

  open("clock");
  action("adjust");
  auto priorClock = r.now();
  action("set_time", "2027-02-30 12:00");
  check(r.view().body.find("inválida") != std::string::npos &&
            r.now() >= priorClock && r.now() < priorClock + 2,
        "invalid civil date rejected without clock mutation");
  action("set_time", "2027-01-15 12:00 junk");
  check(r.view().body.find("inválida") != std::string::npos,
        "clock rejects trailing garbage");
  open("contacts");
  action("edit:" + contactId);
  action("delete");
  check(get(root / "data/organizer/contacts.tsv").find("Ana Silva") !=
            std::string::npos,
        "contact delete requires confirmation");
  action("confirm_delete");
  check(
      get(root / "data/organizer/contacts.tsv").find("\n" + contactId + "\t") ==
          std::string::npos,
      "contact CRUD deletion");
  auto snapshot =
      [&](const std::vector<std::pair<std::string, std::string>> &records) {
        std::string bytes = "T5BAK001";
        auto append = [&](uint64_t n, unsigned count) {
          for (unsigned i = 0; i < count; ++i) {
            bytes += char(n & 255);
            n >>= 8;
          }
        };
        append(records.size(), 8);
        for (auto &item : records) {
          append(item.first.size(), 2);
          append(item.second.size(), 8);
          bytes += item.first;
          bytes += item.second;
        }
        return bytes + sha256(bytes);
      };
  put(root / "backups/escape.t5bak", snapshot({{"/../outside", "bad"}}));
  check(!disk.restore("/backups/escape.t5bak") && get(outside) == "untouched",
        "authentic digest cannot authorize backup traversal");
  put(root / "backups/aliases.t5bak",
      snapshot(
          {{"/home/Documents/alias", "a"}, {"/home/Documents/ALIAS", "b"}}));
  check(!disk.restore("/backups/aliases.t5bak") &&
            !fs::exists(root / "home/Documents/alias"),
        "backup aliases rejected before staging");
  auto beforeCancel = get(root / "data/organizer/contacts.tsv");
  check(!disk.restore("/backups/full.t5bak", &cancel) &&
            get(root / "data/organizer/contacts.tsv") == beforeCancel,
        "restore cancellation preserves live documents");
  put(root / "home/Documents/first.txt", "old-first");
  put(root / "home/Documents/second.txt", "old-second");
  put(root / "home/Documents/first.txt.t5-new", "new-first");
  put(root / "home/Documents/second.txt.t5-new", "bad-second");
  put(root / "meta/restore.plan",
      "T5RESTORE1\n/home/Documents/first.txt|9|" + sha256("new-first") +
          "\n/home/Documents/second.txt|10|" + sha256("new-second") + "\n");
  check(!disk.mount() && get(root / "home/Documents/first.txt") == "old-first",
        "journal validates all entries before committing any");
  put(root / "home/Documents/second.txt.t5-new", "new-second");
  check(disk.mount() && get(root / "home/Documents/second.txt") == "new-second",
        "repairing stage permits roll-forward recovery");
  auto tiny = base / "tiny";
  fs::create_directories(tiny / "data");
  fs::create_directories(tiny / "backups");
  fs::create_directories(tiny / "meta");
  put(tiny / "data/document", std::string(200, 'a'));
  Storage quota(tiny.string(), 1000, 100);
  check(quota.mount() && quota.backup("/backups/one.t5bak"),
        "small quota snapshot fixture");
  check(quota.write("/data/document", std::string(200, 'b')),
        "small quota live edit");
  check(!quota.restore("/backups/one.t5bak") &&
            get(tiny / "data/document") == std::string(200, 'b'),
        "restore reserves full staging peak before replacing originals");
  // Native timer polling does not wait for the filesystem mutex or Lua worker.
  open("files");
  auto timerBase = r.now();
  check(disk.write("/data/organizer/alarms.tsv",
                   "T5DATA1\nlongcopy\tAlarme durante cópia\t" +
                       std::to_string(timerBase + 10) + "\t0\t1\n"),
        "future alarm fixture");
  tick();
  std::atomic<bool> copyEntered{false}, copyDone{false};
  disk.setFreeSpaceProvider([&] {
    copyEntered = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    return UINT64_MAX / 2;
  });
  r.beginOperation();
  std::thread longCopy([&] {
    r.dispatch("copy",
               "/home/Documents/contacts.vcf\n/home/Documents/timer-copy.vcf");
    copyDone = true;
  });
  while (!copyEntered.load())
    std::this_thread::yield();
  bool probePresent = true;
  check(!disk.probe([] { return false; }, probePresent) && probePresent,
        "card probe skips a busy filesystem without blocking keyboard polling");
  r.setClock(timerBase + 11);
  auto due = r.pollTimers();
  bool during = false;
  for (auto &n : due)
    if (n.id == "alarm:longcopy")
      during = n.alarm && n.sound;
  if (during)
    r.emitSignal(true);
  check(during && !copyDone,
        "native alarm fires while copy holds filesystem lock");
  check(r.pollTimers().empty(),
        "native timer deduplicates before journal flush");
  longCopy.join();
  disk.setFreeSpaceProvider({});
  tick();
  bool delivered = false;
  for (auto &n : r.notices())
    if (n.id == "alarm:longcopy")
      delivered = true;
  check(delivered, "service persists native delivery after long operation");
  // Recipients carry a VM generation, so closing/reopening drops old events.
  auto manifest = get(root / "boot/manifest.txt");
  std::string mailbox =
      "local n=0;function event(e)if e=='queue'then "
      "sys.send('mailbox','late','')elseif e=='late'then n=n+1 elseif "
      "e=='explode'then while true do end end;return "
      "{title='Mailbox',body=tostring(n),buttons={}}end";
  put(root / "system/releases/0.7.1/mailbox.lua", mailbox);
  put(root / "boot/manifest.txt",
      manifest + "app.mailbox=mailbox.lua\nfile=mailbox.lua|" +
          sha256(mailbox) + "\n");
  check(r.boot(), "generation fixture boot");
  open("mailbox");
  action("queue");
  open("shell");
  check(r.close("mailbox"), "close queued recipient");
  open("mailbox");
  tick();
  action("refresh");
  check(r.view().body == "0", "events for prior VM generation discarded");
  r.beginOperation();
  check(!r.dispatch("explode") && r.faulted(),
        "active script watchdog quarantines app");
  open("shell");
  check(r.close("mailbox"),
        "faulted app can be closed without executing faulty checkpoint");
  open("notes");
  action("suspend", "draft survives failed checkpoint");
  open("shell");
  disk.setFreeSpaceProvider([] { return 0; });
  check(!r.close("notes"),
        "failed checkpoint prevents unloading a healthy app");
  disk.setFreeSpaceProvider({});
  open("notes");
  check(r.view().text == "draft survives failed checkpoint",
        "failed checkpoint preserves in-memory draft");
  auto upgrade = base / "upgrade/SymbianOS";
  fs::create_directories(upgrade.parent_path());
  fs::copy(argv[1], upgrade, fs::copy_options::recursive);
  fs::remove_all(upgrade / "data");
  fs::remove_all(upgrade / "config/public");
  put(upgrade / "data/notes/document.txt", "Documento anterior à atualização");
  Storage upgradedDisk(upgrade.string());
  Runtime upgraded(upgradedDisk);
  check(upgraded.boot(),
        "validated upgrade creates missing app/config directories");
  check(get(upgrade / "data/notes/document.txt") ==
                "Documento anterior à atualização" &&
            fs::is_directory(upgrade / "data/organizer") &&
            fs::is_directory(upgrade / "config/public"),
        "upgrade preserves documents while provisioning directories");
  check(upgradedDisk.write(
            "/data/organizer/contacts.tsv",
            "T5DATA1\nz\tZeca\t999\tz@example.org\na\tÁlvaro\t111\ta@example."
            "org\nb\tAna\t222\tb@example.org\n"),
        "unsorted contact fixture");
  check(upgraded.open("contacts") && upgraded.view().layout == "contacts" &&
            upgraded.view().buttons[0].label == "Álvaro" &&
            upgraded.view().buttons[1].label == "Ana" &&
            upgraded.view().buttons[2].label == "Zeca" &&
            upgraded.view().buttons[0].subtitle == "111",
        "accent-aware alphabetical contact list with phone subtitle");
  check(upgraded.dispatch("edit:a") && upgraded.view().layout == "form" &&
            upgraded.view().fields == "Nome\nTelefone\nE-mail",
        "contact structured fields");
  check(upgraded.open("calculator") && upgraded.view().layout == "calculator" &&
            upgraded.view().buttons.size() == 20,
        "calculator keypad schema");
  for (const auto &expression : {"1/0", "(2+3", "abc", "", "2+", "1e999"})
    check(upgraded.dispatch("calculate", expression) && !upgraded.faulted(),
          "invalid calculator input remains recoverable");
  check(upgraded.dispatch("calculate", "8/2") && upgraded.view().body == "4",
        "calculator recovers after invalid expressions");
  upgraded.shutdown();
  fs::remove_all(upgrade / "data/organizer");
  put(upgrade / "data/organizer", "keep this file");
  check(!upgraded.boot() && get(upgrade / "data/organizer") == "keep this file",
        "directory/file collision refuses destructive migration");
  fs::remove_all(base);
  std::cout << checks << " phase 2/3 checks passed\n";
}
