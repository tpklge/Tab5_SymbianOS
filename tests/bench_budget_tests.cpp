// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unistd.h>
using namespace tab5;namespace fs=std::filesystem;
std::string load(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){assert(argc==2);auto root=fs::temp_directory_path()/("tab5-bench-budget-"+std::to_string(getpid()));fs::remove_all(root);fs::copy(argv[1],root,fs::copy_options::recursive);Storage disk(root.string());assert(disk.mount());auto manifest=load(root/"boot/manifest.txt");auto release=manifest.substr(manifest.find("release=")+8);release.resize(release.find('\n'));auto path="/system/releases/"+release+"/services.lua";auto script=load(root/path.substr(1));auto oldHash=sha256(script);
 script="local nativeMemory=sys.memory;local fixtureTime=0;sys.memory=function(op) if op=='time' then fixtureTime=fixtureTime+5000;return fixtureTime end;return nativeMemory(op) end\n"+script;
 auto at=manifest.find(oldHash);assert(at!=manifest.npos);manifest.replace(at,64,sha256(script));assert(disk.write(path,script)&&disk.write("/boot/manifest.txt",manifest));Runtime runtime(disk);assert(runtime.boot()&&runtime.open("bench")&&runtime.dispatch("start_fast")&&runtime.tick());
 for(unsigned i=0;i<400;++i){runtime.beginOperation();if(!runtime.tick()){std::cerr<<"tick "<<i<<": "<<runtime.error<<'\n';return 1;}}
 std::string trace;assert(disk.read("/data/bench/trace-1.tsv",trace)&&std::count(trace.begin(),trace.end(),'\n')==257);assert(disk.read("/data/bench/trace-2.tsv",trace)&&std::count(trace.begin(),trace.end(),'\n')==145);assert(!runtime.faulted());
 assert(runtime.dispatch("stop")&&runtime.tick());runtime.shutdown();fs::remove_all(root);std::cout<<"400 virtual samples and segment rollover stay inside the unchanged Lua instruction budget\n";
}
