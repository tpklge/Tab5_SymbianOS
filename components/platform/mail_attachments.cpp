// SPDX-License-Identifier: MIT
#include "mail.hpp"
#include <algorithm>
#include <cctype>
namespace tab5 {
namespace {
std::string safeName(std::string name){
 for(auto &c:name)if((unsigned char)c<32 || std::string("\\/:*?\"<>|~").find(c)!=std::string::npos)c='_';
 if(name.size()>64){size_t n=64;while(n&&((unsigned char)name[n]&0xc0)==0x80)--n;name.resize(n);}
 while(!name.empty()&&(name.back()=='.'||name.back()==' '))name.pop_back();
 return name.empty()||name=="."||name==".."?"attachment.bin":name;
}
}
bool Mail::loadDeletions(){
 if(deletionsLoaded_)return true;
 std::vector<File> files;if(!disk_.listDir("/data/mail",files))return false;
 deletedKeys_="@T5DEL1\n";
 for(const auto &file:files)if(file.path=="/data/mail/deleted"){
  if(!disk_.read(file.path,deletedKeys_,300000)||deletedKeys_.rfind("@T5DEL1\n",0)!=0)return false;
  for(size_t p=8;p<deletedKeys_.size();p+=65)if(p+65>deletedKeys_.size() || deletedKeys_[p+64]!='\n' || deletedKeys_.substr(p,64).find_first_not_of("0123456789abcdef")!=std::string::npos)return false;
 }
 deletionsLoaded_=true;return true;
}
bool Mail::isDeleted(const std::string &account,const std::string &uid){
 return !loadDeletions() || deletedKeys_.find("\n"+sha256(account+"\n"+uid)+"\n")!=std::string::npos;
}
bool Mail::deleted(const std::string &account,const std::string &uid,bool on){
 if(!loadDeletions())return false;
 const auto key=sha256(account+"\n"+uid);auto next=deletedKeys_;auto p=next.find("\n"+key+"\n");
 if(on && p==next.npos){if(next.size()+65>300000)return false;next+=key+"\n";}
 else if(!on && p!=next.npos)next.erase(p+1,65);
 if(!disk_.write("/data/mail/deleted",next))return false;
 deletedKeys_=std::move(next);return true;
}
bool Mail::saveAttachment(const std::string &id,const std::string &name,const std::string &type,const std::string &encoding,const std::string &body,MailAttachment &out,std::string &error){
 if(!error.empty())return false;
 const auto directory="/data/mail/attachments/"+id;
 const auto path=directory+"/"+sha256(body).substr(0,16)+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 disk_.mkdir(directory);const auto temporary=path+".part";
 if(!disk_.write(temporary,"")){error="Sem espaço para anexo";return false;}
 size_t bytes=0;std::string group;
 auto emit=[&](const std::string &data){bytes+=data.size();return bytes<=1024*1024&&!cancel_&&disk_.append(temporary,data);};
 bool ok=true;
 if(encoding=="BASE64"){
  bool padded=false;
  for(unsigned char c:body){
   if(isspace(c))continue;
   if(padded || !(isalnum(c)||c=='+'||c=='/'||c=='=')){ok=false;break;}
   group+=char(c);
   if(group.size()==4096 || c=='='){
    // Padding is decoded only at the end; complete four-character groups first.
    if(group.size()%4)continue;
    auto pad=group.find('=');if(pad!=group.npos&&(pad+2<group.size()||group.find_first_not_of('=',pad)!=group.npos)){ok=false;break;}
    auto data=mail::unbase64(group);if(!emit(data)){ok=false;break;}if(group.find('=')!=group.npos)padded=true;group.clear();
   }
  }
  if(ok&&!group.empty()){if(group.size()%4)ok=false;else ok=emit(mail::unbase64(group));}
 }else if(encoding.empty()||encoding=="7BIT"||encoding=="8BIT"||encoding=="BINARY"){
  for(size_t p=0;p<body.size()&&ok;p+=4096)ok=emit(body.substr(p,4096));
 }else if(encoding=="QUOTED-PRINTABLE"){
  std::string data;
  for(size_t p=0;p<body.size()&&ok;++p){char c=body[p];if(c=='='&&p+1<body.size()){
    if(body[p+1]=='\r'&&p+2<body.size()&&body[p+2]=='\n'){p+=2;continue;}
    if(body[p+1]=='\n'){++p;continue;}
    auto digit=[](char n){return n>='0'&&n<='9'?n-'0':n>='A'&&n<='F'?n-'A'+10:n>='a'&&n<='f'?n-'a'+10:-1;};
    if(p+2>=body.size()||digit(body[p+1])<0||digit(body[p+2])<0){ok=false;break;}
    c=char(digit(body[p+1])*16+digit(body[p+2]));p+=2;
   }data+=c;if(data.size()>=4096){ok=emit(data);data.clear();}}
  if(ok&&!data.empty())ok=emit(data);
 }else ok=false;
 if(ok){std::string prior;if(disk_.readRange(path,0,1,prior))ok=false;else ok=disk_.move(temporary,path);}
 if(!ok){disk_.remove(temporary);error=cancel_?"Operação cancelada":"Falha, formato ou limite de anexo; confira espaço do SD";return false;}
 out={safeName(name),type,path,bytes};return true;
}
bool Mail::attach(const std::string &id,const std::string &source,std::string &error){
 MailMessage m;
 if(!read(id,m)||m.folder!="drafts"||m.attachments.size()>=4||source.rfind("/home/",0)!=0||!Storage::validPath(source)){error="Escolha um arquivo em /home; máximo quatro anexos";return false;}
 const auto slash=source.find_last_of('/');std::vector<File> files;
 if(!disk_.listDir(source.substr(0,slash),files)){error="Pasta indisponível";return false;}
 uint64_t size=0;bool found=false;for(const auto &f:files)if(f.path==source&&!f.directory){size=f.size;found=true;}
 uint64_t total=size;for(const auto &a:m.attachments)total+=a.size;
 if(!found||size>1024*1024||total>2*1024*1024){error="Anexo até 1 MiB; total até 2 MiB";return false;}
 if(!start([this,m,source,size]() mutable {
  auto directory="/data/mail/attachments/"+m.id;disk_.mkdir(directory);
  auto destination=directory+"/out-"+sha256(source).substr(0,20);
  for(const auto &a:m.attachments)if(a.path==destination){finish("Arquivo já anexado");return;}
  auto temporary=destination+".part";bool ok=disk_.write(temporary,"");
  for(uint64_t offset=0;ok&&offset<size;){std::string bytes;size_t n=std::min<uint64_t>(4096,size-offset);ok=!cancel_&&disk_.readRange(source,offset,n,bytes)&&bytes.size()==n&&disk_.append(temporary,bytes);offset+=n;}
  std::string extra;if(ok)ok=disk_.readRange(source,size,1,extra)&&extra.empty();
  if(ok)ok=disk_.move(temporary,destination);
  if(!ok)disk_.remove(temporary);
  if(ok){
   std::lock_guard<std::mutex> lock(mutex_);
   MailMessage latest;
   ok=readStored(m.id,latest) && latest.folder=="drafts" && latest.account==m.account;
   uint64_t total=size;
   if(ok)for(const auto &a:latest.attachments){total+=a.size;if(a.path==destination)ok=false;}
   ok=ok && latest.attachments.size()<4 && total<=2*1024*1024 && !cancel_;
   if(ok){latest.attachments.push_back({safeName(source.substr(source.find_last_of('/')+1)),"application/octet-stream",destination,size});ok=put(latest);}
  }
  if(!ok)disk_.remove(destination);
  finish(ok?"Arquivo anexado":"Falha ao copiar anexo; confira espaço do SD");
 })){error="E-mail ocupado";return false;}
 return true;
}
bool Mail::detach(const std::string &id,unsigned index,std::string &error){
 MailMessage m;if(!read(id,m)||m.folder!="drafts"||index>=m.attachments.size()){error="Anexo indisponível";return false;}
 std::lock_guard<std::mutex> lock(mutex_);if(busy_){error="E-mail ocupado";return false;}
 auto path=m.attachments[index].path;m.attachments.erase(m.attachments.begin()+index);if(!put(m))return false;
 if(path.rfind("/data/mail/attachments/"+id+"/",0)==0)disk_.remove(path);
 return true;
}
bool Mail::exportAttachment(const std::string &id,unsigned index,std::string &error){
 MailMessage m;if(!read(id,m)||index>=m.attachments.size()){error="Anexo indisponível";return false;}
 const auto a=m.attachments[index];if(a.path.rfind("/data/mail/attachments/"+id+"/",0)!=0||!Storage::validPath(a.path)){error="Caminho de anexo inválido";return false;}
 const std::string destination="/home/Downloads/"+id.substr(0,8)+"-"+std::to_string(index+1)+"-"+safeName(a.name);
 if(!start([this,a,destination]{disk_.mkdir("/home/Downloads");bool ok=disk_.copy(a.path,destination,&cancel_);finish(ok?"Anexo salvo: "+destination:"Falha ou arquivo já existente: "+destination);})){error="E-mail ocupado";return false;}
 return true;
}
}
