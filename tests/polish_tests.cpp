// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "http_body.hpp"
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;namespace fs=std::filesystem;
unsigned checks=0;void check(bool yes,const std::string &why){++checks;if(!yes){std::cerr<<"FAIL: "<<why<<'\n';exit(1);}}
int main(int argc,char **argv){
 check(argc==2,"SD argument");
 HttpBody body;body.length=10;body.lastProgress=100;
 check(body.accept(5,false,false,false,100)==5,"body progress");
 check(body.accept(-1,false,true,false,4000)==HttpBody::again,"temporary receive wait retries");
 check(body.accept(5,true,false,false,5000)==5 && body.accept(0,true,false,false,5000)==0,"declared-length body completes");
 body={};body.length=10;body.lastProgress=0;
 check(body.accept(5,false,false,false,100)==5 && body.accept(0,false,false,true,200)==-1,"truncated declared length fails even on FIN");
 body={};body.chunked=true;check(body.accept(5,false,false,false,100)==5 && body.accept(0,false,false,true,200)==-1,"missing terminal HTTP chunk rejected");
 check(body.accept(0,true,false,true,200)==0,"terminal chunk accepted");
 body={};body.chunked=true;body.length=1;check(body.accept(1024,false,false,false,100)==1024&&body.accept(0,false,false,true,200)==-1&&body.accept(0,true,false,true,300)==0,"chunk framing ignores Content-Length without accepting missing terminal chunk");
 body={};check(body.accept(8,false,false,false,100)==8 && body.accept(0,false,false,true,200)==0,"close-delimited HTTP response supported");
 body={};body.length=1;check(body.accept(-1,false,true,false,29999)==HttpBody::again && body.accept(-1,false,true,false,30000)==-1,"bounded body idle timeout");
 auto root=fs::temp_directory_path()/("tab5-polish-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);
 Storage disk(root.string());Runtime r(disk);r.entropy=[] {return std::string(32,'b');};r.setPowerWriter([](auto &){return true;});r.setOfflineWriter([](bool){return true;});
 r.memoryReader=[] {MemoryInfo m;m.available=true;m.internalTotal=524288;m.internalFree=262144;m.internalLargest=131072;m.internalMinimum=200000;m.psramTotal=33554432;m.psramFree=16777216;m.psramLargest=8388608;return m;};
 check(r.boot(),"boot API11");
 auto open=[&](const std::string &id){check(r.open(id),"open "+id+": "+r.error);};
 auto action=[&](const std::string &id,const std::string &value=""){r.beginOperation();check(r.dispatch(id,value),"action "+id+": "+r.error);};
 open("energy");check(r.view().title=="Ajustes","hub renamed");bool pin=false;for(auto &b:r.view().buttons)pin|=b.id=="security";check(pin,"PIN has direct visible hub entry");action("security");check(r.view().title=="Bloqueio por PIN" && r.view().controls[0].type=="pin","direct PIN form");action("back");check(r.view().title=="Ajustes","return to hub");
 action("screen");check(r.view().controls.back().type=="slider" && !r.view().footer.empty(),"brightness slider with bottom OK");action("apply","2\n37");check(r.brightness==37,"brightness persists");action("sound");check(r.view().controls[0].type=="slider","volume slider");action("apply","23");check(r.media().volume()==23,"volume commits");action("charge");check(r.view().layout=="options" && r.view().controls.empty() && r.view().buttons.size()==3,"charge is noneditable choice list");action("sleep");check(r.view().buttons.size()==3 && r.view().buttons[0].icon=="ui_sleep","sleep choices have duration icons");
 open("profiles");action("silent");check(r.view().layout=="options" && r.view().profile=="silent","profile list and top-bar snapshot");check(!r.view().statusFields.empty() && r.view().statusFields[0].value=="Silencioso","applied profile readonly");action("normal");
 open("network");check(!r.view().field && r.view().refreshMs==1000 && r.view().statusFields.size()==4,"Wi-Fi readonly fields auto refresh");for(auto &b:r.view().buttons)check(b.id!="refresh" && !b.icon.empty(),"no manual status refresh button; actions have icons");action("wifi");check(r.view().refreshMs==0 && r.view().field && !r.view().footer.empty(),"credential form is not refreshed and commits in footer");
 open("memory");check(r.view().refreshMs && !r.view().field && r.view().statusFields.size()==6,"live memory monitor");check(r.view().statusFields[0].percent==50 && r.view().statusFields[1].percent==50,"real provider heap/PSRAM percentages");
 open("notes");open("shell");action("switcher");check(r.view().layout=="switcher","compact task switcher layout");for(auto &b:r.view().buttons)check(!b.icon.empty() && !b.close.empty() && b.subtitle.find("Lua")!=std::string::npos,"task row has icon Lua RAM and close action");
 open("clock");check(r.view().layout=="clock" && r.view().optionAction=="adjust","clock side display and real settings action");open("contacts");for(auto &b:r.view().buttons)check(!b.icon.empty(),"contact actions have function icons");open("files");for(auto &b:r.view().buttons)check(!b.icon.empty(),"file actions have function icons");open("backup");check(!r.view().field && !r.view().statusFields.empty(),"backup dashboard readonly");open("installer");check(r.view().layout=="packages","installer uses compact package rows");
 {
  Network network(disk);bool linked=false;int polls=0,rssi=-68;
  network.backend.connect=[&](const std::string &,const std::string &,std::string &ip,const std::atomic<bool> &){ip="192.0.2.7";linked=true;return true;};
  network.backend.linked=[&]{return linked;};network.backend.disconnect=[&]{linked=false;};network.backend.signal=[&]{++polls;return rssi;};
  check(network.connect("fixture",""),"signal test connect");for(unsigned i=0;i<500 && network.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
  check(network.status().connected,"signal test connected");network.autoConnect(1000);check(polls==1 && network.status().signalValid && network.status().signal==-68,"RSSI snapshot collected outside status getter");
  network.status();network.status();network.autoConnect(2000);check(polls==1,"status reads and frequent ticks do not trigger radio RPC");
  network.autoConnect(6001,false);check(polls==1,"PIN screen can defer RSSI polling");rssi=-52;network.autoConnect(6001);check(polls==2 && network.status().signal==-52,"RSSI refresh every five seconds");
  linked=false;check(!network.status().connected && !network.status().signalValid,"disconnect invalidates cached signal");network.disconnect();
 }
 const auto oldCache=std::string("<rss><channel><title>antigo</title></channel></rss>");check(disk.write("/data/feeds/latest.xml",oldCache),"old feed cache fixture");
 const auto newFeed=std::string("<rss><channel><title>novo</title></channel></rss>");int fetches=0;
 r.network().backend.fetch=[&](const std::string &,const Network::Sink &sink,const std::atomic<bool> &,std::string &error) {
  ++fetches;check(sink("<rss>",5),"feed first chunk accepted in RAM");std::string existing;check(disk.read("/data/feeds/latest.xml",existing)&&existing==oldCache,"partial feed does not alter cache");check(!fs::exists(root/"data/feeds/latest.xml.part"),"feed reception does not touch a partial file on SD");
  if(fetches==1) {error="Resposta HTTP/TLS interrompida";return false;}
  return sink(newFeed.data()+5,newFeed.size()-5);
 };
 check(r.network().download("https://example.test/feed","/data/feeds/latest.xml",true),"feed request accepted");for(unsigned i=0;i<1000 && r.network().status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));std::string committed;
 check(fetches==2 && disk.read("/data/feeds/latest.xml",committed) && committed==newFeed,"bounded retry restarts from zero and commits complete feed only");
 auto beforeLedger=disk.used();check(disk.write("/data/notes/quota-ledger-test",std::string(13,'x')) && disk.used()==beforeLedger+13,"quota ledger adds a new file");check(disk.write("/data/notes/quota-ledger-test",std::string(23,'x'))&&disk.used()==beforeLedger+23,"quota ledger replaces old size");check(disk.remove("/data/notes/quota-ledger-test")&&disk.used()==beforeLedger,"quota ledger subtracts deletion");
 const std::string fullError="Falha HTTP/TLS: ESP_ERR_HTTP_CONNECT • heap=32256 • "+std::string(600,'x')+" • FIM DO DIAGNÓSTICO";
 r.network().backend.fetch=[&](const std::string &,const Network::Sink &,const std::atomic<bool> &,std::string &error){error=fullError;return false;};
 open("feeds");action("source:gizmodo");action("update");for(unsigned i=0;i<500 && r.network().status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));action("refresh");
 check(r.view().body.find("Baixando")==std::string::npos,"failed feed clears stale downloading label");bool details=false;for(auto &b:r.view().buttons)details|=b.id=="error_details";check(details,"failed feed offers complete diagnostics");action("error_details");check(r.view().readonly && r.view().layout=="reader" && r.view().text==fullError,"HTTP/TLS error fully visible in scrollable reader");
 fs::remove_all(root);std::cout<<checks<<" interaction/HTTP body-policy checks passed (host)\n";
}
