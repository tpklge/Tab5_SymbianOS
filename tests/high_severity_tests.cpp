// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;
namespace fs = std::filesystem;
static void put(const fs::path &p, const std::string &s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}
static std::string get(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
static void storage(const fs::path &root) {
  put(root/"filler",std::string(10000,'z'));
  put(root/"data/folder/keep.txt","precious");
  fs::create_directories(root/"data/empty");
  Storage disk(root.string(),1000000,0);
  assert(disk.mount());
  const auto used=disk.used();
  assert(!disk.write("/data/folder","bad") && disk.online());
  assert(!disk.write("/data/empty","") && disk.online());
  assert(get(root/"data/folder/keep.txt")=="precious");
  assert(!fs::exists(root/"data/folder.t5-old") && disk.used()==used);
  assert(disk.mount());
  put(root/"src/sub/file","keep");
  fs::create_directories(root/"src/empty");
  std::string deep;
  for(unsigned i=0;i<15;++i)deep+="/a";
  fs::create_directories(root/deep.substr(1));
  assert(disk.reconcile());
  assert(!disk.move("/src",deep+"/moved"));
  assert(get(root/"src/sub/file")=="keep" && disk.mount());
  assert(!fs::exists(root/(deep.substr(1)+"/moved")));
  std::string longParent="/"+std::string(100,'a')+"/"+std::string(100,'b');
  fs::create_directories(root/longParent.substr(1));
  put(root/"src/sub"/std::string(40,'c'),"long descendant");
  assert(disk.reconcile());
  assert(!disk.move("/src",longParent+"/moved"));
  assert(disk.mount() && get(root/"src/sub/file")=="keep");
  assert(disk.move("/src","/moved") && disk.mount());
  assert(get(root/"moved/sub/file")=="keep" && fs::is_directory(root/"moved/empty"));
}
static void ipc(const fs::path &root,const fs::path &source) {
  fs::copy(source,root,fs::copy_options::recursive);
  const std::string code=R"(local n=0
function event(a,v)
 if a=='attack' then
  assert(not sys.send('notes','save','OVERWRITTEN'))
  assert(not sys.send('services','bench:start','5'))
  assert(not sys.send('services','ack','alarm:victim'))
  assert(not sys.send('services','mesh','uart'))
  assert(sys.send('probe','self',''))
 elseif a=='self' then n=n+1 end
 return {title='Probe',body=tostring(n)}
end)";
  const std::string manifest="format=1\nid=probe\nversion=1.0\ntitle=Probe\nauthor=Audit\nkind=app\napi_min=4\napi_max=4\norientation=landscape\nmemory=1048576\ninstructions=100000\npermissions=\nfile=main.lua|"+sha256(code)+"\n";
  put(root/"apps/probe/1.0/manifest.txt",manifest);
  put(root/"apps/probe/1.0/main.lua",code);
  put(root/"apps/registry.tsv","T5APPS1\nprobe\t1.0\t"+sha256(manifest)+"\n");
  Storage disk(root.string());Runtime r(disk);
  assert(r.boot() && r.open("notes") && r.dispatch("save","ORIGINAL"));
  assert(r.open("probe") && r.dispatch("attack") && r.tick());
  assert(get(root/"data/notes/document.txt")=="ORIGINAL");
  assert(r.dispatch("refresh") && r.view().body=="1");
}
static void attachments(const fs::path &root,const fs::path &source) {
  fs::copy(source,root,fs::copy_options::recursive);
  Storage disk(root.string());assert(disk.mount());Mail mail(disk);
  MailAccount account;account.address="audit@example.test";account.inUser=account.address;
  account.inHost="imap.example.test";account.inPassword="pass";account.outHost="smtp.example.test";
  std::string error;assert(mail.configure(account,error));
  MailMessage draft;draft.to="friend@example.test";draft.body="OLD";
  assert(mail.draft(draft,false,error));
  assert(disk.write("/home/payload",std::string(1024*1024,'x')));
  // Hold the copy before its first write, so queueing is checked while an
  // attachment is certainly in progress. Slow subsequent writes permit edits.
  std::atomic<bool> entered{false},release{false};
  disk.setFreeSpaceProvider([&]{
    entered=true;
    while(!release)std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return UINT64_MAX/2;
  });
  assert(mail.attach(draft.id,"/home/payload",error));
  while(!entered)std::this_thread::yield();
  assert(!mail.draft(draft,true,error));
  release=true;
  draft.subject="NEW SUBJECT";draft.body="NEW BODY";
  assert(mail.draft(draft,false,error));
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
  while(mail.status().busy && std::chrono::steady_clock::now()<deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(!mail.status().busy);
  disk.setFreeSpaceProvider({});
  MailMessage result;
  assert(mail.read(draft.id,result) && result.body=="NEW BODY" &&
         result.subject=="NEW SUBJECT" && result.attachments.size()==1);
  std::string bytes;
  assert(disk.read(result.attachments[0].path,bytes) && bytes==std::string(1024*1024,'x'));
  assert(mail.draft(draft,true,error));
}
static auto block(const char *value) {
  return [value](char *out,size_t n){n=std::min(n,size_t(4));memcpy(out,value,n);return int(n);};
}
static void uploads(const fs::path &root) {
  fs::create_directories(root/"home/Transfer");fs::create_directories(root/"tmp");
  Storage disk(root.string(),1000000,0);assert(disk.mount());Runtime r(disk);
  auto &net=r.network();std::string error,data;
  const std::string id="0123456789abcdef",path="/home/Transfer/file.bin";
  assert(net.receivePart(id,path,0,8,4,block("AAAA"),error));
  r.shutdown();disk.pause();fs::rename(root,root.string()+"-old");
  fs::create_directories(root/"home/Transfer");fs::create_directories(root/"tmp");
  assert(disk.mount());
  assert(!net.receivePart(id,path,4,8,4,block("BBBB"),error));
  assert(!fs::exists(root/"home/Transfer/file.bin"));
  assert(net.receivePart(id,path,0,8,4,block("AAAA"),error));
  assert(disk.write("/tmp/web-upload.part","A"));
  assert(!net.receivePart(id,path,4,8,4,block("BBBB"),error));
  assert(!fs::exists(root/"home/Transfer/file.bin"));
  assert(net.receivePart(id,path,0,8,4,block("AAAA"),error));
  assert(net.receivePart(id,path,4,8,4,block("BBBB"),error));
  assert(net.receivePart(id,path,4,8,4,block("BBBB"),error));
  assert(disk.read(path,data) && data=="AAAABBBB");
  assert(!net.removeShared(path,"0000000000000000",error));
  assert(net.removeShared(path,id,error)); // Completed session never deletes its destination.
  assert(disk.read(path,data)&&data=="AAAABBBB");
  assert(net.removeShared(path,"",error));
  assert(!net.removeShared("/config/private","",error));
  assert(!net.removeShared("/home/Transfer","",error));
  assert(net.receivePart(id,path,0,8,4,block("AAAA"),error));
  assert(net.removeShared(path,id,error));
  assert(!fs::exists(root/"tmp/web-upload.part"));
  const std::string next="abcdef0123456789";
  assert(net.receivePart(next,path,0,8,4,block("AAAA"),error));
  assert(net.receivePart(next,path,4,8,4,block("BBBB"),error));
  assert(net.removeShared(path,"",error));net.disconnect();
  assert(net.receivePart(id,path,0,8,4,block("AAAA"),error));
  assert(disk.mount()); // Also invalidate a session on remount without shutdown.
  assert(!net.receivePart(id,path,4,8,4,block("BBBB"),error));
  assert(!fs::exists(root/"home/Transfer/file.bin"));
}
static void largeOffsets(const fs::path &root) {
  fs::create_directories(root);const uint64_t offset=uint64_t(3)*1024*1024*1024;
  {std::ofstream file(root/"large.bin",std::ios::binary);file.seekp(offset);file<<"ABC";assert(file.good());}
  Storage disk(root.string(),4000000000ULL,0);disk.setFreeSpaceProvider([]{return UINT64_MAX/2;});assert(disk.mount());
  uint64_t size=0;std::string value;
  assert(disk.fileSize("/large.bin",size)&&size==offset+3&&disk.used()==size);
  assert(disk.readRange("/large.bin",offset,3,value)&&value=="ABC");
  assert(disk.patch("/large.bin",offset+1,"Z")&&disk.readRange("/large.bin",offset,3,value)&&value=="AZC");
  assert(!disk.patch("/large.bin",offset+3,"X"));
  assert(disk.remove("/large.bin")&&disk.used()==0);
}
int main(int argc,char **argv) {
  assert(argc==2);
  auto base=fs::temp_directory_path()/("tab5-high-severity-"+std::to_string(getpid()));
  fs::remove_all(base);fs::create_directories(base);
  largeOffsets(base/"large");
  storage(base/"storage");ipc(base/"ipc",argv[1]);
  attachments(base/"mail",argv[1]);uploads(base/"card");
  fs::remove_all(base);
  std::cout<<"High severity regressions: directory integrity, IPC isolation, concurrent drafts and card/upload lifecycle passed\n";
}
