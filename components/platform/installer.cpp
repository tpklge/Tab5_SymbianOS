// SPDX-License-Identifier: MIT
#include "installer.hpp"
#include <algorithm>
#include <set>
#include <sstream>
namespace tab5 {
static uint32_t number(const std::string &s, size_t at, int n) {
  uint32_t v = 0;
  for (int i = n - 1; i >= 0; --i)
    v = (v << 8) | uint8_t(s[at + i]);
  return v;
}
static uint32_t crc(const std::string &s) {
  uint32_t c = ~0u;
  for (unsigned char b : s) {
    c ^= b;
    for (int i = 0; i < 8; ++i)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
bool Installer::identifier(const std::string &s) {
  if (s.empty() || s.size() > 32)
    return false;
  for (char c : s)
    if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_' &&
        c != '-')
      return false;
  return s != "shell" && s != "services";
}
bool Installer::parse(const std::string &text, Package &p) {
  p = {};
  p.manifest = text;
  if (text.size() > 16384 || text.find('\0') != std::string::npos) {
    error = "invalid manifest";
    return false;
  }
  std::istringstream lines(text);
  std::string line;
  std::map<std::string, std::string> fields;
  while (std::getline(lines, line)) {
    auto at = line.find('=');
    if (at == std::string::npos) {
      error = "invalid manifest line";
      return false;
    }
    auto k = line.substr(0, at), v = line.substr(at + 1);
    if (k == "file") {
      auto bar = v.find('|');
      if (bar == std::string::npos) {
        error = "invalid file hash";
        return false;
      }
      auto name = v.substr(0, bar), hash = v.substr(bar + 1);
      if (!Storage::validPath("/" + name) ||
          name.find('/') != std::string::npos || hash.size() != 64 ||
          !p.hashes.emplace(name, hash).second) {
        error = "invalid package file";
        return false;
      }
      for (char c : hash)
        if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) {
          error = "invalid hash";
          return false;
        }
    } else if (!fields.emplace(k, v).second) {
      error = "duplicate manifest key";
      return false;
    }
  }
  p.id = fields["id"];
  p.version = fields["version"];
  p.title = fields["title"];
  p.author = fields["author"];
  p.permissions = fields["permissions"];
  p.orientation = fields["orientation"];
  if (fields["format"] != "1" || !identifier(p.id) || p.version.empty() ||
      p.version.size() > 32 || p.title.empty() || p.title.size() > 80 ||
      p.author.empty() || p.author.size() > 80 || fields["api_min"] != "4" ||
      fields["api_max"] != "4" || p.orientation != "landscape" ||
      fields["memory"] != "1048576" || fields["instructions"] != "100000" ||
      (fields["kind"] != "app" && fields["kind"] != "theme") ||
      (fields["kind"] == "app" && !p.hashes.count("main.lua")) ||
      (fields["kind"] == "theme" && !p.hashes.count("theme.txt")) ||
      (p.hashes.count("theme.txt") && p.hashes.count("main.lua")) ||
      (!p.hashes.count("main.lua") && !p.hashes.count("theme.txt")) ||
      p.hashes.size() > 32) {
    error = "incompatible package metadata/API";
    return false;
  }
  if (p.version.front() == '.' || p.version.back() == '.' ||
      p.version.find("..") != std::string::npos) {
    error = "invalid version";
    return false;
  }
  for (char c : p.version)
    if (!(c >= '0' && c <= '9') && c != '.') {
      error = "invalid version";
      return false;
    }
  std::istringstream permissions(p.permissions);
  while (std::getline(permissions, line, ','))
    if (!line.empty() && line != "documents" && line != "media" &&
        line != "camera") {
      error = "unsupported permission";
      return false;
    }
  return true;
}
bool Installer::archive(const std::string &path, Package &p,
                        std::map<std::string, std::string> &files) {
  error.clear();
  files.clear();
  std::string zip;
  if (!disk_.read(path, zip, 4 * 1024 * 1024)) {
    error = disk_.lastError();
    return false;
  }
  auto fail = [&](const char *s) {
    error = s;
    return false;
  };
  if (zip.size() < 22)
    return fail("invalid ZIP");
  size_t end = zip.size() - 22;
  if (number(zip, end, 4) != 0x06054b50 || number(zip, end + 4, 4) ||
      number(zip, end + 20, 2))
    return fail("unsupported ZIP footer");
  unsigned count = number(zip, end + 10, 2);
  size_t central = number(zip, end + 16, 4),
         centralSize = number(zip, end + 12, 4);
  if (count < 2 || count > 33 || number(zip, end + 8, 2) != count ||
      central > end || centralSize != end - central)
    return fail("ZIP limits");
  std::set<std::string> aliases;
  size_t at = central;
  uint64_t total = 0;
  for (unsigned i = 0; i < count; ++i) {
    if (at + 46 > end || number(zip, at, 4) != 0x02014b50)
      return fail("invalid ZIP directory");
    size_t n = number(zip, at + 28, 2), extra = number(zip, at + 30, 2),
           comment = number(zip, at + 32, 2), offset = number(zip, at + 42, 4),
           bytes = number(zip, at + 24, 4);
    if (n == 0 || n > 100 || extra || comment || number(zip, at + 8, 2) ||
        number(zip, at + 10, 2) || number(zip, at + 20, 4) != bytes ||
        bytes > 1024 * 1024 || at + 46 + n > end || number(zip, at + 34, 2))
      return fail("ZIP requires stored, unencrypted, flat entries");
    std::string name = zip.substr(at + 46, n), alias = name;
    if (name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz0123456789_.-") != std::string::npos)
      return fail("ASCII package filenames required");
    std::transform(alias.begin(), alias.end(), alias.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (name.find('/') != std::string::npos ||
        !Storage::validPath("/" + name) || !aliases.insert(alias).second)
      return fail("invalid or duplicate ZIP path");
    if (offset > central || central - offset < 30 ||
        number(zip, offset, 4) != 0x04034b50 || number(zip, offset + 6, 2) ||
        number(zip, offset + 8, 2) || number(zip, offset + 26, 2) != n ||
        number(zip, offset + 28, 2) || number(zip, offset + 18, 4) != bytes ||
        number(zip, offset + 22, 4) != bytes)
      return fail("invalid local ZIP header");
    size_t data = offset + 30 + n;
    if (data > central || bytes > central - data ||
        zip.substr(offset + 30, n) != name)
      return fail("ZIP bounds");
    auto content = zip.substr(data, bytes);
    if (crc(content) != number(zip, at + 16, 4) ||
        number(zip, offset + 14, 4) != number(zip, at + 16, 4))
      return fail("ZIP CRC mismatch");
    total += bytes;
    if (total > 4 * 1024 * 1024)
      return fail("unpacked limit");
    files.emplace(name, std::move(content));
    at += 46 + n;
  }
  if (at != end || !files.count("manifest.txt") ||
      !parse(files["manifest.txt"], p))
    return false;
  if (files.size() != p.hashes.size() + 1)
    return fail("uncatalogued ZIP entry");
  for (const auto &f : p.hashes)
    if (!files.count(f.first) || sha256(files[f.first]) != f.second)
      return fail("package hash mismatch");
  for (const auto &file : files)
    if (file.first.size() > 4 &&
        file.first.substr(file.first.size() - 4) == ".lua" &&
        (file.second.size() > 65536 ||
         (!file.second.empty() && uint8_t(file.second[0]) == 27)))
      return fail("Lua source only, up to 64 KiB");
  p.archiveHash = sha256(zip);
  p.bytes = total;
  return true;
}
bool Installer::inspect(const std::string &path, Package &p) {
  std::map<std::string, std::string> files;
  return archive(path, p, files);
}
bool Installer::catalog(std::vector<Package> &out) {
  out.clear();
  std::string registry;
  if (!disk_.read("/apps/registry.tsv", registry, 65536)) {
    if (disk_.lastError() == "not found")
      return true;
    error = disk_.lastError();
    return false;
  }
  std::istringstream lines(registry);
  std::string line;
  std::getline(lines, line);
  if (line != "T5APPS1") {
    error = "invalid app registry";
    return false;
  }
  std::set<std::string> ids;
  while (std::getline(lines, line)) {
    std::istringstream fields(line);
    std::string id, version, hash, text;
    std::getline(fields, id, '\t');
    std::getline(fields, version, '\t');
    std::getline(fields, hash);
    if (!identifier(id) || version.empty() ||
        version.find_first_not_of("0123456789.") != std::string::npos ||
        !ids.insert(id).second || out.size() >= 32 ||
        !disk_.read("/apps/" + id + "/" + version + "/manifest.txt", text,
                    16384) ||
        sha256(text) != hash) {
      error = "installed manifest integrity";
      return false;
    }
    Package p;
    if (!parse(text, p) || p.id != id || p.version != version)
      return false;
    out.push_back(std::move(p));
  }
  return true;
}
static std::string registryText(const std::vector<Package> &all) {
  std::string text = "T5APPS1\n";
  for (const auto &p : all)
    text += p.id + "\t" + p.version + "\t" + sha256(p.manifest) + "\n";
  return text;
}
bool Installer::install(const std::string &path,
                        const std::atomic<bool> *cancel,
                        const std::string &expectedHash) {
  Package p;
  std::map<std::string, std::string> files;
  if (!archive(path, p, files))
    return false;
  if (!expectedHash.empty() && p.archiveHash != expectedHash) {
    error = "package changed after review";
    return false;
  }
  std::vector<Package> all;
  if (!catalog(all))
    return false;
  std::string dest = "/apps/" + p.id + "/" + p.version;
  for (const auto &a : all)
    if (a.id == p.id && a.version == p.version) {
      error = "version already active";
      return false;
    }
  if (all.size() >= 32 && std::none_of(all.begin(), all.end(),
                                       [&](auto &a) { return a.id == p.id; })) {
    error = "installed app limit";
    return false;
  }
  std::vector<File> existing;
  if (disk_.listDir(dest, existing) && !disk_.removeTree(dest)) {
    error = disk_.lastError();
    return false;
  }
  if (!disk_.canFit(p.bytes + 1024 * 1024 + 65536)) {
    error = disk_.lastError();
    return false;
  }
  if (!disk_.prepare(dest)) {
    error = disk_.lastError();
    return false;
  }
  for (const auto &f : files)
    if (!disk_.write(dest + "/" + f.first, f.second, cancel)) {
      error = disk_.lastError();
      return false;
    }
  if (cancel && cancel->load()) {
    error = "cancelled before activation";
    return false;
  }
  all.erase(std::remove_if(all.begin(), all.end(),
                           [&](auto &a) { return a.id == p.id; }),
            all.end());
  all.push_back(p);
  if (!disk_.prepare("/data/" + p.id) ||
      !disk_.write("/apps/registry.tsv", registryText(all), cancel)) {
    error = disk_.lastError();
    return false;
  }
  return true;
}
bool Installer::remove(const std::string &id, bool eraseData) {
  std::vector<Package> all;
  if (!identifier(id) || !catalog(all))
    return false;
  auto old = all.size();
  all.erase(std::remove_if(all.begin(), all.end(),
                           [&](auto &a) { return a.id == id; }),
            all.end());
  if (old == all.size()) {
    error = "app not installed";
    return false;
  }
  if (!disk_.write("/apps/registry.tsv", registryText(all))) {
    error = disk_.lastError();
    return false;
  }
  if (!disk_.removeTree("/apps/" + id)) {
    error = "app deactivated; payload cleanup failed: " + disk_.lastError();
    return false;
  }
  if (eraseData && !disk_.removeTree("/data/" + id)) {
    error = "app removed; data cleanup failed: " + disk_.lastError();
    return false;
  }
  return true;
}
bool Installer::read(const Package &p, const std::string &name,
                     std::string &out) {
  auto found = p.hashes.find(name);
  if (found == p.hashes.end()) {
    error = "asset not in package";
    return false;
  }
  if (!disk_.read("/apps/" + p.id + "/" + p.version + "/" + name, out,
                  1024 * 1024) ||
      sha256(out) != found->second) {
    error = "installed asset integrity";
    return false;
  }
  return true;
}
} // namespace tab5
