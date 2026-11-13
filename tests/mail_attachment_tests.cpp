// SPDX-License-Identifier: MIT
#include "mail.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;namespace fs=std::filesystem;
struct State {std::string input,output;};
struct Wire:MailWire {std::shared_ptr<State>s;size_t at=0;explicit Wire(std::shared_ptr<State> v):s(v){}bool open(const std::string&,unsigned,bool,std::string&)override{return true;}bool upgrade(const std::string&,std::string&)override{return true;}int read(char *p,size_t n)override{n=std::min(n,s->input.size()-at);memcpy(p,s->input.data()+at,n);at+=n;return n;}int write(const char*p,size_t n)override{s->output.append(p,n);return n;}};
void wait(Mail &m){for(unsigned i=0;i<2000&&m.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!m.status().busy);}
std::string script(const std::string &raw){return "+OK\r\n+OK\r\n+OK\r\n+OK\r\n1 attachment-uid\r\n.\r\n+OK\r\n"+raw+"\r\n.\r\n";}
int main(int argc,char **argv){assert(argc==2);auto root=fs::temp_directory_path()/("tab5-mail-attachments-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);Storage disk(root.string());assert(disk.mount());
 MailAccount a;a.address="me@example.test";a.inUser=a.address;a.inHost="pop.example.test";a.inPassword="receive";a.protocol="pop";a.inPort=995;a.outHost="smtp.example.test";a.outPort=465;
 std::string binary;for(unsigned i=0;i<15000;++i)binary+=char(i%256);
 std::string b64=mail::base64(binary),wrapped;for(size_t p=0;p<b64.size();p+=76)wrapped+=b64.substr(p,76)+"\r\n";
 std::string raw="From: friend@example.test\r\nSubject: Attachment test\r\nContent-Type: multipart/mixed; boundary=parts\r\n\r\n--parts\r\nContent-Type: text/plain\r\n\r\nBody\r\n--parts\r\nContent-Type: application/pdf\r\nContent-Disposition: attachment; filename*=UTF-8''caf%C3%A9.pdf\r\nContent-Transfer-Encoding: base64\r\n\r\n"+wrapped+"\r\n--parts--\r\n";
 // The delimiter CRLF must not become an extra byte in raw attachments.
 for(const auto &encoding:{"binary","7bit","quoted-printable"}) {
  const std::string body="ABC\r\n--edgeExtra\r\n";
  auto fixture="Content-Type: multipart/mixed; boundary=edge\r\n\r\n--edge\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=x.bin\r\nContent-Transfer-Encoding: "+std::string(encoding)+"\r\n\r\n"+body+"\r\n--edge--\r\n";
  std::string received;mail::decodeMessage(fixture,[&](auto&,auto&,auto&,auto &data,MailAttachment &){received=data;return true;});assert(received==body);
 }
 std::string error,id;auto state=std::make_shared<State>();state->input=script(raw);
 {Mail m(disk);assert(m.configure(a,error));m.wire=[state]{return std::make_unique<Wire>(state);};assert(m.synchronize("INBOX",error));wait(m);auto list=m.list("inbox");if(list.size()!=1)std::cerr<<"MAIL: "<<m.status().message<<" DISK: "<<disk.lastError()<<"\n";assert(list.size()==1);id=list[0].id;MailMessage msg;assert(m.read(id,msg)&&msg.attachments.size()==1&&msg.attachments[0].name=="café.pdf"&&msg.attachments[0].size==binary.size());std::string bytes;assert(disk.read(msg.attachments[0].path,bytes)&&bytes==binary);
 assert(m.exportAttachment(id,0,error));wait(m);std::vector<File> exports;assert(disk.listDir("/home/Downloads",exports));bool exported=false;for(const auto &f:exports)if(f.path.find(id.substr(0,8))!=f.path.npos)exported=disk.read(f.path,bytes)&&bytes==binary;assert(exported);
 assert(m.refresh(id,error));wait(m);assert(m.list("inbox")[0].seen);
 assert(m.move(id,"trash",error)&&m.move(id,"inbox",error)&&m.move(id,"trash",error)&&m.move(id,"delete",error));assert(m.synchronize("INBOX",error));wait(m);assert(m.list("inbox").empty());
 m.stop();assert(disk.remove("/data/mail/deleted"));
 assert(m.synchronize("INBOX",error));wait(m);assert(m.list("inbox").size()==1);
 assert(m.move(id,"trash",error)&&m.move(id,"delete",error));}
 {Mail m(disk);m.wire=[state]{return std::make_unique<Wire>(state);};assert(m.synchronize("INBOX",error));wait(m);assert(m.list("inbox").empty());auto other=a;other.address="other@example.test";other.inUser=other.address;assert(m.configure(other,error)&&m.synchronize("INBOX",error));wait(m);assert(m.list("inbox").size()==1);assert(m.configure(a,error));
 MailMessage draft;draft.to="friend@example.test";draft.subject="Binary attachment";draft.body="Text body";assert(m.draft(draft,false,error));assert(disk.write("/home/payload.bin",binary));assert(!m.attach(draft.id,"/config/mail/account",error));assert(m.attach(draft.id,"/home/payload.bin",error));wait(m);MailMessage attached;assert(m.read(draft.id,attached)&&attached.attachments.size()==1);
 draft.body="Edited body";assert(m.draft(draft,false,error));assert(m.read(draft.id,attached)&&attached.attachments.size()==1);assert(m.draft(draft,true,error));
 auto smtp=std::make_shared<State>();smtp->input="220 hello\r\n250 hello\r\n250 mail\r\n250 rcpt\r\n354 data\r\n250 accepted\r\n";m.wire=[smtp]{return std::make_unique<Wire>(smtp);};assert(m.flush(error));wait(m);assert(m.read(draft.id,attached)&&attached.status=="accepted");auto start=smtp->output.find("DATA\r\n");assert(start!=smtp->output.npos);std::string delivered;auto decoded=mail::decodeMessage(smtp->output.substr(start+6),[&](auto&,auto&,auto &encoding,auto &body,MailAttachment &attachment){assert(encoding=="BASE64");delivered=mail::unbase64(body);attachment.name="fixture";return true;});assert(delivered==binary&&decoded.body.find("Edited body")!=decoded.body.npos);
 auto interrupted=std::make_shared<State>();interrupted->input="220 hello\r\n250 hello\r\n250 mail\r\n250 rcpt\r\n354 data\r\n250 accepted\r\n";Wire wire(interrupted);std::atomic<bool> cancel{false};unsigned reads=0;
 auto result=mail::send(wire,a,attached,cancel,error,[&](const std::string &path,uint64_t offset,size_t n,std::string &out){assert(n<=3078);++reads;bool ok=disk.readRange(path,offset,n,out);cancel=true;return ok;});assert(reads==1&&result==mail::SendResult::uncertain);

 }
 // A failure cannot publish a partial attachment or erase the cached message.
 {Mail m(disk);auto badAccount=a;badAccount.address="broken@example.test";badAccount.inUser=badAccount.address;assert(m.configure(badAccount,error));
  auto badRaw=raw;auto position=badRaw.find(wrapped.substr(0,20));assert(position!=badRaw.npos);badRaw.replace(position,4,"@@@@");auto bad=std::make_shared<State>();bad->input=script(badRaw);m.wire=[bad]{return std::make_unique<Wire>(bad);};assert(m.synchronize("INBOX",error));wait(m);assert(m.list("inbox").empty());
  auto good=std::make_shared<State>();good->input=script(raw);m.wire=[good]{return std::make_unique<Wire>(good);};assert(m.synchronize("INBOX",error));wait(m);assert(m.list("inbox").size()==1);auto cached=m.list("inbox")[0].id;
  disk.setFreeSpaceProvider([]{return uint64_t(0);});assert(m.refresh(cached,error));wait(m);MailMessage retained;assert(m.read(cached,retained)&&retained.attachments.size()==1);disk.setFreeSpaceProvider({});
 }
 // Restoring a trashed IMAP message returns it to its original folder.
 {Mail m(disk);auto imap=a;imap.protocol="imap";imap.inPort=993;assert(m.configure(imap,error));
  auto server=std::make_shared<State>();server->input="* OK ready\r\nT1 OK capability\r\nT2 OK login\r\nT3 OK list\r\n* OK [UIDVALIDITY 42]\r\n* 1 EXISTS\r\nT4 OK select\r\n* SEARCH 7\r\nT5 OK search\r\n* 1 FETCH (RFC822.SIZE "+std::to_string(raw.size())+")\r\nT6 OK size\r\n* 1 FETCH (BODY[] {"+std::to_string(raw.size())+"}\r\n"+raw+"\r\n)\r\nT7 OK fetched\r\n";
  m.wire=[server]{return std::make_unique<Wire>(server);};assert(m.synchronize("Archive:2026",error));wait(m);auto list=m.list("remote:Archive:2026");assert(list.size()==1);auto id=list[0].id;assert(m.move(id,"trash",error)&&m.move(id,"inbox",error));assert(m.list("remote:Archive:2026").size()==1&&m.list("inbox").empty());
 }
 fs::remove_all(root);std::cout<<"MIME attachments/UTF8 filenames, streamed files/export/SMTP, draft preservation, persistent deletion and account isolation passed\n";
}
