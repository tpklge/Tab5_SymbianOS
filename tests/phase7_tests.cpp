// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "screen_power.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <unistd.h>
using namespace tab5;
namespace fs=std::filesystem;
unsigned checks=0;
void check(bool b,const char *s) { ++checks; if(!b) { std::cerr<<"FAIL: "<<s<<'\n';exit(1); } }
std::string read(const fs::path &p) { std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
int main(int argc,char **argv) {
  check(argc==2,"source path");
  auto root=fs::temp_directory_path()/("tab5-phase7-"+std::to_string(getpid()));
  fs::remove_all(root); fs::copy(argv[1],root,fs::copy_options::recursive);
  Storage disk(root.string()); Runtime runtime(disk);
  check(runtime.boot(),"initial essential self-test");
  auto manifest=read(root/"boot/manifest.txt");
  check(read(root/"boot/confirmed.txt")==manifest && !fs::exists(root/"boot/attempt.txt"),"confirmed boot clears trial marker");
  check(disk.write("/home/Documents/keep.txt","document"),"user document fixture");
  Releases releases(disk); std::string error, selected;
  check(!releases.stage("/update/missing",error),"only confined staging manifest accepted");
  auto future=manifest; auto at=future.find("release=1.0.0");
  check(at!=std::string::npos,"current release fixture"); future.replace(at,std::string("release=1.0.0").size(),"release=9.9.0");
  fs::copy(root/"system/releases/1.0.0",root/"system/releases/9.9.0",fs::copy_options::recursive);
  check(disk.reconcile() && disk.write("/update/manifest.txt",future),"prepare separate SD candidate");
  check(releases.stage("/update/manifest.txt",error),"verified release staged");
  check(releases.select(selected,error) && selected==future,"first trial selected");
  check(releases.select(selected,error) && selected==manifest,"interrupted boot rolls back confirmed manifest");
  auto shell=read(root/"system/releases/9.9.0/shell.lua");
  const std::string broken="function event( syntax error";
  check(disk.write("/system/releases/9.9.0/shell.lua",broken),"prepare script boot failure");
  future.replace(future.find(sha256(shell)),64,sha256(broken));
  check(disk.write("/update/manifest.txt",future) && releases.stage("/update/manifest.txt",error),"hash-valid script failure accepted for trial");
  check(runtime.boot() && read(root/"boot/confirmed.txt")==manifest,"failed essential script restores known-good same boot");
  check(read(root/"home/Documents/keep.txt")=="document","rollback preserves documents");
  check(disk.write("/system/releases/9.9.0/shell.lua",shell),"restore candidate script");
  future.replace(future.find(sha256(broken)),64,sha256(shell));
  check(disk.write("/update/manifest.txt",future) && releases.stage("/update/manifest.txt",error) && runtime.boot(),"valid SD candidate boots and confirms");
  check(read(root/"boot/previous.txt")==manifest,"previous release retained");
  check(releases.restore() && runtime.boot() && read(root/"boot/manifest.txt")==manifest,"manual rollback to previous release");
  check(screenBrightness(0,4000000000u)==100 && screenBrightness(1,44999)==100 &&
      screenBrightness(1,45000)==20 && screenBrightness(1,60000)==0,"off/dim/disabled boundaries");
  check(runtime.open("energy") && runtime.dispatch("timeout:5") && runtime.screenMinutes==5,"energy setting applies");
  check(runtime.dispatch("timeout:3") && runtime.screenMinutes==5,"unsupported timeout refused");
  check(runtime.boot() && runtime.screenMinutes==5,"screen preference persists");
  check(runtime.open("energy") && runtime.dispatch("screen_off") && runtime.screenOffRequested.exchange(false),"manual screen-off request");
  std::string operation;
  runtime.systemWriter=[&](const std::string &op,std::string &message) { operation=op;message="mock";return true; };
  check(runtime.open("energy") && runtime.dispatch("sleep:1") && operation.empty(),"deep sleep requires explicit UI confirmation");
  check(runtime.dispatch("cancel") && operation.empty(),"sleep confirmation cancel");
  check(runtime.dispatch("sleep:1") && runtime.dispatch("confirm") && operation=="sleep:1","confirmed sleep reaches backend");
  auto &net=runtime.network();
  const std::string id="0123456789abcdef",path="/home/Transfer/thirty.bin";
  const size_t total=30u*1024u*1024u;
  std::string block(65536,'a');
  auto reader=[&](char *p,size_t n) { n=std::min(n,size_t(1301));memcpy(p,block.data(),n);return int(n); };
  check(!net.receivePart(id,path,0,100u*1024u*1024u,block.size(),reader,error),"100 MiB refused before read");
  check(net.receivePart(id,path,0,total,block.size(),reader,error),"first block acknowledged");
  check(net.receivePart(id,path,0,total,block.size(),reader,error),"lost response retry idempotent");
  auto wrong=[&](char *p,size_t n) { memset(p,'b',n);return int(n); };
  check(!net.receivePart(id,path,0,total,block.size(),wrong,error),"altered duplicate rejected");
  size_t interrupted=0;
  check(!net.receivePart(id,path,65536,total,65536,[&](char *p,size_t n) {
    if(interrupted++) return -1;memset(p,'a',1000);return 1000;
  },error),"partial HTTP block interrupted");
  check(fs::file_size(root/"tmp/web-upload.part")==65536,"unacknowledged bytes do not reach SD");
  for(size_t offset=65536;offset<total;offset+=65536)
    check(net.receivePart(id,path,offset,total,65536,reader,error),"30 MiB acknowledged stream");
  check(net.receivePart(id,path,total-65536,total,65536,reader,error),"last response lost retry safe");
  check(fs::file_size(root/"home/Transfer/thirty.bin")==total && !fs::exists(root/"tmp/web-upload.part"),"30 MiB committed exact size without partial");
  check(read(root/"home/Transfer/thirty.bin")==std::string(total,'a'),"30 MiB byte-for-byte integrity");
  for(unsigned i=0;i<100;++i) {
    check(runtime.open("notes") && runtime.dispatch("save","stress "+std::to_string(i)) &&
      runtime.open("calculator") && runtime.dispatch("calculate","2+3*4") && runtime.open("shell"),"100 navigation/save/checkpoint cycles");
  }
  check(runtime.checkpointApps(),"checkpoint resident apps before suspend/reboot");
  fs::remove_all(root);
  std::cout<<checks<<" phase-7/recovery/resumable upload checks passed (host; hardware simulated)\n";
}
