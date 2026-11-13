// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "text_lines.hpp"
#include <sstream>
#include <filesystem>
#include <iostream>
#include <deque>
#include <map>
#include <fstream>
#include <algorithm>
#include <unistd.h>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
using namespace tab5;namespace fs=std::filesystem;
void check(bool ok,const std::string &why) {if(!ok) {std::cerr<<"FAIL: "<<why<<'\n';exit(1);}}
std::string var(unsigned n) {std::string b;do {unsigned v=n&127;n>>=7;b+=char(v|(n?128:0));} while(n);return b;}
std::string num(unsigned tag,unsigned n) {return var(tag*8)+var(n);}
std::string bytes(unsigned tag,const std::string &s) {return var(tag*8+2)+var(s.size())+s;}
std::string fixed(unsigned tag,unsigned n) {std::string b=var(tag*8+5);for(int i=0;i<4;++i)b+=char(n>>(i*8));return b;}
struct Wire {unsigned n=0;std::string s;};
std::map<unsigned,Wire> decode(const std::string &s) {
 size_t at=0;auto varint=[&](){unsigned long long n=0;for(unsigned shift=0;shift<64;shift+=7){check(at<s.size(),"fixture protobuf bounds");unsigned b=uint8_t(s[at++]);n|=uint64_t(b&127)<<shift;if(!(b&128))return n;}check(false,"fixture varint bounds");return 0ULL;};
 std::map<unsigned,Wire> out;
 while(at<s.size()) {auto key=varint();Wire f;switch(key&7) {case 0:f.n=unsigned(varint());break;case 2:{auto n=varint();check(n<=s.size()-at,"fixture field bounds");f.s=s.substr(at,n);at+=n;break;}case 5:check(at+4<=s.size(),"fixture fixed bounds");for(unsigned i=0;i<4;++i)f.n|=unsigned(uint8_t(s[at++]))<<(8*i);break;default:check(false,"fixture wire type");}out[unsigned(key>>3)]=f;}return out;
}
int main(int argc,char **argv) {
 check(argc==2,"SD argument");auto root=fs::temp_directory_path()/("tab5-mesh-"+std::to_string(getpid()));fs::copy(argv[1],root,fs::copy_options::recursive);
 Storage disk(root.string());Runtime r(disk);r.setClock(1794174000);std::string state="Desconectado";std::deque<std::string> received;std::vector<std::string> sent;
 r.radioCommand=[&](const std::string &op,const std::string &arg,std::string &out) {
  if(op=="state") out=state;
  else if(op=="link")out=state=="Conectado UART"?"uart":"";
  else if(op=="connect_uart") state="Conectado UART";
  else if(op=="disconnect")state="Desconectado";
  else if(op=="send")sent.push_back(arg);
  else if(op=="read" && !received.empty()){out=received.front();received.pop_front();}
  return true;
 };
 auto open=[&](const char *app){r.beginOperation();check(r.open(app),std::string("open ")+app+": "+r.error);};
 auto action=[&](const char *a,const std::string &v=""){r.beginOperation();check(r.dispatch(a,v),std::string(a)+": "+r.error);};
 // Two cooperative slices allow deferred database persistence to reach disk.
 auto tick=[&] {for(unsigned slice=0;slice<2;++slice){r.beginOperation();check(r.tick(),"services tick: "+r.error);}};
 std::vector<unsigned> progress;r.bootProgress=[&](unsigned n){check(n>0 && n<=100 && (progress.empty() || n>=progress.back()),"real boot integrity progress is monotonic");progress.push_back(n);};
 check(r.boot(),"API13 boot: "+r.error);check(!progress.empty() && progress.back()==100,"boot reports completed integrity validation");r.bootProgress={};
 open("shell");action("home_settings");check(r.view().controls.size()==3,"three dedicated Home shortcuts");action("home_save","mesh_rak\nmesh_ble\nnotes");check(r.view().buttons[0].id=="open:mesh_rak","custom Home shortcut");open("notes");open("shell");action("home");check(r.view().buttons[1].id=="open:mesh_ble","Home shortcut persists across VMs");
 check(disk.write("/config/public/profile","offline"),"offline compose fixture");
 open("mesh_rak");action("compose");action("send","4294967295\n0\nOlá malha");tick();
 std::string queue;check(disk.read("/data/mesh/outbox.tsv",queue) && queue.find("Olá malha")!=std::string::npos,"message queued offline");check(sent.empty(),"not sent without sync");
 check(disk.write("/config/public/profile","normal"),"enable radio for connection test");
 action("wiring");check(r.view().text.find("GPIO53")!=std::string::npos,"Grove wiring information");action("back");action("connect");check(state=="Conectado UART" && r.view().statusFields[0].value.find("RAK")!=std::string::npos,"connect is acknowledged synchronously, not merely queued");tick();tick();check(sent.size()==1 && sent[0][0]==char(24),"want_config_id handshake");
 received.push_back(sent[0]);tick();std::string syncStatus;check(disk.read("/data/mesh/status.tsv",syncStatus) && syncStatus.find("Eco da solicitação")!=std::string::npos && sent.size()==1,"echo of want_config is not a radio response and does not unlock outbox");
 auto nonce=1u;received.push_back(num(7,nonce+1));tick();check(sent.size()==1,"wrong config ID cannot release outbox");
 auto syncBase=r.now();r.setClock(syncBase+4);received.push_back(bytes(4,num(1,42)+bytes(2,bytes(2,"Rádio de teste")+bytes(8,std::string(32,'K')))));tick();r.setClock(syncBase+8);tick();check(sent.size()==1,"large node sync stays alive while making progress");
 received.push_back(num(7,nonce));tick();check(sent.size()==2 && sent[1][0]==char(10),"synced outbox emits ToRadio packet");
 std::string history;check(disk.read("/data/mesh/history.tsv",history) && history.find("aceita pelo transporte")!=std::string::npos,"local write is not called delivery confirmation");
 auto outgoingId=unsigned(std::stoul(history.substr(8)));
 received.push_back(bytes(11,num(1,0)+num(4,outgoingId)));tick();check(disk.read("/data/mesh/history.tsv",history) && history.find("entregue ao rádio")!=std::string::npos,"radio queue receipt is distinct from transport acceptance");
 received.push_back(bytes(2,fixed(1,42)+bytes(4,num(1,5)+fixed(6,outgoingId)+bytes(2,num(3,0)))));tick();check(disk.read("/data/mesh/history.tsv",history) && history.find("confirmada")!=std::string::npos,"routing acknowledgement confirms matching message");
 auto inbound=bytes(2,fixed(1,42)+fixed(6,123)+bytes(4,num(1,1)+bytes(2,"Olá Tab5")));
 received.push_back(inbound);received.push_back(inbound);tick();check(disk.read("/data/mesh/history.tsv",history),"history exists");auto pos=history.find("Olá Tab5");check(pos!=std::string::npos && history.find("Olá Tab5",pos+1)==std::string::npos,"incoming duplicate suppressed");check(!r.notices().empty(),"incoming notification");
 received.push_back(std::string("\xff",1));tick();check(r.ready(),"malformed protobuf isolated");
 action("history");check(r.view().layout=="chat" && !r.view().buttons.empty(),"Meshtastic history uses readable message bubbles");action("back");
 // Own name and rich node details come from real FromRadio protobuf fields.
 received.push_back(bytes(3,num(1,99)));
 received.push_back(bytes(4,num(1,99)+bytes(2,bytes(2,"Meu RAK")+bytes(3,"RAK"))));tick();
 action("refresh");check(r.view().statusFields[1].value.find("Meu RAK")!=std::string::npos,"main shows own node name: "+r.view().title+" | "+r.view().statusFields[1].value);
 action("nodes");check(r.view().layout=="mesh_nodes" && r.view().centerTools,"node details and bottom pagination share a screen");action("node:42");check(r.view().statusFields[0].value.find("Rádio de teste")!=std::string::npos,"selected node details shown inline");action("dm");check(r.view().layout=="chat" && r.view().field && r.view().controls.size()==3 && r.view().text.rfind("42\n",0)==0,"DM opens inline composer with node selected");
 r.setClock(r.now()+3);action("send","42\n3\nPrivada de teste");tick();
 auto privatePacket=decode(decode(sent.back())[1].s);check(privatePacket[2].n==42 && privatePacket[3].n==3 && privatePacket[10].n==1 && !privatePacket.count(17) && !privatePacket.count(16),"DM delegates encryption to radio and preserves selected channel even with cached public key");
 received.push_back(bytes(2,fixed(1,42)+fixed(2,99)+fixed(6,125)+bytes(4,num(1,1)+bytes(2,"Resposta privada"))));tick();action("refresh","42\n0\nrascunho intacto");
 check(r.view().buttons.size()==2 && r.view().buttons[0].outgoing && !r.view().buttons[1].outgoing,"private thread filters broadcast history and preserves directions");check(r.view().text.find("rascunho intacto")!=std::string::npos,"automatic refresh retains composer draft");
 received.push_back(bytes(2,fixed(1,99)+bytes(4,num(1,5)+fixed(6,privatePacket[6].n)+bytes(2,num(3,0)))));tick();check(disk.read("/data/mesh/history.tsv",history) && history.find("Privada de teste\tentregue ao rádio")!=std::string::npos,"local routing acknowledgement is not remote delivery confirmation");
 received.push_back(bytes(2,fixed(1,42)+bytes(4,num(1,5)+fixed(6,privatePacket[6].n)+bytes(2,num(3,6)))));tick();action("refresh","42\n0\nrascunho intacto");
 check(r.view().buttons[0].label.find("NO_CHANNEL")!=std::string::npos && r.view().buttons[0].label.find("nó !0000002a")!=std::string::npos,"routing failure explains error and remote origin inside readable bubble");
 received.push_back(bytes(11,num(1,0)+num(4,privatePacket[6].n)));tick();action("refresh","42\n0\nrascunho intacto");check(r.view().buttons[0].label.find("NO_CHANNEL")!=std::string::npos,"late queue status cannot hide a terminal routing error");
 auto privateSentCount=sent.size();action(("message:"+std::to_string(privatePacket[6].n)).c_str());check(r.view().text.find("Privada de teste")!=std::string::npos && r.view().text.find("NO_CHANNEL")==std::string::npos && sent.size()==privateSentCount,"failed DM copies original text for manual retry without automatic resend");
 action("refresh","4294967295\n0\nrascunho intacto");for(auto &b:r.view().buttons)check(b.label!="Resposta privada","private replies do not leak into channel conversation");
 // Security frames are reduced to presence flags, never persisted as secrets.
 received.push_back(bytes(5,bytes(8,bytes(1,std::string(32,'P'))+bytes(2,std::string(32,'S')))));
 received.push_back(bytes(13,bytes(1,"2.7.26.fixture")+num(11,1)));
 received.push_back(bytes(5,bytes(6,num(7,16))));tick();action("back");action("diagnostic");
 check(r.view().text.find("CHAVE PRIVADA NO RÁDIO: presente (32 bytes recebidos)")!=std::string::npos && r.view().text.find("REGIÃO: 16")!=std::string::npos,"diagnostic reports radio-owned key presence and region");
 check(r.view().text.find("FIRMWARE RAK: 2.7.26.fixture")!=std::string::npos && r.view().text.find("SUPORTE PKI RAK: sim (hasPKC)")!=std::string::npos,"radio metadata reports PKI support independently of saved keys");
 action("save_diagnostic");std::string diagnosticExport;check(disk.read("/data/mesh/diagnostic.txt",diagnosticExport) && diagnosticExport.find("ORIGEM:")!=std::string::npos && diagnosticExport.find(std::string(32,'S'))==std::string::npos,"exportable diagnostic includes origin without private key values");
 check(r.view().text.find(std::string(32,'S'))==std::string::npos && r.view().text.find(std::string(32,'P'))==std::string::npos,"key values absent from diagnostic view");
 for(const auto &entry:fs::recursive_directory_iterator(root/"data/mesh")) if(entry.is_regular_file()) {std::ifstream in(entry.path(),std::ios::binary);std::string content((std::istreambuf_iterator<char>(in)),{});check(content.find(std::string(32,'S'))==std::string::npos,"private key never persisted to SD");}
 received.push_back(bytes(5,bytes(8,bytes(1,std::string(32,'P')))));tick();action("refresh");check(r.view().text.find("CHAVE PRIVADA NO RÁDIO: não informada")!=std::string::npos,"omitted private key is unknown rather than missing");
 received.push_back(bytes(5,bytes(8,bytes(2,std::string(32,'\0')))));tick();action("refresh");check(r.view().text.find("CHAVE PRIVADA NO RÁDIO: ausente ou inválida")!=std::string::npos,"zero key reported invalid without exposing content");
 received.push_back(bytes(13,bytes(1,"2.7.26.fixture")));tick();action("refresh");check(r.view().text.find("não anunciado (false/omitido)")!=std::string::npos,"missing metadata PKI flag is not inferred from stored key lengths");
 action("back");action("channels");action("channel_read");
 auto adminRequest=[&](){return decode(decode(sent.back())[1].s);};
 auto adminReply=[&](unsigned index,unsigned role,const std::string &settings,unsigned from=99,unsigned request=0) {
  auto packet=adminRequest();auto id=request?request:packet[6].n;
  auto c=num(1,index)+bytes(2,settings)+num(3,role);
  received.push_back(bytes(2,fixed(1,from)+bytes(4,num(1,6)+bytes(2,bytes(2,c)+bytes(101,"session!"))+fixed(6,id))));tick();
 };
 for(unsigned i=0;i<8;++i) {auto pkt=adminRequest();auto data=decode(pkt[4].s);check(data[1].n==6 && decode(data[2].s)[1].n==i+1,"admin reads use index plus one");adminReply(i,i==0?1:0,bytes(3,i==0?"Principal":""));}
 std::string channelStatus;check(disk.read("/data/mesh/channel-status.tsv",channelStatus) && channelStatus.find("atualizados")!=std::string::npos,"read all eight channels from radio");
 action("channel_new");check(r.view().layout=="form","secondary creation editor");action("channel_save","Teste\n00112233445566778899aabbccddeeff");
 auto before=sent.size();adminReply(1,0,bytes(3,""),42);check(sent.size()==before,"foreign admin response ignored");
 adminReply(1,0,bytes(3,"")+num(8,1));
 auto writeData=decode(adminRequest()[4].s);auto admin=decode(writeData[2].s);check(admin[101].s=="session!","fresh radio admin passkey is included");
 auto proposed=decode(admin[33].s);auto settings=decode(proposed[2].s);check(proposed[1].n==1 && proposed[3].n==2 && settings[3].s=="Teste" && settings[2].s.size()==16 && settings[8].n==1,"create secondary preserves unrelated settings and encodes key");
 check(disk.read("/data/mesh/channel-status.tsv",channelStatus) && channelStatus.find("confirmado pelo") ==std::string::npos,"write alone is not success");
 r.setClock(r.now()+2);tick();adminReply(1,2,proposed[2].s);
 check(disk.read("/data/mesh/channel-status.tsv",channelStatus) && channelStatus.find("confirmado pelo rádio")!=std::string::npos,"readback confirms actual saved channel");
 action("channel:0");action("channel_disable");before=sent.size();adminReply(0,1,bytes(3,"Principal"));check(sent.size()==before && disk.read("/data/mesh/channel-status.tsv",channelStatus) && channelStatus.find("não pode")!=std::string::npos,"primary channel cannot be disabled");
 // Failed verification remains an error and does not report success.
 action("channel:1");action("channel_save","Outro\n");adminReply(1,2,proposed[2].s);r.setClock(r.now()+2);tick();adminReply(1,2,proposed[2].s);
 check(disk.read("/data/mesh/channel-status.tsv",channelStatus) && channelStatus.find("não confirmou")!=std::string::npos,"mismatched readback rejected");
 open("mesh_ble");action("connect");check(r.view().controls.size()==2 && r.view().controls[1].type=="password","BLE PIN form");action("connect_ble","invalid\n123456");check(r.view().layout=="form","invalid BLE address refused");
 action("back");action("compose");action("send","4294967295\n0\n"+std::string(229,'x'));check(r.view().layout=="chat" && r.view().body.find("228")!=std::string::npos,"oversize message refused in inline composer");
 check(disk.write("/config/public/profile","offline"),"offline profile fixture");tick();check(state=="Desconectado","offline stops radio");
 // A cold Grove radio gets bounded retries without releasing queued text.
 check(disk.write("/config/public/profile","normal"),"normal profile for retry test");open("mesh_rak");action("connect");
 action("compose");action("send","4294967295\n0\nAguardando inicialização");sent.clear();tick();
 for(unsigned i=0;i<12;++i) {
  check(sent.size()==i+1 && sent.back().size()==2 && sent.back()[0]==char(24),"only small want_config requests while unsynced");
  received.push_back(sent.back());tick();
  r.setClock(r.now()+6);tick();tick();
 }
 check(sent.size()==12,"UART retry limit is twelve");r.setClock(r.now()+30);tick();check(sent.size()==12,"no unbounded retries after timeout");
 check(disk.read("/data/mesh/outbox.tsv",queue) && queue.find("Aguardando inicialização")!=std::string::npos,"cold-start timeout preserves queued text");
 check(disk.write("/config/public/profile","offline"),"restore offline after retry test");tick();
 // Wire parser rejects truncation/invalid keys and round-trips UTF-8 text.
 auto *L=luaL_newstate();luaL_openlibs(L);check(luaL_dofile(L,(root/"system/releases/1.0.0/mesh_proto.lua").c_str())==LUA_OK,"load original codec");lua_setglobal(L,"p");
 const char *script="assert(p.decode(string.char(0))==nil);assert(p.decode(string.char(10,4,1))==nil);assert(p.decode(string.rep(string.char(128),11))==nil);local f=p.decode(p.text(7,4294967295,0,'ação'));local packet=p.decode(f[1]);assert(packet[2]==4294967295 and packet[6]==7);assert(p.decode(packet[4])[2]=='ação');assert(p.text(1,1,8,'x')==nil)";
 check(luaL_dostring(L,script)==LUA_OK,"protobuf validation and golden fields");
 const char *pki="local dm=p.decode(p.decode(p.text(8,42,3,'teste',string.rep('K',32)))[1]);assert(dm[17]==nil and dm[16]==nil and dm[3]==3 and dm[10]==1);local b=p.decode(p.decode(p.text(9,4294967295,3,'publica'))[1]);assert(b[3]==3 and b[17]==nil and b[16]==nil and b[10]==0)";
 check(luaL_dostring(L,pki)==LUA_OK,"radio-owned crypto fields and broadcast channel behavior");lua_close(L);
 // Verify parser replacement against std::getline at delimiter/EOF boundaries.
 for(auto source:{std::string(""),std::string("\n"),std::string("a\n\nb\n"),std::string("a,b,,c,"),std::string("a\tb\t")}) for(char delimiter:{'\n',',','\t'}) {
  TextLines actual(source);std::istringstream reference(source);std::string a,b;
  for(unsigned i=0;i<10;++i) {bool x=nextLine(actual,a,delimiter),y=bool(std::getline(reference,b,delimiter));check(x==y && (!x || a==b),"line parser EOF/delimiter compatibility");if(!x)break;}
 }
 // Keep selectors bounded with the full 128-node database and UTF-8 names.
 std::string manyNodes="T5DATA1\n",longName;for(int i=0;i<32;++i)longName+="á";
 for(unsigned i=1;i<=128;++i)manyNodes+=std::to_string(i)+"\t"+longName+std::to_string(i)+"\tND\t1794174000\t6.5\t2\t80\t4.0\t-15.5\t-56.1\t150\t0\n";
 check(disk.write("/data/mesh/nodes.tsv",manyNodes),"128-node UTF-8 fixture");open("mesh_rak");action("nodes");action("node:128");action("dm");
 check(r.view().layout=="chat" && r.view().controls[0].options.size()<=4096 && std::count(r.view().controls[0].keys.begin(),r.view().controls[0].keys.end(),'\n')<=63 && r.view().text.rfind("128\n",0)==0,"bounded selectors retain any node selected from full database");
 // Real radio bursts update a full database, not only its list view.
 check(disk.write("/config/public/profile","normal"),"enable burst fixture");action("back");action("connect");tick();
 for(unsigned i=1;i<=18;++i)received.push_back(bytes(4,num(1,i)+bytes(2,bytes(2,"Nó burst "+std::to_string(i))+bytes(3,"BST")+bytes(8,std::string(32,'B')))+num(5,1794174000)));
 for(unsigned i=0;i<5;++i)tick();
 std::string fullHistory="T5DATA1\n";for(unsigned i=0;i<80;++i)fullHistory+=std::to_string(1000+i)+"\t42\t1794174000\t0\t"+std::string(228,'x')+"\trecebida\t4294967295\n";
 check(disk.write("/data/mesh/history.tsv",fullHistory),"full message history fixture");action("connect");tick();
 for(unsigned i=0;i<6;++i)received.push_back(bytes(2,fixed(1,42)+fixed(6,2000+i)+bytes(4,num(1,1)+bytes(2,std::string(228,'y')))));
 for(unsigned i=0;i<4;++i)tick();std::string burstHistory;check(disk.read("/data/mesh/history.tsv",burstHistory) && burstHistory.find("2005")!=std::string::npos,"full history and node database accept a six-message batch without Lua budget failure");
 action("history");action("refresh","4294967295\n0\nrascunho sob carga");check(r.view().layout=="chat" && r.view().buttons.size()==2 && r.view().text.find("rascunho sob carga")!=std::string::npos,"full chat renders the last page and preserves draft under database load: "+r.view().title+" buttons="+std::to_string(r.view().buttons.size())+" text="+r.view().text.substr(0,100));
 // An app without mesh cannot use the privileged services event as a deputy.
 std::string manifest;check(disk.read("/boot/manifest.txt",manifest),"manifest fixture");
 const std::string unauthorized="function event() sys.send('services','mesh','uart');return {title='No mesh permission'} end";
 check(disk.write("/system/releases/1.0.0/unauthorized.lua",unauthorized),"unprivileged app fixture");
 manifest+="app.unprivileged=unauthorized.lua\nfile=unauthorized.lua|"+sha256(unauthorized)+"\n";
 check(disk.write("/boot/manifest.txt",manifest),"test manifest");for(auto p:{"/boot/confirmed.txt","/boot/previous.txt","/boot/pending.txt","/boot/attempt.txt"})disk.remove(p);
 check(disk.write("/config/public/profile","normal"),"normal profile for permission test");disk.remove("/data/mesh/connection.tsv");
 Runtime unprivileged(disk);bool accessed=false;unprivileged.radioCommand=[&](const std::string &op,const std::string &,std::string &){if(op=="connect_uart")accessed=true;return true;};
 check(unprivileged.boot() && unprivileged.open("unprivileged") && unprivileged.tick() && !accessed,"mesh capability enforced for service messages");
 fs::remove_all(root);std::cout<<"Mesh protocol, offline queue, handshake, dedupe, Home and BLE forms passed\n";
}
