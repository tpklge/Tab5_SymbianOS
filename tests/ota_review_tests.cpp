// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "ota_review.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace tab5;namespace fs=std::filesystem;
std::string load(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
 assert(argc==3);auto root=fs::temp_directory_path()/("tab5-ota-review-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);
 Storage disk(root.string());assert(disk.mount());auto binary=load(argv[2]);assert(binary.size()>1024*1024);std::string error;OtaReview report;bool cancel=false;
 auto review=[&](size_t capacity=4194304){return reviewOta(disk,capacity,[&]{return cancel;},report,error);};
 auto pair=[&](const std::string &bin){assert(disk.write("/update/runtime.bin",bin)&&disk.write("/update/runtime.sha256",sha256(bin)+"\n"));};
 pair(binary);assert(review()&&report.bytes==binary.size()&&report.actual==sha256(binary)&&report.stage=="verified");
 assert(!review(1048576)&&report.stage=="slot"&&error.find("capacidade")!=error.npos);
 pair(binary.substr(0,100));assert(!review()&&report.stage=="header");
 auto corrupt=binary;corrupt.back()^=1;assert(disk.write("/update/runtime.bin",corrupt));assert(!review()&&report.stage=="compare"&&report.actual!=report.expected);
 pair(binary);auto checksum=sha256(binary);for(auto &c:checksum)c=toupper((unsigned char)c);assert(disk.write("/update/runtime.sha256","\xef\xbb\xbf"+checksum+"\r\n"));assert(review());
 assert(disk.write("/update/runtime.sha256",sha256(binary)+"\\n"));assert(!review()&&report.stage=="checksum");
 pair(binary);cancel=true;assert(!review()&&error.find("cancelada")!=error.npos);cancel=false;
 auto wrong=binary;wrong[12]=9;pair(wrong);assert(!review()&&report.stage=="header");
 wrong=binary;wrong[80]='X';pair(wrong);assert(!review()&&error.find("projeto")!=error.npos);
 pair("PK\x03\x04"+std::string(4096,'x'));assert(!review()&&report.stage=="header");
 assert(disk.remove("/update/runtime.bin"));assert(!review()&&report.stage=="read");
 // Use a genuine Lua hook cancellation after the native callback returns.
 auto manifest=load(root/"boot/manifest.txt");auto release=manifest.substr(manifest.find("release=")+8);release.resize(release.find('\n'));
 const auto path=root/"system/releases"/release/"bench.lua";auto script=load(path),oldHash=sha256(script);
 auto at=script.find("local ok,why=sys.update('inspect');");assert(at!=script.npos);script.insert(at+std::string("local ok,why=sys.update('inspect');").size(),"for i=1,10000 do message=tostring(i) end;");
 assert(disk.write("/system/releases/"+release+"/bench.lua",script));auto h=manifest.find(oldHash);assert(h!=manifest.npos);manifest.replace(h,64,sha256(script));assert(disk.write("/boot/manifest.txt",manifest));
 Runtime runtime(disk);assert(runtime.boot()&&runtime.open("bench"));runtime.systemWriter=[&](const std::string&,std::string &message){runtime.cancel();message="cancel";return false;};
 runtime.beginOperation();assert(!runtime.dispatch("ota_review")&&!runtime.faulted()&&runtime.view().title=="Testes e medições");
 runtime.beginOperation();assert(runtime.dispatch("refresh")&&!runtime.faulted());
 assert(disk.write("/logs/update.txt","stage=compare\nbytes=2095120\n"));assert(runtime.open("updates")&&runtime.dispatch("diagnostic")&&runtime.view().layout=="diagnostic"&&runtime.view().text.find("stage=compare")!=std::string::npos);assert(runtime.dispatch("back")&&runtime.current()=="updates");
 runtime.shutdown();fs::remove_all(root);
 std::cout<<"Actual ESP32-P4 BIN, SHA, slot/truncation/corruption/header/cancel regressions passed\n";
}
