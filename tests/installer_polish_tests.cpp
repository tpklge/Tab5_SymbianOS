// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <set>
#include <thread>
#include <unistd.h>
using namespace tab5;
void wait(Network &net){for(unsigned i=0;i<2000&&net.status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!net.status().busy);}
int main(int argc,char **argv){assert(argc==2);namespace fs=std::filesystem;auto root=fs::temp_directory_path()/("tab5-installer-polish-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);Storage disk(root.string());Runtime runtime(disk);assert(runtime.boot());
 for(unsigned n=0;n<65;++n){auto folder=n<45?"/home/Transfer/":"/update/packages/";auto name=std::string(folder)+"package-"+std::to_string(n)+(n%2?".SIS":".sis");assert(disk.write(name,"fixture"));}
 for(unsigned n=0;n<20;++n)assert(disk.write("/home/Transfer/not-a-package-"+std::to_string(n)+".txt","ignored"));assert(disk.prepare("/home/Transfer/folder.sis"));
 assert(runtime.open("installer"));std::set<std::string> first,all;for(const auto &b:runtime.view().buttons){assert(b.id.rfind("inspect:",0)==0);first.insert(b.id);all.insert(b.id);}assert(first.size()==40&&runtime.view().body.find("65")!=std::string::npos);
 assert(runtime.dispatch("next_list"));assert(runtime.view().buttons.size()==25);for(const auto &b:runtime.view().buttons){assert(!first.count(b.id));assert(all.insert(b.id).second);}assert(all.size()==65);assert(runtime.dispatch("next_list")&&runtime.view().buttons.size()==25);assert(runtime.dispatch("prev_list")&&runtime.view().buttons.size()==40);
 auto &net=runtime.network();net.backend.connect=[](const auto&,const auto&,std::string &ip,const auto&){ip="192.0.2.5";return true;};assert(net.connect("fixture",""));wait(net);
 net.backend.share=[](bool,std::string&,std::string&){return false;};assert(net.share(true));wait(net);assert(net.status().connected&&net.status().message.find("Conecte o Wi-Fi")==std::string::npos);
 net.backend.shareDiagnostic=[]{return "Iniciar envio web na porta 8080: ESP_ERR_NO_MEM";};assert(net.share(true));wait(net);assert(net.status().message.find("ESP_ERR_NO_MEM")!=std::string::npos);
 net.backend.share=[](bool on,std::string &address,std::string &token){address="192.0.2.5:8080";token=on?"TOKEN":"";return true;};assert(net.share(true));wait(net);assert(net.status().sharing&&net.status().address=="192.0.2.5"&&net.status().transferAddress=="192.0.2.5:8080");
 assert(runtime.open("network"));bool endpoint=false;for(auto &f:runtime.view().statusFields)endpoint|=f.value.find("http://192.0.2.5:8080")!=f.value.npos;assert(endpoint);net.backend.linked=[]{return false;};assert(!net.status().sharing&&net.status().transferAddress.empty());net.backend.linked={};assert(net.share(false));wait(net);assert(net.status().transferAddress.empty());
 runtime.shutdown();fs::remove_all(root);std::cout<<"Filtered/merged package pagination, uppercase SIS, no duplicates, sharing diagnostics and endpoint separation passed\n";
}
