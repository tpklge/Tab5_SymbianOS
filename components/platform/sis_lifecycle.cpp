// SPDX-License-Identifier: MIT
#include "digest.hpp"
#include "installer.hpp"
#include "text_lines.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
namespace tab5 {
namespace {
const std::string journal = "/meta/sis-operation.txt",
                  work = "/tmp/install/sis-lifecycle";
void require(bool ok, const std::string &error) {
  if (!ok)
    throw std::runtime_error(error.empty() ? "SIS storage operation failed"
                                           : error);
}
bool exists(Storage &d, const std::string &p) {
  bool value = false;
  require(d.pathExists(p, value), d.lastError());
  return value;
}
void eraseTree(Storage &d, const std::string &p) {
  if (exists(d, p))
    require(d.removeTree(p), d.lastError());
}
std::string readText(Storage &d, const std::string &p, size_t max = 65536) {
  std::string out;
  require(d.read(p, out, max), d.lastError());
  return out;
}
void write(Storage &d, const std::string &p, const std::string &text) {
  require(d.write(p, text), d.lastError());
}
bool hex(const std::string &s, size_t count = 64) {
  return s.size() == count && s.find_first_not_of("0123456789abcdef") == s.npos;
}
std::array<uint32_t, 3> version(const std::string &s) {
  std::array<uint32_t, 3> out{};
  TextLines input(s);
  std::string part, extra;
  for (auto &n : out) {
    require(nextLine(input, part, '.') && !part.empty() && part.size() <= 10 &&
                part.find_first_not_of("0123456789") == part.npos,
            "invalid SIS version");
    uint64_t value = 0;
    for (char c : part)
      value = value * 10 + c - '0';
    require(value <= 2147483647 && std::to_string(value) == part,
            "invalid SIS version");
    n = uint32_t(value);
  }
  require(!nextLine(input, extra, '.'), "invalid SIS version");
  return out;
}
std::string registry(const std::vector<Package> &all) {
  std::string out = "T5APPS1\n";
  for (const auto &p : all)
    out += p.id + "\t" + p.version + "\t" + sha256(p.manifest) + "\n";
  return out;
}
struct Journal {
  std::string op, phase, id, oldVersion = "-", oldHash = "-", newVersion = "-",
                             newHash = "-", oldRegistry, newRegistry,
                             snapshot = "-";
  bool erase = false, created = false;
  std::string body() const {
    return "T5SIS2\n" + op + "\n" + phase + "\n" + id + "\n" + oldVersion +
           "\n" + oldHash + "\n" + newVersion + "\n" + newHash + "\n" +
           (erase ? "1" : "0") + "\n" + oldRegistry + "\n" + newRegistry +
           "\n" + snapshot + "\n" + (created ? "1" : "0") + "\n";
  }
  void save(Storage &d) {
    auto text = body();
    write(d, journal, text + sha256(text) + "\n");
    // Bounded diagnostic, never contains application data or credentials.
    std::string log;
    d.read("/logs/sis-lifecycle.txt", log, 16384);
    if (log.size() > 12000) {
      auto cut = log.find('\n', log.size() - 8000);
      log = cut == log.npos ? std::string() : log.substr(cut + 1);
    }
    if (d.prepare("/logs"))
      d.write("/logs/sis-lifecycle.txt", log + op + " " + id + " " +
                                             oldVersion + " -> " + newVersion +
                                             " " + phase + "\n");
  }
  static Journal load(Storage &d) {
    auto text = readText(d, journal, 2048);
    TextLines input(text);
    std::vector<std::string> f;
    std::string line;
    while (nextLine(input, line))
      f.push_back(line);
    require(f.size() == 14 && f[0] == "T5SIS2" && hex(f[13]),
            "invalid SIS lifecycle journal; preserved");
    Journal j;
    j.op = f[1];
    j.phase = f[2];
    j.id = f[3];
    j.oldVersion = f[4];
    j.oldHash = f[5];
    j.newVersion = f[6];
    j.newHash = f[7];
    j.erase = f[8] == "1";
    j.oldRegistry = f[9];
    j.newRegistry = f[10];
    j.snapshot = f[11];
    j.created = f[12] == "1";
    require((j.op == "update" || j.op == "rollback" || j.op == "remove") &&
                (j.phase == "prepared" || j.phase == "trial" ||
                 j.phase == "started" || j.phase == "restoring" ||
                 j.phase == "finalizing" || j.phase == "cleanup") &&
                Installer::identifier(j.id) && (f[8] == "0" || f[8] == "1") &&
                (f[12] == "0" || f[12] == "1") && hex(j.oldRegistry) &&
                hex(j.newRegistry) && sha256(j.body()) == f[13],
            "invalid SIS lifecycle journal; preserved");
    if (j.oldVersion != "-") {
      version(j.oldVersion);
      require(hex(j.oldHash) && j.snapshot == "snapshot-" + j.oldHash,
              "invalid SIS previous identity");
    }
    if (j.newVersion != "-") {
      version(j.newVersion);
      require(hex(j.newHash), "invalid SIS candidate identity");
    }
    require((j.op == "remove" && j.oldVersion != "-" && j.newVersion == "-") ||
                (j.op != "remove" && j.newVersion != "-"),
            "invalid SIS operation");
    return j;
  }
};
struct TreeFile {
  std::string relative;
  uint64_t bytes;
};
std::vector<TreeFile> tree(Storage &d, const std::string &root) {
  std::vector<TreeFile> files;
  std::vector<std::pair<std::string, unsigned>> pending{{root, 0}};
  uint64_t total = 0;
  unsigned entries = 0;
  while (!pending.empty()) {
    auto item = pending.back();
    pending.pop_back();
    require(item.second <= 8, "SIS data depth exceeds 8");
    std::vector<File> children;
    require(d.listDir(item.first, children), d.lastError());
    for (const auto &f : children) {
      require(++entries <= 512, "SIS data tree exceeds 512 entries");
      if (f.directory)
        pending.push_back({f.path, item.second + 1});
      else {
        total += f.size;
        require(total <= 64 * 1024 * 1024 && files.size() < 256,
                "SIS data snapshot exceeds 64 MiB / 256 files");
        files.push_back({f.path.substr(root.size() + 1), f.size});
      }
    }
  }
  std::sort(files.begin(), files.end(),
            [](auto &a, auto &b) { return a.relative < b.relative; });
  return files;
}
std::string hashFile(Storage &d, const std::string &path) {
  uint64_t size = 0;
  require(d.fileSize(path, size), d.lastError());
  Digest digest;
  for (uint64_t at = 0; at < size; at += 16384) {
    std::string block;
    size_t n = std::min(uint64_t(16384), size - at);
    require(d.readRange(path, at, n, block) && block.size() == n,
            "SIS snapshot read failed");
    digest.update(block.data(), block.size());
  }
  return digest.finish();
}
void checkRebase(Storage &d, const std::string &src, const std::string &dst) {
  std::vector<std::string> pending{src};
  unsigned count = 0;
  while (!pending.empty()) {
    auto dir = pending.back();
    pending.pop_back();
    auto to = dst + dir.substr(src.size());
    require(Storage::validPath(to) &&
                std::count(to.begin(), to.end(), '/') <= 16,
            "SIS snapshot path too long");
    std::vector<File> files;
    require(d.listDir(dir, files), d.lastError());
    for (const auto &f : files) {
      require(++count <= 512, "SIS snapshot entry limit");
      auto target = dst + f.path.substr(src.size());
      require(Storage::validPath(target), "SIS snapshot path too long");
      if (f.directory)
        pending.push_back(f.path);
    }
  }
}
std::string inventory(Storage &d, const std::string &root) {
  std::string out = "T5SNAP1\n";
  for (const auto &f : tree(d, root))
    out += f.relative + "|" + std::to_string(f.bytes) + "|" +
           hashFile(d, root + "/" + f.relative) + "\n";
  return out;
}
void clone(Storage &d, const std::string &src, const std::string &dst,
           const std::atomic<bool> *cancel) {
  require(d.prepare(dst), d.lastError());
  std::vector<std::string> dirs{src};
  unsigned count = 0;
  while (!dirs.empty()) {
    auto dir = dirs.back();
    dirs.pop_back();
    std::vector<File> children;
    require(d.listDir(dir, children), d.lastError());
    for (auto &f : children)
      if (f.directory) {
        require(++count <= 512, "SIS snapshot directory limit");
        require(d.prepare(dst + f.path.substr(src.size())), d.lastError());
        dirs.push_back(f.path);
      }
  }
  for (const auto &f : tree(d, src)) {
    auto to = dst + "/" + f.relative;
    auto slash = to.rfind('/');
    require(d.prepare(to.substr(0, slash)), d.lastError());
    require(d.copy(src + "/" + f.relative, to, cancel), d.lastError());
    require(hashFile(d, src + "/" + f.relative) == hashFile(d, to),
            "SIS snapshot copy mismatch");
  }
}
bool flat(const std::string &s) {
  return !s.empty() &&
         s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvw"
                             "xyz0123456789_.-") == s.npos &&
         Storage::validPath("/" + s);
}
void migrate(Storage &d, const Package &p, const std::string &commands) {
  require(commands.size() <= 16384, "SIS migration command size");
  TextLines lines(commands);
  std::string line;
  require(nextLine(lines, line) && line == "T5MIGRATE1",
          "invalid SIS migration");
  unsigned count = 0;
  while (nextLine(lines, line)) {
    require(++count <= 32, "SIS migration operation limit");
    auto first = line.find('\t');
    auto second = first == line.npos ? line.npos : line.find('\t', first + 1);
    require(first != line.npos && second != line.npos &&
                line.find('\t', second + 1) == line.npos,
            "unsafe SIS migration");
    auto op = line.substr(0, first),
         a = line.substr(first + 1, second - first - 1),
         b = line.substr(second + 1);
    require(flat(a), "unsafe SIS migration");
    auto root = work + "/data-new/";
    if (op == "rename") {
      auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        return value;
      };
      require(flat(b) && lower(a) != lower(b), "unsafe SIS rename");
      if (exists(d, root + a)) {
        uint64_t bytes = 0;
        require(d.fileSize(root + a, bytes),
                "SIS migration accepts files only");
        require(!exists(d, root + b), "SIS migration target exists");
        require(d.move(root + a, root + b), d.lastError());
      }
    } else if (op == "default") {
      require(b.size() <= 8192 && b.size() % 2 == 0 &&
                  b.find_first_not_of("0123456789abcdef") == b.npos,
              "invalid SIS migration value");
      if (!exists(d, root + a)) {
        std::string data;
        for (size_t i = 0; i < b.size(); i += 2) {
          auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
          data += char(digit(b[i]) * 16 + digit(b[i + 1]));
        }
        write(d, root + a, data);
      }
    } else
      require(false, "unsupported SIS migration operation");
  }
  (void)p;
}
} // namespace
bool Installer::validateSisGraph(const std::vector<Package> &all) {
  try {
    std::map<uint32_t, const Package *> packages;
    std::set<std::string> ids;
    for (const auto &p : all) {
      require(ids.insert(p.id).second, "duplicate app ID");
      if (p.uid) {
        require(packages.emplace(p.uid, &p).second, "SIS UID collision");
        version(p.version);
      }
    }
    std::map<uint32_t, unsigned> color;
    std::function<void(uint32_t)> visit = [&](uint32_t uid) {
      require(color[uid] != 1, "SIS dependency cycle");
      if (color[uid] == 2)
        return;
      color[uid] = 1;
      const auto &p = *packages.at(uid);
      std::set<uint32_t> seen;
      for (const auto &dep : p.dependencies) {
        require(dep.uid != uid && seen.insert(dep.uid).second &&
                    packages.count(dep.uid),
                "SIS dependency missing or repeated: " +
                    std::to_string(dep.uid));
        auto lo = version(dep.minimum), hi = version(dep.maximum),
             active = version(packages.at(dep.uid)->version);
        require(lo <= hi && lo <= active && active <= hi,
                "SIS dependency version mismatch: " + std::to_string(dep.uid));
        visit(dep.uid);
      }
      color[uid] = 2;
    };
    for (const auto &p : packages)
      visit(p.first);
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::pendingSis(std::string &id, std::string &value) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  id.clear();
  value.clear();
  try {
    if (!exists(disk_, journal))
      return false;
    auto j = Journal::load(disk_);
    if (j.phase == "trial" || j.phase == "started") {
      id = j.id;
      value = j.newVersion;
      return true;
    }
    return false;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::previousSis(const std::string &id, Package &p) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  try {
    require(identifier(id), "invalid app ID");
    auto pointer = "/meta/sis/" + id + "/previous.txt";
    if (!exists(disk_, pointer))
      return false;
    TextLines lines(readText(disk_, pointer));
    std::string v, h, snapshot, extra;
    require(nextLine(lines, v) && nextLine(lines, h) &&
                nextLine(lines, snapshot) && !nextLine(lines, extra) &&
                hex(h) && snapshot == "snapshot-" + h,
            "invalid previous SIS record");
    version(v);
    auto text =
        readText(disk_, "/apps/" + id + "/" + v + "/manifest.txt", 16384);
    require(sha256(text) == h && parse(text, p) && p.id == id &&
                p.version == v && p.uid,
            "previous SIS integrity");
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::recoverLifecycle(bool boot) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  try {
    return disk_.atGeneration(disk_.generation(), [&] {
      if (!exists(disk_, journal))
        return true;
      auto j = Journal::load(disk_);
      auto old = readText(disk_, work + "/registry-old"),
           next = readText(disk_, work + "/registry-new");
      require(sha256(old) == j.oldRegistry && sha256(next) == j.newRegistry,
              "SIS recovery registry integrity");
      auto active = exists(disk_, "/apps/registry.tsv")
                        ? readText(disk_, "/apps/registry.tsv")
                        : "T5APPS1\n";
      require(active == old || active == next,
              "SIS registry changed outside transaction");
      if (j.phase == "restoring")
        return lifecycleFinish(true);
      if (j.phase == "finalizing")
        return lifecycleFinish(false);
      if (active == old) {
        j.phase = "restoring";
        j.save(disk_);
        return lifecycleFinish(true);
      }
      if (j.op == "remove") {
        j.phase = "cleanup";
        j.save(disk_);
        eraseTree(disk_, "/apps/" + j.id);
        eraseTree(disk_, "/meta/sis/" + j.id);
        if (j.erase) {
          eraseTree(disk_, "/data/" + j.id);
          if (exists(disk_, "/meta/sis-data/" + j.id + ".txt"))
            require(disk_.remove("/meta/sis-data/" + j.id + ".txt"),
                    disk_.lastError());
        }
        require(disk_.remove(journal), disk_.lastError());
        eraseTree(disk_, work);
        return true;
      }
      if (boot && j.phase == "started") {
        j.phase = "restoring";
        j.save(disk_);
        return lifecycleFinish(true);
      }
      // A stable pending transaction was verified when published; the runtime
      // still checks every asset it loads. Recheck all files after
      // boot/recovery.
      if (!boot && (j.phase == "trial" || j.phase == "started"))
        return true;
      // Verify the complete candidate before allowing any first launch.
      auto valid = [&]() {
        try {
          auto manifest = readText(
              disk_, "/apps/" + j.id + "/" + j.newVersion + "/manifest.txt",
              16384);
          Package p;
          if (sha256(manifest) != j.newHash || !parse(manifest, p))
            return false;
          for (const auto &f : p.hashes)
            if (hashFile(disk_, "/apps/" + j.id + "/" + j.newVersion + "/" +
                                    f.first) != f.second)
              return false;
          return true;
        } catch (const std::exception &) {
          return false;
        }
      };
      if (!valid()) {
        j.phase = "restoring";
        j.save(disk_);
        return lifecycleFinish(true);
      }
      if (boot && j.phase == "started") {
        j.phase = "restoring";
        j.save(disk_);
        return lifecycleFinish(true);
      }
      if (j.phase == "prepared") {
        j.phase = "trial";
        j.save(disk_);
      }
      return true;
    });
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::lifecycleFinish(bool restore) {
  try {
    auto step = [&](const std::string &name) {
#ifndef ESP_PLATFORM
      if (transactionCheckpoint && !transactionCheckpoint(name))
        throw std::runtime_error("injected finalization interruption at " +
                                 name);
#endif
      (void)name;
    };
    auto j = Journal::load(disk_);
    auto data = "/data/" + j.id;
    auto meta = "/meta/sis/" + j.id;
    if (restore) {
      if (j.phase != "restoring") {
        j.phase = "restoring";
        j.save(disk_);
      }
      if (exists(disk_, work + "/data-old")) {
        eraseTree(disk_, data);
        require(disk_.move(work + "/data-old", data), disk_.lastError());
      }
      auto old = readText(disk_, work + "/registry-old");
      require(sha256(old) == j.oldRegistry, "SIS rollback registry integrity");
      write(disk_, "/apps/registry.tsv", old);
      if (j.created && j.newVersion != "-")
        eraseTree(disk_, "/apps/" + j.id + "/" + j.newVersion);
      if (j.oldVersion == "-" && exists(disk_, "/apps/" + j.id)) {
        std::vector<File> children;
        require(disk_.listDir("/apps/" + j.id, children), disk_.lastError());
        if (children.empty())
          require(disk_.remove("/apps/" + j.id), disk_.lastError());
      }
    } else {
      if (j.phase != "finalizing") {
        j.phase = "finalizing";
        j.save(disk_);
      }
      step("finalizing");
      if (j.oldVersion != "-") {
        require(disk_.prepare(meta), disk_.lastError());
        auto snapshot = meta + "/" + j.snapshot;
        require(disk_.prepare(snapshot), disk_.lastError());
        auto expected = readText(disk_, work + "/inventory-old", 131072);
        if (exists(disk_, work + "/data-old")) {
          require(!exists(disk_, snapshot + "/data"),
                  "SIS snapshot destination collision");
          require(disk_.move(work + "/data-old", snapshot + "/data"),
                  disk_.lastError());
        }
        step("snapshot_published");
        require(inventory(disk_, snapshot + "/data") == expected,
                "SIS previous data integrity");
        write(disk_, snapshot + "/inventory.txt", expected);
        write(disk_, meta + "/previous.txt",
              j.oldVersion + "\n" + j.oldHash + "\n" + j.snapshot + "\n");
        step("previous_recorded");
        // Only the current and immediately previous immutable payload survive.
        std::vector<File> versions;
        require(disk_.listDir("/apps/" + j.id, versions), disk_.lastError());
        for (const auto &f : versions)
          if (f.directory && f.path != "/apps/" + j.id + "/" + j.oldVersion &&
              f.path != "/apps/" + j.id + "/" + j.newVersion)
            eraseTree(disk_, f.path);
        std::vector<File> snapshots;
        require(disk_.listDir(meta, snapshots), disk_.lastError());
        for (const auto &f : snapshots)
          if (f.directory && f.path != snapshot)
            eraseTree(disk_, f.path);
      }
      step("versions_pruned");
      Package active;
      auto manifest = readText(
          disk_, "/apps/" + j.id + "/" + j.newVersion + "/manifest.txt", 16384);
      require(parse(manifest, active), error);
      require(disk_.prepare("/meta/sis-data"), disk_.lastError());
      write(disk_, "/meta/sis-data/" + j.id + ".txt",
            std::to_string(active.uid) + "\n" + std::to_string(active.schema) +
                "\n");
      step("schema_recorded");
    }
    step("before_cleanup");
    require(disk_.remove(journal), disk_.lastError());
    eraseTree(disk_, work);
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::recoverSisBoot() {
  return recoverSis() && recoverLifecycle(true);
}
bool Installer::beginSisTrial(const std::string &id) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  try {
    if (!exists(disk_, journal))
      return true;
    auto j = Journal::load(disk_);
    if (j.id != id)
      return true;
    require(j.phase == "trial", "SIS trial unavailable");
    j.phase = "started";
    j.save(disk_);
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::finishSisTrial(const std::string &id, bool healthy) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  try {
    return disk_.atGeneration(disk_.generation(), [&] {
      if (!exists(disk_, journal))
        return true;
      auto j = Journal::load(disk_);
      if (j.id != id)
        return true;
      require(j.phase == "trial" || j.phase == "started",
              "SIS trial unavailable");
      return lifecycleFinish(!healthy);
    });
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::lifecycleInstall(const std::string &path,
                                 const std::atomic<bool> *cancel,
                                 const std::string &expectedHash,
                                 bool allowDowngrade) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  try {
    std::vector<Package> all;
    require(catalog(all), error);
    require(!exists(disk_, journal),
            "Abra o aplicativo pendente ou cancele a atualização primeiro");
    const auto generation = disk_.generation();
    const bool rollback = path.rfind("@rollback:", 0) == 0;
    Package p;
    std::map<std::string, std::string> files;
    if (rollback) {
      require(previousSis(path.substr(10), p), error);
      for (const auto &f : p.hashes) {
        std::string content;
        require(this->read(p, f.first, content), error);
        files[f.first] = std::move(content);
      }
      files["manifest.txt"] = p.manifest;
    } else {
      require(sisArchive(path, p, files, cancel, true), error);
      require(expectedHash.empty() || p.archiveHash == expectedHash,
              "package changed after review");
    }
    require(disk_.generation() == generation, "SIS card changed");
    version(p.version);
    Package old;
    bool upgrade = false;
    for (const auto &a : all) {
      if (a.id == p.id) {
        require(a.uid && a.uid == p.uid && a.author == p.author,
                "SIS identity or declared publisher changed");
        old = a;
        upgrade = true;
      } else
        require(!p.uid || a.uid != p.uid,
                "SIS UID already belongs to another app");
    }
    require(!upgrade || p.version != old.version, "version already active");
    require(!upgrade || rollback || allowDowngrade ||
                version(p.version) > version(old.version),
            "Downgrade requires explicit confirmation");
    require(upgrade || all.size() < 32, "installed app limit");
    auto next = all;
    next.erase(std::remove_if(next.begin(), next.end(),
                              [&](const Package &a) { return a.id == p.id; }),
               next.end());
    next.push_back(p);
    require(validateSisGraph(next), error);
    unsigned sourceSchema = upgrade ? old.schema : p.schema;
    auto marker = "/meta/sis-data/" + p.id + ".txt";
    if (!upgrade && exists(disk_, marker)) {
      TextLines lines(readText(disk_, marker));
      std::string uid, schema, extra;
      require(nextLine(lines, uid) && nextLine(lines, schema) &&
                  !nextLine(lines, extra) && uid == std::to_string(p.uid),
              "Retained data belongs to another SIS UID");
      require(!schema.empty() && schema.size() <= 4 &&
                  schema.find_first_not_of("0123456789") == schema.npos,
              "invalid retained data schema");
      sourceSchema = 0;
      for (char c : schema)
        sourceSchema = sourceSchema * 10 + c - '0';
    }
    require(rollback || sourceSchema == p.schema ||
                (p.migrateFrom == sourceSchema && !p.migration.empty()),
            "SIS data schema requires an explicit migration");
    auto checkpoint = [&](const std::string &step) {
#ifndef ESP_PLATFORM
      if (transactionCheckpoint && !transactionCheckpoint(step))
        throw std::runtime_error("injected interruption at " + step);
#endif
      require((!cancel || !cancel->load()) && disk_.generation() == generation,
              "SIS cancelled or card changed");
    };
    auto mutate = [&](const std::function<bool()> &fn) {
      require(disk_.atGeneration(generation, fn), disk_.lastError());
    };
    const auto data = "/data/" + p.id, dest = "/apps/" + p.id + "/" + p.version;
    mutate([&] {
      require(disk_.prepare("/meta") && disk_.prepare("/tmp/install") &&
                  disk_.prepare(data),
              disk_.lastError());
      eraseTree(disk_, work);
      return true;
    });
    uint64_t dataBytes = 0;
    for (const auto &f : tree(disk_, data))
      dataBytes += f.bytes;
    checkRebase(disk_, data, work + "/data-old");
    checkRebase(disk_, data, work + "/data-new");
    if (upgrade)
      checkRebase(disk_, data,
                  "/meta/sis/" + p.id + "/snapshot-" + sha256(old.manifest) +
                      "/data");
    require(disk_.canFit(p.bytes + dataBytes * 2 + 2 * 1024 * 1024),
            disk_.lastError());
    bool created = !exists(disk_, dest);
    if (!created) {
      require(readText(disk_, dest + "/manifest.txt", 16384) == p.manifest,
              "immutable SIS version collision");
      for (const auto &f : p.hashes)
        require(hashFile(disk_, dest + "/" + f.first) == f.second,
                "retained SIS payload damaged");
    }
    Journal j;
    j.op = rollback ? "rollback" : "update";
    j.phase = "prepared";
    j.id = p.id;
    j.newVersion = p.version;
    j.newHash = sha256(p.manifest);
    j.created = created;
    if (upgrade) {
      j.oldVersion = old.version;
      j.oldHash = sha256(old.manifest);
      j.snapshot = "snapshot-" + j.oldHash;
    }
    auto oldRegistry = registry(all), newRegistry = registry(next);
    j.oldRegistry = sha256(oldRegistry);
    j.newRegistry = sha256(newRegistry);
    mutate([&] {
      require(disk_.prepare(work) && disk_.prepare(work + "/payload"),
              disk_.lastError());
      write(disk_, work + "/registry-old", oldRegistry);
      write(disk_, work + "/registry-new", newRegistry);
      write(disk_, work + "/inventory-old", inventory(disk_, data));
      return true;
    });
    for (const auto &f : files) {
      checkpoint("before:" + f.first);
      mutate([&] {
        require(disk_.write(work + "/payload/" + f.first, f.second, cancel),
                disk_.lastError());
        require(hashFile(disk_, work + "/payload/" + f.first) ==
                    sha256(f.second),
                "SIS payload readback failed");
        return true;
      });
      checkpoint("write:" + f.first);
    }
    mutate([&] {
      if (rollback) {
        auto snapshot = "/meta/sis/" + p.id + "/snapshot-" + sha256(p.manifest);
        require(inventory(disk_, snapshot + "/data") ==
                    readText(disk_, snapshot + "/inventory.txt", 131072),
                "previous SIS snapshot damaged");
        checkRebase(disk_, snapshot + "/data", work + "/data-new");
        clone(disk_, snapshot + "/data", work + "/data-new", cancel);
      } else {
        clone(disk_, data, work + "/data-new", cancel);
        if (sourceSchema != p.schema)
          migrate(disk_, p, files.at(p.migration));
      }
      return true;
    });
    checkpoint("staged");
    mutate([&] {
      j.save(disk_);
      return true;
    });
    checkpoint("prepared");
    if (created)
      mutate([&] {
        require(disk_.prepare("/apps/" + p.id) &&
                    disk_.move(work + "/payload", dest),
                disk_.lastError());
        return true;
      });
    checkpoint("published");
    mutate([&] {
      require(disk_.move(data, work + "/data-old"), disk_.lastError());
      return true;
    });
    checkpoint("data_old");
    mutate([&] {
      require(disk_.move(work + "/data-new", data), disk_.lastError());
      return true;
    });
    checkpoint("data_new");
    checkpoint("before_activation");
    mutate([&] {
      require(disk_.write("/apps/registry.tsv", newRegistry, cancel),
              disk_.lastError());
      return true;
    });
#ifndef ESP_PLATFORM
    if (transactionCheckpoint && !transactionCheckpoint("activated"))
      throw std::runtime_error("injected interruption at activated");
#endif
    mutate([&] {
      j.phase = "trial";
      j.save(disk_);
      return true;
    });
#ifndef ESP_PLATFORM
    if (transactionCheckpoint && !transactionCheckpoint("trial"))
      throw std::runtime_error("injected interruption at trial");
#endif
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool Installer::installDowngrade(const std::string &path,
                                 const std::atomic<bool> *cancel,
                                 const std::string &hash) {
  return lifecycleInstall(path, cancel, hash, true);
}
bool Installer::rollbackSis(const std::string &id) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  std::string pending, ver;
  if (pendingSis(pending, ver)) {
    if (pending != id) {
      error = "Another SIS trial is pending";
      return false;
    }
    return finishSisTrial(id, false);
  }
  return lifecycleInstall("@rollback:" + id, nullptr, "", true);
}
bool Installer::lifecycleRemove(const std::string &id, bool eraseData) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  try {
    std::vector<Package> all;
    require(catalog(all), error);
    require(!exists(disk_, journal), "Validate or cancel pending SIS first");
    Package old;
    bool found = false;
    auto next = all;
    for (const auto &p : all)
      if (p.id == id) {
        old = p;
        found = true;
      }
    require(found && old.uid, "SIS not installed");
    next.erase(std::remove_if(next.begin(), next.end(),
                              [&](const Package &p) { return p.id == id; }),
               next.end());
    require(validateSisGraph(next), error);
    return disk_.atGeneration(disk_.generation(), [&] {
      eraseTree(disk_, work);
      require(disk_.prepare(work) && disk_.prepare("/meta/sis-data"),
              disk_.lastError());
      auto oldRegistry = registry(all), newRegistry = registry(next);
      write(disk_, work + "/registry-old", oldRegistry);
      write(disk_, work + "/registry-new", newRegistry);
      // Retained data keeps its UID/schema even after all payload is removed.
      write(disk_, "/meta/sis-data/" + id + ".txt",
            std::to_string(old.uid) + "\n" + std::to_string(old.schema) + "\n");
      Journal j;
      j.op = "remove";
      j.phase = "prepared";
      j.id = id;
      j.oldVersion = old.version;
      j.oldHash = sha256(old.manifest);
      j.snapshot = "snapshot-" + j.oldHash;
      j.erase = eraseData;
      j.oldRegistry = sha256(oldRegistry);
      j.newRegistry = sha256(newRegistry);
      j.save(disk_);
#ifndef ESP_PLATFORM
      if (transactionCheckpoint && !transactionCheckpoint("remove_prepared"))
        throw std::runtime_error("injected removal interruption");
#endif
      write(disk_, "/apps/registry.tsv", newRegistry);
#ifndef ESP_PLATFORM
      if (transactionCheckpoint && !transactionCheckpoint("remove_activated"))
        throw std::runtime_error("injected removal interruption");
#endif
      return recoverLifecycle();
    });
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
} // namespace tab5
