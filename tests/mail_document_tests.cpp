#include "browser_web.hpp"
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
namespace fs=std::filesystem;
std::string file(const fs::path &p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
struct Wire:MailWire {
 std::string data;size_t at=0;
 explicit Wire(std::string script):data(std::move(script)){}
 bool open(const std::string&,unsigned,bool,std::string&)override{return true;}
 bool upgrade(const std::string&,std::string&)override{return true;}
 int write(const char*,size_t n)override{return n;}
 int read(char *p,size_t n)override{n=std::min(n,data.size()-at);memcpy(p,data.data()+at,n);at+=n;return n;}
};
int main(int argc,char **argv){
 assert(argc==3);
 auto html="<head><style>SECRET STYLE</style></head><script>SECRET SCRIPT</script><div hidden>SECRET HIDDEN</div><h1 align='center'>Olá &amp; &#x1f600;</h1><p>Texto <b>forte</b> e <em>ênfase</em> <a href='https://example.test/?a=1&amp;b=2'>link</a></p><ul><li>Um</li><li>Dois</li></ul><img src='cid:photo' alt='Foto > teste'>";
 auto blocks=mail::document(html);std::string joined;bool bold=false,link=false,heading=false,image=false;
 for(auto &b:blocks){heading|=b.heading==1&&b.align=="center";image|=b.source=="cid:photo"&&b.alt=="Foto > teste";for(auto &run:b.runs){joined+=run.text;bold|=run.strong&&run.text=="forte";link|=run.href=="https://example.test/?a=1&b=2";}}
 assert(joined.find("SECRET")==joined.npos&&joined.find("Olá & 😀")!=joined.npos&&bold&&link&&heading&&image);
 assert(mail::htmlEntityText("&#0;&#xD800;&#99999999;")=="\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd");
 assert(mail::document(std::string(200,'<')).empty());
 std::string bomb;for(unsigned i=0;i<10000;++i)bomb+="<p>x</p>";assert(mail::document(bomb).size()<=97);
 std::string spanBomb="<p>";for(unsigned i=0;i<5000;++i)spanBomb+="<p><b>x</b><i>y</i></p>";spanBomb+="</p>";
 auto bounded=mail::document(spanBomb);size_t spans=0,characters=0;bool limited=false;
 for(const auto &b:bounded)for(const auto &run:b.runs){++spans;characters+=run.text.size();limited|=run.text.find("Conteúdo visual limitado")!=run.text.npos;}
 assert(spans<=97 && characters<17000 && limited);
 auto longParagraph=mail::document("<p>"+std::string(4000,'a')+std::string(5000,'b')+"</p>");
 assert(longParagraph.size()==2 && longParagraph[0].runs[0].text.size()==4096 && longParagraph[1].runs[0].text.find("limitado")!=std::string::npos);

 const auto png=file(fs::path(argv[2])/"picture.png");assert(!png.empty());
 const auto raw=std::string("From: sender@example.test\r\nTo: me@example.test\r\nSubject: Rich mail\r\nContent-Type: multipart/related; boundary=outer\r\n\r\n--outer\r\nContent-Type: multipart/alternative; boundary=alt\r\n\r\n--alt\r\nContent-Type: text/plain\r\n\r\nTexto alternativo áááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááááá\r\n--alt\r\nContent-Type: text/html; charset=utf-8\r\nContent-Transfer-Encoding: base64\r\n\r\n")+mail::base64(html)+"\r\n--alt--\r\n--outer\r\nContent-Type: image/png\r\nContent-ID: <photo>\r\nContent-Transfer-Encoding: base64\r\n\r\n"+mail::base64(png)+"\r\n--outer--\r\n";
 auto decoded=mail::decodeMessage(raw);assert(decoded.html==html&&decoded.body.find("Texto alternativo")!=decoded.body.npos&&decoded.images.size()==1&&decoded.images[0].cid=="photo"&&decoded.images[0].data==png);
 auto root=fs::temp_directory_path()/("tab5-rich-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);Storage disk(root.string());assert(disk.mount());
 MailAccount account;account.address="me@example.test";account.protocol="pop";account.inHost="pop.example.test";account.inUser=account.address;account.inPassword="receive";account.outHost="smtp.example.test";account.outUser=account.address;account.outPassword="send";account.inPort=995;
 const auto script="+OK\r\n+OK\r\n+OK\r\n+OK\r\n1 rich-id\r\n.\r\n+OK\r\n"+raw+"\r\n.\r\n";
 std::string error,id;
 {Mail mailbox(disk);assert(mailbox.configure(account,error));mailbox.wire=[script]{return std::make_unique<Wire>(script);};assert(mailbox.synchronize("INBOX",error));for(unsigned i=0;i<1000&&mailbox.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(!mailbox.status().busy);auto list=mailbox.list("inbox");assert(list.size()==1&&list[0].html.empty()&&list[0].images.empty());id=list[0].id;MailMessage read;assert(mailbox.read(id,read)&&read.html==html&&read.images[0].data==png);auto legacy=file(root/"data/mail/messages"/id);size_t legacyEnd=0;for(unsigned n=0;n<14;++n)legacyEnd=legacy.find('\n',legacyEnd)+1;legacy.resize(legacyEnd);assert(disk.write("/data/mail/messages/"+id,legacy));assert(mailbox.read(id,read)&&read.html.empty()&&read.images.empty());assert(mailbox.refresh(id,error));for(unsigned i=0;i<1000&&mailbox.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(!mailbox.status().busy&&mailbox.list("inbox").size()==1&&mailbox.read(id,read)&&read.html==html);}
 Runtime runtime(disk);assert(runtime.boot()&&runtime.open("email"));assert(runtime.dispatch("open:"+id));assert(runtime.view().layout=="web_document");bool externalButton=false;for(const auto &b:runtime.view().footer)externalButton|=b.id=="load_images";assert(externalButton);bool picture=bool(runtime.view().picture);assert(picture);
 for(unsigned i=0;i<20;++i)assert(runtime.dispatch("refresh")&&runtime.view().layout=="web_document");
 assert(runtime.dispatch("reply")&&runtime.view().layout=="mail_compose");assert(runtime.dispatch("open:"+id));
 assert(runtime.dispatch("toggle_html")&&runtime.view().layout=="diagnostic");assert(runtime.dispatch("toggle_html")&&runtime.view().layout=="web_document");
 // Explicit external loading is asynchronous; opening the document never fetches.
 unsigned fetches=0;runtime.network().backend.connect=[](auto&,auto&,std::string &ip,auto&){ip="127.0.0.1";return true;};runtime.network().backend.fetch=[&](const std::string&,const Network::Sink &sink,const std::atomic<bool>&,std::string&){++fetches;return sink(png.data(),png.size());};
 assert(runtime.network().connect("test",""));for(unsigned i=0;i<1000&&runtime.network().status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
 assert(fetches==0);assert(runtime.network().images("<img src='../photo.png'>","https://example.test/articles/item"));for(unsigned i=0;i<1000&&runtime.network().status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(fetches==1);std::string cached;assert(disk.read(Network::imageCachePath("https://example.test/photo.png"),cached,256*1024)&&cached==png);
 assert(runtime.open("feeds"));assert(runtime.storage().write("/data/feeds/sources.tsv","T5DATA1\nfixture\tFixture\thttps://example.test/feed\n"));
 assert(runtime.storage().write("/data/feeds/cache-fixture.xml","<rss><channel><item><title>Artigo</title><link>https://example.test/articles/item</link><description><![CDATA[<h2>Teste RSS</h2><p>HTML offline</p><img src='../photo.png'>]]></description></item></channel></rss>"));
 assert(runtime.dispatch("source:fixture")&&runtime.dispatch("article:1")&&runtime.view().layout=="web_document");picture=bool(runtime.view().picture);assert(picture);
 assert(runtime.dispatch("save_article"));assert(runtime.dispatch("back")&&runtime.dispatch("back")&&runtime.dispatch("saved"));assert(!runtime.view().buttons.empty());auto action=runtime.view().buttons[0].id;assert(runtime.dispatch(action)&&runtime.view().layout=="web_document");assert(runtime.dispatch("toggle_html")&&runtime.view().layout=="reader");
 std::string complex="<html><style>p{color:blue}</style><body><main>";for(unsigned i=0;i<4000;++i)complex+="<div>\r\n";complex+="<p>Complex mail survives</p>";for(unsigned i=0;i<4000;++i)complex+="</div>\r\n";complex+="</main></body></html>";
 std::string complexRaw="From: sender@example.test\r\nTo: me@example.test\r\nSubject: Complex\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"+complex;
 auto complexScript="+OK\r\n+OK\r\n+OK\r\n+OK\r\n1 complex-id\r\n.\r\n+OK\r\n"+complexRaw+"\r\n.\r\n";
 runtime.mail().wire=[complexScript]{return std::make_unique<Wire>(complexScript);};assert(runtime.mail().synchronize("INBOX",error));for(unsigned i=0;i<2000&&runtime.mail().status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));auto complexList=runtime.mail().list("inbox");std::string complexId;for(const auto &entry:complexList)if(entry.subject=="Complex")complexId=entry.id;if(complexId.empty()){std::cerr<<"Complex import: "<<runtime.mail().status().message<<" count="<<complexList.size()<<"\n";for(const auto &entry:complexList)std::cerr<<entry.subject<<" / "<<entry.id<<"\n";}assert(!complexId.empty());
 assert(runtime.open("email")&&runtime.dispatch("open:"+complexId)&&runtime.view().picture);for(unsigned i=0;i<10;++i){assert(runtime.dispatch("toggle_html"));assert(runtime.dispatch("toggle_html")&&runtime.view().picture&&runtime.ready());}
 MailMessage complexMessage;assert(runtime.mail().read(complexId,complexMessage));BrowserWeb boundedMail;boundedMail.feed(complexMessage.html.data(),complexMessage.html.size());auto admitted=boundedMail.finish();assert(disk.write("/logs/html-render-pending-email.txt",sha256(admitted+"\n")));assert(runtime.dispatch("refresh")&&runtime.view().layout=="reader"&&runtime.ready());assert(disk.remove("/logs/html-render-pending-email.txt"));assert(runtime.dispatch("refresh")&&runtime.view().picture);
 runtime.shutdown();fs::remove_all(root);std::cout<<"HTML sanitization, MIME alternatives/CID, SD persistence, native images, explicit downloads and RSS offline passed\n";
}
