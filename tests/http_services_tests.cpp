// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "http_fetch.hpp"
#include "http_body.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
using namespace tab5;
struct TimedStream:HttpStream {
 mutable uint64_t clock=0;unsigned reads=0;bool closed=false,progress=false;
 bool open(const std::string &,int &status,std::string &,std::string &)override{status=200;return true;}
 uint64_t nowMs()const override{return clock;}
 int read(char *out,size_t)override{++reads;clock+=5000;if(progress){out[0]='x';return 1;}return HttpBody::again;}
 void close()override{closed=true;}
};
void wait(Network &n){for(unsigned i=0;i<3000&&n.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!n.status().busy);}
int main(int argc,char **argv){
 assert(argc==2);namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("tab5-http-services-"+std::to_string(getpid()));fs::copy(argv[1],root,fs::copy_options::recursive);
 Storage disk(root.string());Runtime runtime(disk);assert(runtime.boot());
 // Invalid service records must remain intact and not block feed refresh.
 const auto path="/data/services/delivered.tsv";std::string original;bool exists=disk.read(path,original);
 for(const auto &bad:{std::string("damaged\n"),std::string("T5DATA1\nid-without-due\n"),std::string("T5DATA1\nid\t1e999\n")}){
  assert(disk.write(path,bad));assert(runtime.tick());std::string kept;assert(disk.read(path,kept)&&kept==bad);
  bool warning=false;for(const auto &n:runtime.notices())warning|=n.id=="services:storage";assert(warning);
  assert(runtime.open("feeds")&&runtime.dispatch("refresh"));
 }
 assert(disk.write("/config/public/profile","offline"));
 bool applied=false;runtime.setOfflineWriter([&](bool off){applied=off;return true;});
 assert(runtime.boot()&&applied); // Offline profile is applied despite a damaged notification ledger.
 if(exists)assert(disk.write(path,original));else assert(disk.remove(path));assert(runtime.tick());
 for(const auto &n:runtime.notices())assert(n.id!="services:storage");
 assert(disk.write("/data/organizer/alarms.tsv","T5DATA1\ninvalid-repeat\tAlarm\t1800000000\t5\t1\n"));assert(runtime.tick()&&runtime.ready());std::string badSchedule;assert(disk.read("/data/organizer/alarms.tsv",badSchedule)&&badSchedule.find("invalid-repeat")!=badSchedule.npos);assert(disk.write("/data/organizer/alarms.tsv","T5DATA1\n"));
 // Capture the read's own diagnostic while other workers change global state.
 std::atomic<bool> stop{false};std::thread noisy([&]{while(!stop)disk.restoreDiagnosticError("unrelated network log error");});
 for(unsigned i=0;i<100;++i){std::string out,why;assert(!disk.readWithError("/data/services/absent.tsv",out,why)&&why=="not found");}
 stop=true;noisy.join();
 assert(disk.write("/data/services/probe","data"));
 struct rlimit limits{},blocked{};assert(getrlimit(RLIMIT_NOFILE,&limits)==0);blocked=limits;blocked.rlim_cur=0;
 assert(setrlimit(RLIMIT_NOFILE,&blocked)==0);std::string value,why;bool ok=disk.readWithError("/data/services/probe",value,why);bool online=disk.online();
 assert(setrlimit(RLIMIT_NOFILE,&limits)==0);assert(!ok&&online&&why!="not found");
 // Partial page retry starts over; progress reflects only this attempt.
 auto &net=runtime.network();unsigned attempts=0;
 net.backend.fetchDocument=[&](const auto &,const auto &sink,const auto &,std::string &error,std::string &url){++attempts;url="https://example.test/final";if(attempts==1){assert(sink("discard",7));error="Resposta HTTP/TLS interrompida";return false;}assert(sink("<h1>Complete</h1>",17));return true;};
 net.backend.browserFetch=[&](const auto &url,const auto &request,const auto &sink,const auto &cancel,std::string &error,std::string &resolved){assert(request.legacyTls==(attempts>0));return net.backend.fetchDocument(url,sink,cancel,error,resolved);};
 assert(net.browse("https://example.test/retry"));wait(net);assert(attempts==2&&net.status().bytes==17);net.backend.browserFetch={};
 assert(disk.read(Network::pageCachePath("https://example.test/retry"),value)&&value.find("discard")==value.npos&&value.find("Complete")!=value.npos);
 std::atomic<bool> entered{false},release{false};
 net.backend.fetchDocument=[&](const auto &,const auto &sink,const auto &,std::string &,std::string &url){entered=true;while(!release)std::this_thread::sleep_for(std::chrono::milliseconds(1));url="https://example.test/final";return sink("<p>done</p>",11);};
 assert(net.browse("https://example.test/slow"));while(!entered)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!net.browse("https://example.test/rejected"));release=true;wait(net);assert(net.pageStatus("https://example.test/slow").ok&&!net.pageStatus("https://example.test/slow").busy);
 TimedStream stalled;std::atomic<bool> cancel{false};std::string error;
 assert(!fetchHttp(stalled,"https://example.test",[](auto,size_t){return true;},cancel,error));assert(stalled.closed&&stalled.reads==6&&error.find("30 segundos")!=error.npos);
 TimedStream trickle;trickle.progress=true;
 assert(!fetchHttp(trickle,"https://example.test",[](auto,size_t){return true;},cancel,error));assert(trickle.closed&&error.find("90 segundos")!=error.npos);
 runtime.shutdown();fs::remove_all(root);
 std::cout<<"Service data preserved, recoverable errors, atomic read reasons, descriptor pressure, full retry and HTTP deadlines passed\n";
}
