// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "browser_profile.hpp"
#include <cassert>
#include <filesystem>
#include <thread>
#include <iostream>
#include <fstream>
#include <unistd.h>
using namespace tab5;
void waitFor(Network &n){for(unsigned i=0;i<2000&&n.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!n.status().busy);}
int main(int argc,char **argv){
 assert(argc==2||argc==4||argc==5);namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("tab5-browser-"+std::to_string(getpid()));fs::copy(argv[1],root,fs::copy_options::recursive);
 Storage disk(root.string());Runtime r(disk);assert(r.boot());auto &net=r.network();
 unsigned calls=0;bool fail=false,oversize=false,cancel=false;size_t large=0;const std::string url="https://example.test/start";
 net.backend.fetchDocument=[&](const std::string &requested,const Network::Sink &sink,const std::atomic<bool>&,std::string &error,std::string &resolved){
  ++calls;resolved="https://example.test/final/";
  std::string html=oversize?std::string(browserProfile::wireLimit+1,'x'):"<html><head><style>body{background:white}a{color:blue}</style></head><body><h1>Browser fixture</h1><a href='next.html'>Next page</a><a href='javascript:alert(1)'>Forbidden</a></body></html>";
  if(large){html="<html><body><h1>Large page</h1><!--";html+=std::string(large-html.size()-17,'x')+"--></body></html>";}
  bool ok=true;for(size_t at=0;at<html.size()&&ok;at+=4096)ok=sink(html.data()+at,std::min(size_t(4096),html.size()-at));
  if(cancel)net.cancel();if(fail){error="Injected failure";return false;}return ok;
 };
 assert(disk.prepare("/data/browser/cache"));assert(disk.write("/data/browser/cache/"+sha256(url)+".page",url+"\n<h1>OLD DESKTOP CACHE</h1>"));assert(Network::pageCachePath(url)!="/data/browser/cache/"+sha256(url)+".page");
 assert(r.open("browser")&&r.view().submitAction=="go");assert(r.dispatch("go",url));waitFor(net);
 net.backend.connect=[](const auto&,const auto&,std::string &ip,const auto&){ip="192.0.2.10";return true;};assert(net.connect("different-operation",""));waitFor(net);assert(net.status().message!="Página salva no cartão");
 assert(r.dispatch("refresh"));
 if(r.view().layout!="reader"){std::cerr<<r.error<<" / "<<r.view().body<<'\n';return 1;}
 assert(r.view().readonly&&!r.view().picture&&r.view().text.find("Browser fixture")!=std::string::npos);
 assert(!r.view().text.empty()&&r.view().body.find("https://example.test/final/")!=std::string::npos);
 assert(r.dispatch("links"));bool next=false,forbidden=false;for(const auto &b:r.view().buttons){next|=b.id=="link:1"&&b.subtitle=="https://example.test/final/next.html";forbidden|=b.subtitle.find("javascript:")==0;}assert(next&&!forbidden);
 assert(r.dispatch("link:1"));waitFor(net);assert(r.dispatch("refresh")&&!r.view().text.empty());assert(r.dispatch("star")&&r.dispatch("favorites"));assert(!r.view().buttons.empty());
 assert(r.dispatch("visit:1"));assert(!net.status().busy&&calls==2); // Offline cache revisit.
 const auto path=Network::pageCachePath("https://example.test/final/next.html");std::string original,data;assert(disk.read(path,original));
 for(unsigned mode=0;mode<3;++mode){fail=mode==0;oversize=mode==1;cancel=mode==2;assert(r.dispatch("reload"));waitFor(net);assert(r.dispatch("refresh"));assert(disk.read(path,data)&&data==original);assert(r.view().layout=="reader");}
 fail=oversize=cancel=false;
 for(size_t size:{100*1024,512*1024,1024*1024}){large=size;assert(r.dispatch("go","https://example.test/large"+std::to_string(size)));waitFor(net);assert(r.dispatch("refresh")&&!r.view().text.empty()&&r.view().layout=="reader");}large=0;
 unsigned mobileAssets=0,genericAssets=0;const std::string assetUrl="https://example.test/profile.css";
 net.backend.fetchBrowserAsset=[&](const auto&,const auto &sink,const auto&,auto&){++mobileAssets;return sink("mobile",6);};
 net.backend.fetch=[&](const auto&,const auto &sink,const auto&,auto&){++genericAssets;return sink("generic",7);};
 assert(net.documentAssets({{assetUrl,true}},true));waitFor(net);assert(net.documentAssets({{assetUrl,true}},false));waitFor(net);
 assert(mobileAssets==1&&genericAssets==1&&Network::imageCachePath(assetUrl,true)!=Network::imageCachePath(assetUrl));
 assert(disk.read(Network::imageCachePath(assetUrl,true),data)&&data=="mobile");assert(disk.read(Network::imageCachePath(assetUrl),data)&&data=="generic");
 assert(!net.browse("file:///etc/passwd"));assert(!net.browse("https://user:password@example.test/"));
 assert(!r.open("updates"));assert(r.open("energy"));bool restart=false;for(auto &b:r.view().buttons)restart|=b.id=="reboot";assert(restart);
 if(argc>=4){
  for(unsigned index=0;index<unsigned(argc-2);++index){std::ifstream file(argv[index+2],std::ios::binary);std::string captured{std::istreambuf_iterator<char>(file),{}};assert(!captured.empty());
   net.backend.fetchDocument=[&,index](const auto &,const auto &sink,const auto &,std::string &,std::string &resolved){resolved=index==2?"https://www.globo.com/":index?"https://g1.globo.com/":"https://www.google.com/";for(size_t at=0;at<captured.size();at+=4096)if(!sink(captured.data()+at,std::min(size_t(4096),captured.size()-at)))return false;return true;};
   assert(r.open("browser")&&r.dispatch("go","https://example.test/captured-"+std::to_string(index)));waitFor(net);assert(r.dispatch("refresh"));
   std::cout<<"Captured runtime "<<(index==2?"Globo":index?"G1":"Google")<<": bytes="<<captured.size()<<" layout="<<r.view().layout<<" picture="<<bool(r.view().picture)<<" body="<<r.view().body<<std::endl;
   assert(r.view().layout=="reader"&&!r.view().text.empty());
   for(unsigned repeat=0;repeat<3;++repeat){assert(r.dispatch("toggle_render"));std::cout<<"Captured rich repeat="<<repeat<<" layout="<<r.view().layout<<" picture="<<bool(r.view().picture)<<" diagnostic="<<r.view().text<<std::endl;assert(r.view().layout=="web_document"&&r.view().picture);assert(r.dispatch("html_next"));assert(r.dispatch("html_prev"));assert(r.dispatch("toggle_render")&&r.view().layout=="reader");}
  }
  net.backend.fetchDocument={};
 }
 r.shutdown();Runtime restored(disk);assert(restored.boot()&&restored.open("browser")&&restored.dispatch("favorites"));bool favorite=false;for(auto &b:restored.view().buttons)favorite|=b.label=="https://example.test/final/next.html";assert(favorite);restored.shutdown();
 fs::remove_all(root);std::cout<<"Browser rendering, resolved links, cache, favorites persistence, limits/cancellation and removed updater passed\n";
}
