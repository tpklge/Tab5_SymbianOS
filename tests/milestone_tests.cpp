// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;namespace fs=std::filesystem;
void wait(Network &n){for(unsigned i=0;i<1500&&n.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!n.status().busy);}
int main(int argc,char **argv){assert(argc==2);auto root=fs::temp_directory_path()/("tab5-milestones-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);Storage disk(root.string());Runtime r(disk);assert(r.boot());
 assert(r.open("contacts")&&r.dispatch("new")&&r.dispatch("save","Salvar\n555\nuser@example.test"));std::string contact;
 for(const auto &b:r.view().buttons)if(b.label=="Salvar")contact=b.id;assert(!contact.empty());
 assert(r.dispatch(contact)&&r.dispatch("star")&&r.dispatch("back")&&r.dispatch("favorites"));unsigned names=0;for(auto &b:r.view().buttons)if(b.id.rfind("edit:",0)==0){++names;assert(b.label.find("Salvar")!=b.label.npos);}assert(names==1);bool starIcon=false;for(const auto &b:r.view().buttons)if(b.id==contact){starIcon=b.icon=="ui_star";assert(b.label.find("★")==b.label.npos);}assert(starIcon);
 assert(r.open("energy"));bool languageEntry=false;for(const auto &b:r.view().buttons)languageEntry|=b.id=="language";assert(languageEntry);assert(r.dispatch("language")&&r.view().title=="Idioma");assert(r.dispatch("back")&&r.view().title=="Ajustes");
 assert(r.open("settings")&&r.dispatch("language")&&r.dispatch("apply","en"));assert(r.view().title=="Language"&&r.view().controls[0].keys=="pt\nen\nes");
 assert(r.open("contacts"));bool original=false,favorites=false;for(auto &b:r.view().buttons){original|=b.id==contact&&b.label.find("Salvar")!=b.label.npos;favorites|=b.label=="Favorites"||b.label=="All contacts";}assert(original&&favorites);
 assert(r.open("clock"));assert(r.view().title.find("Clock")!=std::string::npos);
 assert(r.open("settings")&&r.dispatch("language")&&r.dispatch("apply","es"));assert(r.view().controls[0].label=="Idioma de la interfaz");assert(r.open("energy"));bool spanish=false;for(auto &b:r.view().buttons)spanish|=b.label=="Carga";assert(spanish);assert(r.dispatch("language")&&r.view().title=="Idioma");assert(r.dispatch("back")&&r.view().title=="Ajustes");assert(r.dispatch("security")&&r.dispatch("back")&&r.view().title=="Ajustes");
 assert(r.open("settings")&&r.dispatch("alarm_tone")&&r.dispatch("apply","soft"));unsigned tone=99;r.setSignal([&](bool alarm){assert(alarm);tone=r.media().alertTone();});assert(r.dispatch("test_tone")&&tone==1);
 assert(r.dispatch("apply","double")&&r.dispatch("test_tone")&&tone==2);
 assert(r.open("bench")&&r.dispatch("sd_test"));bool reviewButton=false;for(const auto &b:r.view().footer)reviewButton|=b.id=="ota_review";assert(!reviewButton);std::string data;assert(disk.read("/data/bench/sd.tsv",data)&&data.find("pass")!=data.npos);assert(r.dispatch("start_fast")&&r.tick()&&r.tick());assert(disk.read("/data/bench/trace-1.tsv",data)&&data.find("host")!=data.npos);assert(r.dispatch("stop")&&r.tick());assert(disk.read("/data/bench/enabled",data)&&data=="0");

 auto &net=r.network();net.backend.connect=[](auto&,auto&,std::string &ip,auto&){ip="127.0.0.1";return true;};assert(net.connect("fixture",""));wait(net);
 unsigned failureMode=0;bool fail=false;const std::string payload(8192,'x');net.backend.fetch=[&](auto&,const Network::Sink &sink,const auto &cancel,std::string &error){if(!sink(payload.data(),4096))return false;if(fail){if(failureMode==2)net.cancel();error="Injected disconnect/cancel";return false;}return !cancel&&sink(payload.data()+4096,4096);};
 for(unsigned i=0;i<60;++i){failureMode=i%3;fail=failureMode!=0;assert(net.download("https://example.test/file","/home/Transfer/network.bin"));wait(net);if(!fail){assert(disk.read("/home/Transfer/network.bin",data)&&data==payload);assert(disk.remove("/home/Transfer/network.bin"));}else assert(!disk.read("/home/Transfer/network.bin",data));}
 fail=false;failureMode=0;disk.setFreeSpaceProvider([]{return uint64_t(0);});assert(net.download("https://example.test/file","/home/Transfer/quota.bin"));wait(net);assert(!disk.read("/home/Transfer/quota.bin",data));disk.setFreeSpaceProvider({});
 r.shutdown();{Runtime restarted(disk);assert(restarted.boot()&&restarted.open("settings")&&restarted.dispatch("language")&&restarted.view().controls[0].label=="Idioma de la interfaz");restarted.shutdown();}
 fs::remove_all(root);std::cout<<"Favorites, EN/ES persistence/data isolation, alarm tones, background measurement, no updater and 60 network fault cycles passed\n";
}
