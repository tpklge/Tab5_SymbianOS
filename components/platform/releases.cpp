// SPDX-License-Identifier: MIT
#include "releases.hpp"
#include <map>
#include "text_lines.hpp"
namespace tab5 {
bool Releases::validate(const std::string &manifest, std::string &error) {
  auto fail = [&](const char *why) { error = why; return false; };
  std::map<std::string, std::string> fields, files;
  TextLines in(manifest); std::string line;
  while (nextLine(in,line)) {
    if (line.empty() || line[0]=='#') continue;
    auto eq=line.find('='); if(eq==std::string::npos) return fail("Manifesto inválido");
    auto key=line.substr(0,eq), value=line.substr(eq+1);
    if(key=="file") {
      auto bar=value.find('|'); if(bar==std::string::npos) return fail("Hash inválido");
      auto name=value.substr(0,bar), hash=value.substr(bar+1);
      if(name.empty() || name.find('/')!=std::string::npos || !files.emplace(name,hash).second)
        return fail("Arquivo repetido ou caminho inválido");
    } else if(!fields.emplace(key,value).second) return fail("Campo repetido");
  }
  auto api=[](const std::string &v) { return v=="14" ? 14 : v=="13" ? 13 : v=="12" ? 12 : v=="11" ? 11 : v=="10" ? 10 : v.size()==1 && v[0]>='1' && v[0]<='9' ? v[0]-'0' : 0; };
  auto rel=fields["release"];
  if(fields["format"]!="1" || fields["schema"]!="1" ||
      !api(fields["api_min"]) || !api(fields["api_max"]) || api(fields["api_min"])>api(fields["api_max"]) ||
      rel.empty() || rel=="." || rel==".." || rel.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyz.-")!=std::string::npos)
    return fail("Release incompatível");
  for(const auto &entry:files) {
    std::string data;
    if(!disk_.read("/system/releases/"+rel+"/"+entry.first,data,entry.first=="icons.bin"?3145728:1048576) || sha256(data)!=entry.second)
      return fail("Integridade da release inválida");
  }
  for(auto name:{"theme.ttf","keyboard.map","onscreen.keys","chrome.lua"})
    if(!files.count(name)) return fail("Recurso essencial ausente");
  for(auto name:{"app.shell","app.notes","app.calculator"})
    if(!files.count(fields[name])) return fail("Aplicativo essencial ausente");
  error.clear(); return true;
}
bool Releases::restore() {
  std::string good;
  if(!disk_.read("/boot/confirmed.txt",good,32768)) return false;
  std::string active;
  disk_.read("/boot/manifest.txt",active,32768);
  // For a manual rollback from the confirmed release use the previous snapshot.
  if(active==good) {
    std::string previous;
    if(disk_.read("/boot/previous.txt",previous,32768)) good=previous;
  }
  std::string error;
  if(!validate(good,error) || !disk_.write("/boot/manifest.txt",good)) return false;
  disk_.remove("/boot/pending.txt"); disk_.remove("/boot/attempt.txt");
  return true;
}
bool Releases::select(std::string &manifest, std::string &error) {
  std::string attempt, pending, confirmed;
  disk_.read("/boot/confirmed.txt",confirmed,32768);
  if(disk_.read("/boot/attempt.txt",attempt,128) && !confirmed.empty()) {
    // A previous boot did not finish its essential self-test.
    if(!validate(confirmed,error) || !disk_.write("/boot/manifest.txt",confirmed)) return false;
    disk_.remove("/boot/pending.txt"); disk_.remove("/boot/attempt.txt");
    disk_.write("/logs/recovery.txt","Boot incompleto: release confirmada restaurada.\n");
  }
  if(disk_.read("/boot/pending.txt",pending,32768)) {
    if(!validate(pending,error) || !disk_.write("/boot/manifest.txt",pending)) {
      disk_.remove("/boot/pending.txt");
      if(confirmed.empty() || !disk_.write("/boot/manifest.txt",confirmed)) return false;
    }
  }
  if(!disk_.read("/boot/manifest.txt",manifest,32768)) {
    error="Manifesto de boot ausente"; return false;
  }
  return disk_.write("/boot/attempt.txt",sha256(manifest));
}
bool Releases::confirm(const std::string &manifest) {
  std::string good;
  if(disk_.read("/boot/confirmed.txt",good,32768) && good!=manifest &&
     !disk_.write("/boot/previous.txt",good)) return false;
  if(!disk_.write("/boot/confirmed.txt",manifest)) return false;
  disk_.remove("/boot/attempt.txt"); disk_.remove("/boot/pending.txt"); return true;
}
bool Releases::stage(const std::string &path, std::string &error) {
  std::string manifest;
  if(path!="/update/manifest.txt" || !disk_.read(path,manifest,32768) || !validate(manifest,error)) {
    if(error.empty()) error="Prepare /update/manifest.txt e os arquivos da release no SD";
    return false;
  }
  if(!disk_.canFit(manifest.size()*3) || !disk_.write("/boot/pending.txt",manifest)) {
    error="Espaço insuficiente para ativação recuperável"; return false;
  }
  return true;
}
}
