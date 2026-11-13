// SPDX-License-Identifier: MIT
// Original bounded reader for the unsigned Tab5 SIS v9 profile.
#include "installer.hpp"
#include "digest.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
#include <stdexcept>
#ifdef ESP_PLATFORM
#include "mbedtls/sha1.h"
#else
#include <openssl/evp.h>
#endif
namespace tab5 { namespace {
uint64_t le(const std::string &s,size_t at,unsigned n){
 if(at>s.size()||n>s.size()-at)throw std::runtime_error("SIS truncated integer");
 uint64_t value=0;for(unsigned i=0;i<n;++i)value|=uint64_t(uint8_t(s[at+i]))<<(8*i);return value;
}
uint16_t crc16(const std::string &s,unsigned parity){uint16_t crc=0;for(size_t i=parity;i<s.size();i+=2){crc^=uint16_t(uint8_t(s[i]))<<8;for(unsigned b=0;b<8;++b)crc=uint16_t((crc<<1)^((crc&0x8000)?0x1021:0));}return crc;}
std::string sha1(const std::string &s){unsigned char raw[20]{};
#ifdef ESP_PLATFORM
 if(mbedtls_sha1(reinterpret_cast<const unsigned char*>(s.data()),s.size(),raw)!=0)throw std::runtime_error("SIS SHA1 failed");
#else
 unsigned count=0;if(EVP_Digest(s.data(),s.size(),raw,&count,EVP_sha1(),nullptr)!=1||count!=20)throw std::runtime_error("SIS SHA1 failed");
#endif
 return std::string(reinterpret_cast<char*>(raw),20);
}
struct Field {uint32_t type;size_t at,size,next;};
struct Reader {
 Storage &disk;std::string path;uint64_t generation;const std::atomic<bool> *cancel;
 std::string get(size_t at,size_t n){
  if((cancel&&cancel->load())||disk.generation()!=generation)throw std::runtime_error("SIS cancelled or card changed");
  std::string data;if(!disk.readRange(path,at,n,data)||data.size()!=n)throw std::runtime_error("SIS truncated/read failed");return data;
 }
};
struct Cursor {
 Reader &r;size_t at,end;
 Field take(unsigned expected=0,bool array=false){
  const size_t header=array?4:8;
  if(at>end||header>end-at)throw std::runtime_error("SIS missing field");
  auto h=r.get(at,header);uint32_t type=array?expected:le(h,0,4);size_t n=le(h,array?0:4,4);
  if((n&0x80000000u)|| (expected&&type!=expected))throw std::runtime_error("SIS unsupported field/signature/length");
  size_t data=at+header;if(n>end-data || ((4-n%4)%4)>end-data-n)throw std::runtime_error("SIS field bounds");
  size_t next=data+n+(4-n%4)%4;
  if(next>data+n && r.get(data+n,next-data-n)!=std::string(next-data-n,'\0'))throw std::runtime_error("SIS nonzero padding");
  at=next;return {type,data,n,next};
 }
 void done(){if(at!=end)throw std::runtime_error("SIS extra fields/trailing bytes");}
};
Cursor inside(Reader &r,const Field &f){return {r,f.at,f.at+f.size};}
uint32_t integer(Reader &r,Cursor &c,unsigned type){auto f=c.take(type);if(f.size!=4)throw std::runtime_error("SIS invalid integer");return le(r.get(f.at,4),0,4);}
std::string string(Reader &r,Cursor &c){auto f=c.take(1);if(f.size>1024||f.size%2)throw std::runtime_error("SIS UCS-2 length");auto s=r.get(f.at,f.size);std::string out;
 for(size_t i=0;i<s.size();i+=2){unsigned ch=le(s,i,2);if(ch<32||(ch>=0xd800&&ch<=0xdfff)||ch==127)throw std::runtime_error("SIS invalid UCS-2");if(ch<128)out+=char(ch);else if(ch<2048){out+=char(0xc0|(ch>>6));out+=char(0x80|(ch&63));}else{out+=char(0xe0|(ch>>12));out+=char(0x80|((ch>>6)&63));out+=char(0x80|(ch&63));}}
 return out;
}
Cursor array(Reader &r,Cursor &c,unsigned type){auto f=c.take(2);if(f.size<4||le(r.get(f.at,4),0,4)!=type)throw std::runtime_error("SIS array type");return {r,f.at+4,f.at+f.size};}
void emptyArray(Reader &r,Cursor &c,unsigned type){auto a=array(r,c,type);a.done();}
std::string oneString(Reader &r,Cursor &c){auto a=array(r,c,1);auto f=a.take(1,true); // Array entries omit their type.
 if(f.size>160||f.size%2)throw std::runtime_error("SIS localized text limit");
 // Decode through the same UCS-2 logic without accepting additional entries.
 auto raw=r.get(f.at,f.size);std::string out;for(size_t i=0;i<raw.size();i+=2){unsigned ch=le(raw,i,2);if(ch<32||ch==127||(ch>=0xd800&&ch<=0xdfff))throw std::runtime_error("SIS invalid text");if(ch<128)out+=char(ch);else if(ch<2048){out+=char(0xc0|(ch>>6));out+=char(0x80|(ch&63));}else{out+=char(0xe0|(ch>>12));out+=char(0x80|((ch>>6)&63));out+=char(0x80|(ch&63));}}a.done();return out;
}
Field uncompressed(Reader &r,Cursor &c,size_t limit){auto f=c.take(3);if(f.size<12)throw std::runtime_error("SIS compression header");auto h=r.get(f.at,12);if(le(h,0,4)!=0||le(h,4,8)!=f.size-12||f.size-12>limit)throw std::runtime_error("SIS compression unsupported/size limit");return {0,f.at+12,f.size-12,f.next};}
struct Entry {std::string target,hash;size_t bytes;};
}
bool Installer::sisArchive(const std::string &path,Package &p,std::map<std::string,std::string> &files,const std::atomic<bool> *cancel,bool collect){
 files.clear();p={};auto begin=std::chrono::steady_clock::now();
 try{
  uint64_t bytes=0;if(!disk_.fileSize(path,bytes)||bytes<32||bytes>4*1024*1024)throw std::runtime_error("SIS package limit (4 MiB)");
  Reader r{disk_,path,disk_.generation(),cancel};auto header=r.get(0,16);
  uint32_t uid=le(header,8,4);auto ids=header.substr(0,12);
  if(le(header,0,4)!=0x10201a7a||le(header,4,4)!=0||uid<0xe0000000||uid>0xefffffff||le(header,12,4)!=(uint32_t(crc16(ids,1))<<16|crc16(ids,0)))throw std::runtime_error("SIS v9 header/UID checksum invalid");
  Cursor root{r,16,size_t(bytes)};auto contents=inside(r,root.take(12));root.done();
  auto controllerData=uncompressed(r,contents,65536);Cursor controllerRoot{r,controllerData.at,controllerData.at+controllerData.size};auto controller=inside(r,controllerRoot.take(13));controllerRoot.done();
  auto info=inside(r,controller.take(14));if(integer(r,info,9)!=uid)throw std::runtime_error("SIS UID mismatch");
  auto vendor=string(r,info);auto title=oneString(r,info);if(oneString(r,info)!=vendor)throw std::runtime_error("SIS vendor mismatch");
  auto vf=info.take(4);if(vf.size!=12)throw std::runtime_error("SIS version");auto v=r.get(vf.at,12);std::string version;
  for(unsigned i=0;i<3;++i){auto n=le(v,i*4,4);if(n>2147483647)throw std::runtime_error("SIS version overflow");version+=(i?".":"")+std::to_string(n);}
  auto dateTime=inside(r,info.take(8));auto date=dateTime.take(6),time=dateTime.take(7);dateTime.done();
  if(date.size!=4||time.size!=3||r.get(date.at,4)!=std::string("\xea\x07\x00\x01",4)||r.get(time.at,3)!=std::string(3,'\0'))throw std::runtime_error("SIS profile timestamp");
  if(info.end-info.at!=2||r.get(info.at,2)!=std::string(2,'\0'))throw std::runtime_error("SIS installation type/flags");
  info.at=info.end;
  auto options=inside(r,controller.take(16));emptyArray(r,options,33);options.done();
  auto languages=inside(r,controller.take(15));auto languageArray=array(r,languages,11);auto lang=languageArray.take(11,true);if(lang.size!=4||le(r.get(lang.at,4),0,4)!=1)throw std::runtime_error("SIS language profile");languageArray.done();languages.done();
  auto prerequisites=inside(r,controller.take(17));emptyArray(r,prerequisites,18);
  auto requirements=array(r,prerequisites,18);std::vector<PackageDependency> dependencies;
  while(requirements.at<requirements.end){
   if(dependencies.size()>=16)throw std::runtime_error("SIS dependency count");
   auto dep=inside(r,requirements.take(18,true));auto duid=integer(r,dep,9);auto range=inside(r,dep.take(5));
   auto readVersion=[&](){auto field=range.take(4);if(field.size!=12)throw std::runtime_error("SIS dependency version");auto data=r.get(field.at,12);std::string out;for(unsigned i=0;i<3;++i){auto n=le(data,i*4,4);if(n>2147483647)throw std::runtime_error("SIS dependency version overflow");out+=(i?".":"")+std::to_string(n);}return out;};
   auto minimum=readVersion(),maximum=readVersion();range.done();oneString(r,dep);dep.done();dependencies.push_back({duid,minimum,maximum});
  }
  prerequisites.done();
  auto properties=inside(r,controller.take(19));emptyArray(r,properties,20);properties.done();
  auto block=inside(r,controller.take(28));auto descriptions=array(r,block,24);std::vector<Entry> entries;
  while(descriptions.at<descriptions.end){if(entries.size()>=33)throw std::runtime_error("SIS file count");auto desc=inside(r,descriptions.take(24,true));auto target=string(r,desc);if(!string(r,desc).empty())throw std::runtime_error("SIS MIME operations unsupported");auto hash=inside(r,desc.take(25));
   if(hash.end-hash.at<4||le(r.get(hash.at,4),0,4)!=1)throw std::runtime_error("SIS hash algorithm");
   hash.at+=4;auto blob=hash.take(37);if(blob.size!=20)throw std::runtime_error("SIS SHA1 length");auto digest=r.get(blob.at,20);hash.done();
   if(desc.end-desc.at!=28)throw std::runtime_error("SIS file description");
   auto sizes=r.get(desc.at,28);if(le(sizes,0,4)!=1||le(sizes,4,4)!=0||le(sizes,8,8)!=le(sizes,16,8)||le(sizes,8,8)>1024*1024||le(sizes,24,4)!=entries.size())throw std::runtime_error("SIS operation/index/size unsupported");
   entries.push_back({target,digest,size_t(le(sizes,8,8))});
  }
  emptyArray(r,block,13);emptyArray(r,block,26);block.done();if(integer(r,controller,40)!=0)throw std::runtime_error("SIS data index");controller.done();
  auto data=inside(r,contents.take(30));contents.done();auto units=array(r,data,31);auto unit=inside(r,units.take(31,true));units.done();data.done();auto payloads=array(r,unit,32);unit.done();
  std::map<std::string,std::string> hashes;std::set<std::string> aliases;std::string manifest;size_t total=0,index=0;
  for(const auto &entry:entries){auto file=inside(r,payloads.take(32,true));auto raw=uncompressed(r,file,1024*1024);file.done();if(raw.size!=entry.bytes)throw std::runtime_error("SIS data length mismatch");
   auto slash=entry.target.rfind('\\');if(slash==entry.target.npos)throw std::runtime_error("SIS target path");auto name=entry.target.substr(slash+1),alias=name;
   if(name.empty()||name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")!=name.npos||!Storage::validPath("/"+name))throw std::runtime_error("SIS unsafe filename");
   std::transform(alias.begin(),alias.end(),alias.begin(),[](unsigned char c){return char(std::tolower(c));});if(!aliases.insert(alias).second)throw std::runtime_error("SIS case collision");
   if(alias.size()>4&&(alias.substr(alias.size()-4)==".exe"||alias.substr(alias.size()-4)==".dll"||alias.substr(alias.size()-4)==".bin"))throw std::runtime_error("SIS native executables unsupported");
   std::string content;content.reserve(raw.size);for(size_t at=0;at<raw.size;at+=16384)content+=r.get(raw.at+at,std::min(size_t(16384),raw.size-at));
   if(sha1(content)!=entry.hash)throw std::runtime_error("SIS SHA1 mismatch");
   if(name.size()>4&&name.substr(name.size()-4)==".lua"&&(content.size()>65536||(!content.empty()&&uint8_t(content[0])==27)))throw std::runtime_error("SIS Lua source limit");
   total+=content.size();if(total>4*1024*1024)throw std::runtime_error("SIS payload limit");if(name=="manifest.txt")manifest=content;else hashes[name]=sha256(content);
   if(collect)files.emplace(name,std::move(content));
   ++index;
  }payloads.done();
  if(index<3||!parse(manifest,p)||p.hashes!=hashes||!p.uid||p.uid!=uid||p.title!=title||p.author!=vendor||p.version!=version||!p.hashes.count("LICENSE.txt")||p.hashes.count("theme.txt"))throw std::runtime_error("SIS Tab5 manifest/profile mismatch");
  if(p.dependencies.size()!=dependencies.size())throw std::runtime_error("SIS dependency metadata mismatch");
  for(size_t i=0;i<dependencies.size();++i)if(p.dependencies[i].uid!=dependencies[i].uid||p.dependencies[i].minimum!=dependencies[i].minimum||p.dependencies[i].maximum!=dependencies[i].maximum)throw std::runtime_error("SIS dependency metadata mismatch");
  const std::set<std::string> reserved={"shell","services","notes","calculator","contacts","agenda","clock","converter","files","backup","profiles","music","recorder","gallery","camera","installer","energy","network","feeds","email","browser","memory","bench","mesh_ble","mesh_rak","settings","lock","updates"};
  if(reserved.count(p.id))throw std::runtime_error("SIS system application ID reserved");
  for(const auto &entry:entries){auto name=entry.target.substr(entry.target.rfind('\\')+1);if(entry.target!="!:\\SymbianOS\\apps\\"+p.id+"\\"+p.version+"\\"+name)throw std::runtime_error("SIS destination outside app");}
  Digest digest;for(size_t at=0;at<bytes;at+=16384){auto chunk=r.get(at,std::min(size_t(16384),size_t(bytes)-at));digest.update(chunk.data(),chunk.size());}p.archiveHash=digest.finish();p.bytes=total;p.inspectionMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();return true;
 }catch(const std::exception &e){error=e.what();files.clear();p={};return false;}
}
}
