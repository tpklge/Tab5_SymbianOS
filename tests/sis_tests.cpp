// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <filesystem>
#include <iostream>
#include <cassert>
#include <unistd.h>
using namespace tab5;namespace fs=std::filesystem;
void check(bool ok,const std::string &message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';exit(1);}}
int main(int argc,char **argv){
 check(argc==3,"arguments");auto root=fs::temp_directory_path()/("tab5-sis-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);Storage disk(root.string());Runtime runtime(disk);check(runtime.boot(),runtime.error);fs::copy_file(argv[2],root/"home/Transfer/hello.sis");check(disk.reconcile(),"reconcile fixture");Installer installer(disk);Package p;const std::string path="/home/Transfer/hello.sis";
 check(installer.inspect(path,p),installer.error);check(p.uid==0xe0001001&&p.id=="sis_hello"&&p.hashes.count("main.lua"),"SIS metadata");auto digest=p.archiveHash;
 std::string original;check(disk.read(path,original),"read fixture");
 // Every truncation boundary, including inside each TLV/header/padding.
 for(size_t i=0;i<original.size();++i){check(disk.write(path,original.substr(0,i)),"truncate fixture");check(!installer.inspect(path,p),"truncated SIS accepted at "+std::to_string(i));}
 check(disk.write(path,original),"restore fixture");
 unsigned seed=0x12345678;for(unsigned n=0;n<300;++n){auto data=original;seed=1664525*seed+1013904223;size_t at=seed%data.size();data[at]^=char(1u<<(seed%8));check(disk.write(path,data),"mutation fixture");check(!installer.inspect(path,p),"corrupted SIS accepted at "+std::to_string(at));}
 check(disk.write(path,original),"restore fixture");std::atomic<bool> cancel{true};check(!installer.inspect(path,p,&cancel),"cancel inspection");check(!installer.install(path,&cancel,digest),"cancel installation");cancel=false;
 disk.setFreeSpaceProvider([]{return uint64_t(0);});check(!installer.install(path,nullptr,digest),"no physical space");disk.setFreeSpaceProvider({});
 check(!installer.install(path,nullptr,std::string(64,'a')),"review hash binding");
 std::vector<std::string> checkpoints;
 installer.transactionCheckpoint=[&](const std::string &s){checkpoints.push_back(s);return true;};check(installer.install(path,nullptr,digest),installer.error);installer.transactionCheckpoint={};
 check(runtime.boot()&&runtime.open("sis_hello"),"installed SIS launches in sandbox: "+runtime.error);runtime.open("shell");runtime.close("sis_hello");check(installer.remove("sis_hello",true),"remove fixture");
 // Replay interruption at each durable write/activation boundary and reboot.
 for(size_t fail=0;fail<checkpoints.size();++fail){size_t at=0;installer.transactionCheckpoint=[&](const std::string&){return at++!=fail;};check(!installer.install(path,nullptr,digest),"injected interruption");installer.transactionCheckpoint={};check(disk.mount(),"remount after interruption");check(installer.recoverSis(),installer.error);check(installer.recoverSis(),"idempotent recovery");std::vector<Package> all;check(installer.catalog(all),installer.error);bool active=false;for(auto &a:all)active|=a.id=="sis_hello";
  if(active){check(runtime.boot()&&runtime.open("sis_hello"),"committed app intact");runtime.open("shell");runtime.close("sis_hello");check(installer.remove("sis_hello",true),"remove committed fixture");}
 }
 // Removal and exhaustion during staging must leave a recoverable journal.
 installer.transactionCheckpoint=[&](const std::string &step){if(step=="write:main.lua")disk.pause();return true;};
 check(!installer.install(path,nullptr,digest),"card removal aborts transaction");installer.transactionCheckpoint={};check(disk.mount()&&installer.recoverSis(),installer.error);
 installer.transactionCheckpoint=[&](const std::string &step){if(step=="before:LICENSE.txt")disk.setFreeSpaceProvider([]{return uint64_t(0);});return true;};
 check(!installer.install(path,nullptr,digest),"space exhausted during staging");installer.transactionCheckpoint={};disk.setFreeSpaceProvider({});check(installer.recoverSis(),installer.error);
 installer.transactionCheckpoint=[&](const std::string &step){if(step=="write:main.lua")check(disk.write("/tmp/install/sis/main.lua","corrupted"),"corrupt staging fixture");return true;};
 check(!installer.install(path,nullptr,digest),"readback detects corruption");installer.transactionCheckpoint={};check(installer.recoverSis(),installer.error);
 check(disk.write("/meta/sis-install.txt","invalid"),"invalid journal fixture");check(!installer.recoverSis(),"malformed journal fails closed");std::string kept;check(disk.read("/meta/sis-install.txt",kept)&&kept=="invalid","malformed journal preserved");check(disk.remove("/meta/sis-install.txt"),"remove invalid test journal");
 const auto oldGeneration=disk.generation();check(disk.mount(),"generation test remount");bool touched=false;check(!disk.atGeneration(oldGeneration,[&]{touched=true;return true;})&&!touched,"stale generation cannot mutate new card");
 check(runtime.boot()&&runtime.open("installer")&&runtime.dispatch("inspect:"+path),"device review UI");check(runtime.view().title=="Olá SIS","review title");check(runtime.dispatch("install"),"reviewed install action");check(runtime.open("sis_hello"),"UI install visible");
 check(!installer.install(path,nullptr,digest),"updates deferred");
 std::string installedSource,manifest;check(disk.read("/apps/sis_hello/1.0.0/main.lua",installedSource)&&disk.read("/apps/sis_hello/1.0.0/manifest.txt",manifest),"installed source fixture");
 check(disk.write("/meta/sis-install.txt","T5SIS1\ncommitted\nsis_hello\n1.0.0\n"+sha256(manifest)+"\n"),"committed journal fixture");check(disk.write("/apps/sis_hello/1.0.0/main.lua","damaged"),"committed corruption fixture");check(!installer.recoverSis(),"committed payload corruption fails closed");check(disk.read("/meta/sis-install.txt",kept),"damaged commit journal preserved");check(disk.write("/apps/sis_hello/1.0.0/main.lua",installedSource)&&installer.recoverSis(),"repaired fixture recovers");
 runtime.shutdown();fs::remove_all(root);std::cout<<"SIS metadata, every truncation, mutations, cancellation, quota, review and "<<checkpoints.size()<<" interruption/recovery boundaries passed\n";
}
