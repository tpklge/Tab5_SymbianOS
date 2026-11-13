// SPDX-License-Identifier: MIT
#include "installer.hpp"
#include "text_lines.hpp"
#include <algorithm>
#include <mutex>
#include <stdexcept>
namespace tab5 {
std::recursive_mutex &installerMutex(){static std::recursive_mutex mutex;return mutex;}
namespace {
const std::string journal="/meta/sis-install.txt",staging="/tmp/install/sis";
bool validVersion(const std::string &s){return !s.empty()&&s.size()<=32&&s.front()!='.'&&s.back()!='.'&&s.find("..")==s.npos&&s.find_first_not_of("0123456789.")==s.npos;}
std::string registry(const std::vector<Package> &all){std::string out="T5APPS1\n";for(const auto &p:all)out+=p.id+"\t"+p.version+"\t"+sha256(p.manifest)+"\n";return out;}
}
bool Installer::recoverSis(){
 std::lock_guard<std::recursive_mutex> lock(installerMutex());
 try {
 if(!recoverLifecycle())return false;
 return disk_.atGeneration(disk_.generation(),[&]() -> bool {
 std::string record;if(!disk_.read(journal,record,131072)){if(disk_.lastError()=="not found")return true;error=disk_.lastError();return false;}
 TextLines lines(record);std::string magic,state,id,version,hash;
 nextLine(lines,magic);nextLine(lines,state);nextLine(lines,id);nextLine(lines,version);nextLine(lines,hash);
 if(magic!="T5SIS1"||(state!="prepared"&&state!="committing"&&state!="committed")||!identifier(id)||!validVersion(version)||hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=hash.npos){error="invalid SIS journal; preserved";return false;}
 const auto dest="/apps/"+id+"/"+version;
 std::string active;bool exists=disk_.read("/apps/registry.tsv",active,65536);
 if(!exists&&disk_.lastError()!="not found"){error=disk_.lastError();return false;}
 auto line=id+"\t"+version+"\t"+hash+"\n";
 bool committed=active.find("\n"+line)!=active.npos;
 if(committed){
  std::string manifest;Package p;if(!disk_.read(dest+"/manifest.txt",manifest,16384)||sha256(manifest)!=hash||!parse(manifest,p)||p.id!=id||p.version!=version){error="SIS committed manifest damaged; journal preserved";return false;}
  for(const auto &f:p.hashes){std::string content;if(!disk_.read(dest+"/"+f.first,content,1024*1024)||sha256(content)!=f.second){error="SIS committed payload damaged; journal preserved";return false;}}
 } else {
  // Never touch another active version or application. This destination was
  // checked absent before the prepared journal was committed.
  std::vector<File> parents;if(!disk_.listDir("/apps",parents)){error=disk_.lastError();return false;}
  for(const auto &parent:parents)if(parent.path=="/apps/"+id){
   std::vector<File> children;if(!disk_.listDir(parent.path,children)){error=disk_.lastError();return false;}
   for(const auto &child:children)if(child.path==dest&&!disk_.removeTree(dest)){error=disk_.lastError();return false;}
   if(!disk_.listDir(parent.path,children)){error=disk_.lastError();return false;}
   if(children.empty()&&!disk_.remove(parent.path)){error=disk_.lastError();return false;}
  }
 }
 std::vector<File> temporary;if(!disk_.listDir("/tmp/install",temporary)){error=disk_.lastError();return false;}
 for(const auto &f:temporary)if(f.path==staging&&!disk_.removeTree(staging)){error=disk_.lastError();return false;}
 if(!disk_.remove(journal)){error=disk_.lastError();return false;}
 return true;
 });
 } catch(const std::exception &e) {error=e.what();return false;}
}
bool Installer::installSis(const std::string &path,const std::atomic<bool> *cancel,const std::string &expectedHash){
 std::lock_guard<std::recursive_mutex> lock(installerMutex());
 try {
 std::vector<Package> all;if(!catalog(all))return false;
 const auto generation=disk_.generation();
 Package p;std::map<std::string,std::string> files;if(!sisArchive(path,p,files,cancel,true))return false;
 if(disk_.generation()!=generation){error="SIS card changed";return false;}
 if(!expectedHash.empty()&&p.archiveHash!=expectedHash){error="package changed after review";return false;}
 bool retained=false;if(!disk_.pathExists("/meta/sis-data/"+p.id+".txt",retained)){error=disk_.lastError();return false;}
 if(p.sisProfile==2||retained)return lifecycleInstall(path,cancel,expectedHash,false);
 for(const auto &a:all)if(a.id==p.id||a.uid==p.uid)return lifecycleInstall(path,cancel,expectedHash,false);
 std::string pendingId,pendingVersion;if(pendingSis(pendingId,pendingVersion)){error="Validate pending SIS first";return false;}
 auto candidate=all;candidate.push_back(p);if(!validateSisGraph(candidate))return false;
 if(all.size()>=32){error="installed app limit";return false;}
 const auto dest="/apps/"+p.id+"/"+p.version;std::vector<File> existing;
 if(!disk_.listDir("/apps",existing)){error=disk_.lastError();return false;}
 for(const auto &f:existing)if(f.path=="/apps/"+p.id){error="SIS destination already exists";return false;}
 if(!disk_.canFit(p.bytes+1024*1024+131072)){error=disk_.lastError();return false;}
 auto checkpoint=[&](const std::string &step){
#ifndef ESP_PLATFORM
  if(transactionCheckpoint&&!transactionCheckpoint(step)){error="injected interruption at "+step;return false;}
#endif
  if((cancel&&cancel->load())||disk_.generation()!=generation){error="SIS cancelled or card changed";return false;}return true;
 };
 auto mutate=[&](const std::function<bool()> &operation){return disk_.atGeneration(generation,operation);};
 auto state=[&](const std::string &name){if(!mutate([&]{return disk_.write(journal,"T5SIS1\n"+name+"\n"+p.id+"\n"+p.version+"\n"+sha256(p.manifest)+"\n",cancel);})){error=disk_.lastError();return false;}return checkpoint(name);};
 if(!mutate([&]{return disk_.prepare("/meta")&&disk_.prepare("/tmp/install");})||!state("prepared"))return false;
 if(!mutate([&]{return disk_.prepare(staging);})){error=disk_.lastError();return false;}
 for(const auto &f:files){if(!checkpoint("before:"+f.first)||!mutate([&]{return disk_.write(staging+"/"+f.first,f.second,cancel);})){if(error.empty())error=disk_.lastError();return false;}if(!checkpoint("write:"+f.first))return false;}
 // Read back every staged file before publishing an immutable version.
 for(const auto &f:files){std::string content;if(!disk_.read(staging+"/"+f.first,content,1024*1024)||sha256(content)!=sha256(f.second)){error="SIS staging readback failed";return false;}}
 if(!state("committing")||!mutate([&]{return disk_.prepare("/apps/"+p.id)&&disk_.move(staging,dest);})){if(error.empty())error=disk_.lastError();return false;}
 if(!checkpoint("published"))return false;
 all.push_back(p);
 if(!checkpoint("before_activation")||!mutate([&]{return disk_.prepare("/data/"+p.id)&&disk_.write("/apps/registry.tsv",registry(all),cancel);})){if(error.empty())error=disk_.lastError();return false;}
 // The atomic registry rename is the activation commit point. A cancellation
 // after this point cannot undo an installation already made visible.
#ifndef ESP_PLATFORM
 if(transactionCheckpoint&&!transactionCheckpoint("activated")){error="injected interruption at activated";return false;}
#endif
 if(!mutate([&]{return disk_.write(journal,"T5SIS1\ncommitted\n"+p.id+"\n"+p.version+"\n"+sha256(p.manifest)+"\n");})){error=disk_.lastError();return false;}
#ifndef ESP_PLATFORM
 if(transactionCheckpoint&&!transactionCheckpoint("committed")){error="injected interruption at committed";return false;}
#endif
 return mutate([&]{return recoverSis();});
 } catch(const std::exception &e) {error=e.what();return false;}
}
}
