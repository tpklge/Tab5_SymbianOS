// SPDX-License-Identifier: MIT
#include "installer.hpp"
#include <algorithm>
#include <set>
#include "text_lines.hpp"
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
  TextLines lines(text);
  std::string line;
  std::map<std::string, std::string> fields;
  while (nextLine(lines, line)) {
    auto at = line.find('=');
    if (at == std::string::npos) {
      error = "invalid manifest line";
      return false;
    }
    auto k = line.substr(0, at), v = line.substr(at + 1);
    if(k=="depends") {
      TextLines values(v);std::string uid,lo,hi,extra;
      if(!nextLine(values,uid,'|')||!nextLine(values,lo,'|')||!nextLine(values,hi,'|')||nextLine(values,extra,'|')||uid.size()!=10||uid.find_first_not_of("0123456789")!=uid.npos||p.dependencies.size()>=16){error="invalid dependency";return false;}
      uint64_t n=0;for(char c:uid)n=n*10+c-'0';if(n<0xe0000000||n>0xefffffff){error="dependency UID";return false;}
      p.dependencies.push_back({uint32_t(n),lo,hi});
    } else if (k == "file") {
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
  if(fields.count("sis_profile")) {
    auto uid=fields["uid"];
    if((fields["sis_profile"]!="1"&&fields["sis_profile"]!="2")||fields["license"]!="MIT"||fields["runtime_api"]!="14"||uid.size()!=10||uid.find_first_not_of("0123456789")!=uid.npos ) {error="SIS manifest profile";return false;}
    uint64_t value=0;for(char c:uid)value=value*10+c-'0';
    if(value<0xe0000000||value>0xefffffff||std::to_string(value)!=uid) {error="SIS UID range";return false;}
    p.uid=uint32_t(value);p.sisProfile=fields["sis_profile"]=="2"?2:1;
    if(p.sisProfile==1&&(!p.permissions.empty()||!p.dependencies.empty())){error="profile 1 capabilities";return false;}
    if(p.sisProfile==2){
      auto decimal=[&](const std::string &key,unsigned &out,bool optional){auto v=fields[key];if(optional&&v.empty()){out=0;return true;}if(v.empty()||v.size()>4||v.find_first_not_of("0123456789")!=v.npos)return false;out=unsigned(std::stoul(v));return out>0&&out<=9999&&std::to_string(out)==v;};
      if(!decimal("schema",p.schema,false)||!decimal("migrate_from",p.migrateFrom,true)){error="data schema";return false;}
      p.migration=fields["migration"];
      if((p.migrateFrom && (p.migration!="migration.tsv"||!p.hashes.count(p.migration)||p.migrateFrom==p.schema))||(!p.migrateFrom&&!p.migration.empty())){error="migration contract";return false;}
    }

    unsigned parts=0;for(char c:p.version)parts+=c=='.';if(parts!=2) {error="SIS version";return false;}
  }
  if(!p.uid&&!p.dependencies.empty()){error="SIS dependency in legacy package";return false;}
  if(p.uid&&!p.permissions.empty()&&p.permissions.back()==','){error="invalid SIS permissions";return false;}
  std::set<std::string> uniquePermissions;
  TextLines permissions(p.permissions);
  while (nextLine(permissions, line, ',')) {
    if(p.uid&&(line.empty()||!uniquePermissions.insert(line).second)){error="duplicate SIS permission";return false;}
    if (!line.empty() && line != "documents" && line != "media" &&
        line != "camera") {
      error = "unsupported permission";
      return false;
    }
  }
  return true;
}
bool Installer::archive(const std::string &path, Package &p,
                        std::map<std::string, std::string> &files) {
  error.clear();
  files.clear();
  std::string magic;disk_.readRange(path,0,4,magic);
  if(path.size()>=5&&path.substr(path.size()-5)==".sisx") {error="SISX/signatures deferred";return false;}
  if((magic.size()==4&&number(magic,0,4)==0x10201a7a)||(path.size()>=4&&path.substr(path.size()-4)==".sis"))return sisArchive(path,p,files,nullptr,true);
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
  if(p.uid)return fail("SIS manifest requires a SIS container");
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
bool Installer::inspect(const std::string &path, Package &p,const std::atomic<bool> *cancel) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  error.clear();std::string magic;disk_.readRange(path,0,4,magic);
  if(path.size()>=5&&path.substr(path.size()-5)==".sisx") {error="SISX/signatures deferred";return false;}
  if((magic.size()==4&&number(magic,0,4)==0x10201a7a)||(path.size()>=4&&path.substr(path.size()-4)==".sis")) {std::map<std::string,std::string> ignored;return sisArchive(path,p,ignored,cancel,false);}
  std::map<std::string, std::string> files;
  return archive(path, p, files);
}
bool Installer::catalog(std::vector<Package> &out) {
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  out.clear();
  if(!recoverSis())return false;
  std::string registry;
  if (!disk_.read("/apps/registry.tsv", registry, 65536)) {
    if (disk_.lastError() == "not found")
      return true;
    error = disk_.lastError();
    return false;
  }
  TextLines lines(registry);
  std::string line;
  nextLine(lines, line);
  if (line != "T5APPS1") {
    error = "invalid app registry";
    return false;
  }
  std::set<std::string> ids;std::set<uint32_t> uids;
  while (nextLine(lines, line)) {
    TextLines fields(line);
    std::string id, version, hash, text;
    nextLine(fields, id, '\t');
    nextLine(fields, version, '\t');
    nextLine(fields, hash);
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
    if(p.uid && !uids.insert(p.uid).second) {error="duplicate installed SIS UID";return false;}
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
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  std::string magic;disk_.readRange(path,0,4,magic);
  if(magic.size()==4&&number(magic,0,4)==0x10201a7a)return installSis(path,cancel,expectedHash);
  std::string pendingId,pendingVersion;if(pendingSis(pendingId,pendingVersion)){error="Validate or cancel pending SIS first";return false;}
  Package p;
  std::map<std::string, std::string> files;
  if (!archive(path, p, files))
    return false;
  bool retainedSis=false;if(!disk_.pathExists("/meta/sis-data/"+p.id+".txt",retainedSis)){error=disk_.lastError();return false;}
  if(retainedSis){error="retained SIS data requires its original UID";return false;}
  if (!expectedHash.empty() && p.archiveHash != expectedHash) {
    error = "package changed after review";
    return false;
  }
  std::vector<Package> all;
  if (!catalog(all))
    return false;
  std::string dest = "/apps/" + p.id + "/" + p.version;
  for(const auto &a:all)if(a.id==p.id && a.uid) {error="SIS updates deferred";return false;}
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
  std::lock_guard<std::recursive_mutex> lock(installerMutex());
  std::vector<Package> all;
  if (!identifier(id) || !catalog(all))
    return false;
  std::string pendingId,pendingVersion;if(pendingSis(pendingId,pendingVersion)){error="Validate or cancel pending SIS first";return false;}
  for(const auto &a:all)if(a.id==id&&a.uid)return lifecycleRemove(id,eraseData);
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
