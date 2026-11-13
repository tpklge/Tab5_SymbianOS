// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;
namespace fs=std::filesystem;
unsigned count=0;
void check(bool ok,const std::string &why){++count;if(!ok){std::cerr<<"FAIL: "<<why<<'\n';exit(1);}}
std::string text(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
 check(argc==3,"SD and fixture arguments");auto root=fs::temp_directory_path()/("tab5-experience-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);
 Storage disk(root.string());Runtime r(disk);r.entropy=[] {return std::string("0123456789abcdef0123456789abcdef");};
 auto open=[&](const std::string &id){r.beginOperation();check(r.open(id),"open "+id+": "+r.error);};
 auto action=[&](std::string key,const std::string &value=""){r.beginOperation();check(r.dispatch(key,value),"action "+key+": "+r.error);};
 check(r.boot(),"boot API10");check(r.view().wallpaper && r.view().wallpaper->pixels.size()==320*180*2,"SD wallpaper decoded with bounded size");
 check(disk.write("/config/public/theme","legacy"),"legacy palette preference");open("settings");action("appearance");
 for(auto id:{"solar","prisma","aurora"}){action("apply",std::string(id)+"\n25\n5");check(r.brightness==25 && r.screenMinutes==5 && text(root/"config/public/theme").empty(),"each theme applies with brightness and timeout");open("shell");action("home");check(bool(r.view().wallpaper),"theme wallpaper belongs to Home");open("settings");action("appearance");}
 action("apply","aurora\n100\n2");
 check(disk.write("/data/organizer/events.tsv","T5DATA1\nhome\tCompromisso na Home\t"+std::to_string(r.now()+3600)+"\tDescrição\tcompromisso\t0\n"),"upcoming appointment");
 open("shell");action("home");check(r.view().layout=="home" && r.view().text.find("Compromisso na Home")!=std::string::npos,"Home shows upcoming appointments");
 action("menu");check(r.view().buttons.size()==12 && r.view().columns==4,"four-column twelve app menu");action("menu_mode");check(r.view().layout=="list" && r.view().columns==2 && r.view().buttons.size()==8,"two-column eight app list");action("organize");action("menu_save","music\npersonal\n1\n1\nlist\nalpha");action("folder:personal");
 bool music=false;for(auto &b:r.view().buttons)music|=b.id=="open:music";check(music,"move app into persistent folder");
 check(r.boot(),"boot after menu changes");action("menu");check(r.view().layout=="list","menu preference survives reboot");
 open("agenda");action("new");check(r.view().layout=="form" && r.view().controls.size()==5,"Brazilian agenda form");action("save","Agenda brasileira\n15/01/2027\n09:45\nDescrição\ntarefa");check(text(root/"data/organizer/events.tsv").find("Agenda brasileira")!=std::string::npos,"save Brazilian date/time");action("month");check(r.view().layout=="calendar" && r.view().buttons.size()>=28,"interactive monthly calendar");action("week");check(r.view().buttons.size()==7,"weekly agenda");
 for(unsigned i=0;i<80;++i)check(disk.write("/home/Music/track"+std::to_string(i)+".mp3","fixture"),"library fixture");
 open("music");check(r.view().body.find("80 faixas")!=std::string::npos,"library exceeds previous 64 file ceiling");action("playlists");action("new_list");action("save_list","Viagem");action("add_tracks");action("pick:/home/Music/track0.mp3");action("pick:/home/Music/track1.mp3");action("back");action("select:/home/Music/track1.mp3");action("move_up");
 auto lists=text(root/"data/music/playlists.tsv");check(lists.find("Viagem")!=std::string::npos,"named playlist saved");action("options");action("save_options","all\n1");check(text(root/"config/public/music_repeat")=="all" && text(root/"config/public/music_shuffle")=="1","repeat/shuffle persist");
 auto photo=fs::path(argv[2])/"picture.png";if(!fs::exists(photo)){for(auto &entry:fs::directory_iterator(argv[2]))if(entry.path().extension()==".png"){photo=entry.path();break;}}
 check(fs::exists(photo),"PNG fixture");check(disk.write("/home/Pictures/a.png",text(photo)) && disk.write("/home/Pictures/b.png",text(photo)),"gallery fixtures");
 open("gallery");action("view:/home/Pictures/a.png");action("zoom_in");check(r.view().result=="zoom2" && r.view().picture,"photo 2x zoom");action("zoom_in");check(r.view().result=="zoom4","photo 4x zoom");action("photo_next");check(r.view().body.find("b.png")!=std::string::npos,"next photo");action("fit");check(r.view().result.empty(),"fit photo");
 open("feeds");action("configure");action("save_source","Teste offline\nhttps://example.test/rss");check(r.view().buttons.size()==2,"saved subscriptions");auto source=r.view().buttons.back().id;action(source);
 r.network().backend.fetch=[](auto &,const Network::Sink &sink,const auto &,std::string &){std::string xml="<rss><channel><item><title>Notícia &amp; teste</title><link>https://example.test/article</link><description><![CDATA[<p>Conteúdo offline</p>]]></description></item></channel></rss>";return sink(xml.data(),xml.size());};
 action("update");for(unsigned i=0;i<1000 && r.network().status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));action("read");check(!r.view().buttons.empty() && r.view().buttons[0].label=="Notícia & teste","feed article list strips markup");action("article:1");check(r.view().layout=="web_document" && bool(r.view().picture) && r.view().text.find("Conteúdo offline")!=std::string::npos,"article reader");action("save_article");action("saved");check(!r.view().buttons.empty(),"offline article persisted");action(r.view().buttons[0].id);check(r.view().text.find("Conteúdo offline")!=std::string::npos,"read saved article without network");
 open("settings");action("security");action("apply","\n1234\n1234");check(r.lock.enabled() && text(root/"config/security/pin").size()==104 && text(root/"config/security/pin").rfind("T5PIN1\n",0)==0,"salted PIN verifier, no plaintext PIN");action("lock");check(!r.open("notes"),"locked runtime blocks direct app opening");open("lock");for(char d:std::string("1234"))action(std::string("digit:")+d);action("unlock");check(!r.lock.locked() && r.current()!="lock","correct PIN unlocks previous app");
 check(r.boot() && r.current()=="lock","PIN required again after reboot");for(unsigned attempt=0;attempt<5;++attempt){for(unsigned d=0;d<4;++d)action("digit:0");action("unlock");}check(r.lock.wait()>0 && r.lock.locked(),"rate limit after five failed attempts");check(r.boot() && r.lock.wait()>0,"reboot cannot bypass cooldown");
 fs::remove_all(root);std::cout<<count<<" experience/theme/PIN/agenda/playlist/gallery/feed checks passed (host)\n";
}
