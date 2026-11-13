// SPDX-License-Identifier: MIT
#include "mail_document.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
namespace tab5::mail {
namespace {
std::string lower(std::string s){for(auto &c:s)c=std::tolower(static_cast<unsigned char>(c));return s;}
std::string codepoint(unsigned n){
 if(!n || n>0x10ffff || (n>=0xd800&&n<=0xdfff))return "\xef\xbf\xbd";
 if(n<128)return std::string(1,char(n));
 if(n<2048)return {char(0xc0|(n>>6)),char(0x80|(n&63))};
 if(n<65536)return {char(0xe0|(n>>12)),char(0x80|((n>>6)&63)),char(0x80|(n&63))};
 return {char(0xf0|(n>>18)),char(0x80|((n>>12)&63)),char(0x80|((n>>6)&63)),char(0x80|(n&63))};
}
std::map<std::string,std::string> attributes(const std::string &tag,size_t at){
 std::map<std::string,std::string> out;
 while(at<tag.size()&&out.size()<24){
  while(at<tag.size()&&(isspace((unsigned char)tag[at])||tag[at]=='/'))++at;
  auto start=at;while(at<tag.size()&&(isalnum((unsigned char)tag[at])||tag[at]=='-'||tag[at]==':'))++at;
  if(at==start){++at;continue;}auto name=lower(tag.substr(start,at-start));
  while(at<tag.size()&&isspace((unsigned char)tag[at]))++at;
  std::string value;
  if(at<tag.size()&&tag[at]=='='){
   ++at;while(at<tag.size()&&isspace((unsigned char)tag[at]))++at;
   char quote=at<tag.size()&&(tag[at]=='\''||tag[at]=='"')?tag[at++]:0;
   start=at;while(at<tag.size()&&(quote?tag[at]!=quote:!isspace((unsigned char)tag[at])))++at;
   value=htmlEntityText(tag.substr(start,at-start));if(quote&&at<tag.size())++at;
  }
  out.emplace(name,value);
 }
 return out;
}
}
std::string htmlEntityText(const std::string &text){
 static const std::map<std::string,std::string> names={{"amp","&"},{"lt","<"},{"gt",">"},{"quot","\""},{"apos","'"},{"nbsp"," "},{"ndash","–"},{"mdash","—"},{"hellip","…"},{"copy","©"},{"reg","®"},{"bull","•"},{"aacute","á"},{"eacute","é"},{"iacute","í"},{"oacute","ó"},{"uacute","ú"},{"atilde","ã"},{"otilde","õ"},{"ccedil","ç"},{"Aacute","Á"},{"Ccedil","Ç"}};
 std::string out;
 for(size_t i=0;i<text.size();++i){
  if(text[i]=='&'){
   auto end=text.find(';',i+1);
   if(end!=text.npos&&end-i<16){
    auto key=text.substr(i+1,end-i-1);auto n=names.find(key);
    if(n!=names.end()){out+=n->second;i=end;continue;}
    if(key.size()>1&&key[0]=='#'){
     bool hex=key.size()>2&&(key[1]=='x'||key[1]=='X');auto number=key.substr(hex?2:1);char *last=nullptr;
     auto v=strtoul(number.c_str(),&last,hex?16:10);
     if(!number.empty()&&last&&!*last){out+=codepoint(v>0x10ffff?0:unsigned(v));i=end;continue;}
    }
   }
  }
  if(text[i]!='\0')out+=text[i];
 }
 return out;
}
std::vector<MailBlock> document(const std::string &input){
 const auto html=input.substr(0,96*1024);std::vector<MailBlock> blocks;MailBlock block;
 size_t renderedBytes=0,runs=0;bool limited=false;
 unsigned strong=0,emphasis=0,underline=0,pre=0;std::string href,hidden;unsigned hiddenDepth=0;
 auto flush=[&]{if(!block.runs.empty()&&blocks.size()<96){blocks.push_back(std::move(block));block={};}};
 auto text=[&](const std::string &raw){
  auto s=htmlEntityText(raw);std::string normalized;
  bool space=!block.runs.empty()&&!block.runs.back().text.empty()&&isspace((unsigned char)block.runs.back().text.back());
  for(unsigned char c:s){if(!pre&&isspace(c)){if(!space)normalized+=' ';space=true;}else {normalized+=char(c);space=false;}}
  if(normalized.empty())return;
  // A marketing email can otherwise instantiate thousands of LVGL spans.
  // Keep the rich preview bounded; the original plain text remains available.
  if(runs>=96 || renderedBytes>=16384){limited=true;return;}
  const auto available=std::min(size_t(4096),size_t(16384-renderedBytes));
  if(normalized.size()>available){
    auto end=available;while(end && (static_cast<unsigned char>(normalized[end])&0xc0)==0x80)--end;
    normalized.resize(end);limited=true;
  }
  renderedBytes+=normalized.size();

  if(block.runs.size()>=32){block.runs.back().text+=normalized;return;}
  ++runs;block.runs.push_back({normalized,href,strong>0,emphasis>0,underline>0});
 };
 for(size_t i=0;i<html.size()&&blocks.size()<48&&!limited;){
  if(html.compare(i,4,"<!--")==0){auto e=html.find("-->",i+4);i=e==html.npos?html.size():e+3;continue;}
  if(html[i]!='<'){auto e=html.find('<',i);if(e==html.npos)e=html.size();if(hidden.empty())text(html.substr(i,e-i));i=e;continue;}
  auto end=i+1;char quote=0;
  for(;end<html.size();++end){char c=html[end];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break;}
  if(end==html.size())break;
  auto tag=html.substr(i+1,end-i-1);i=end+1;if(tag.size()>4096)continue;
  size_t at=0;while(at<tag.size()&&isspace((unsigned char)tag[at]))++at;
  bool close=at<tag.size()&&tag[at]=='/';if(close)++at;
  auto start=at;while(at<tag.size()&&(isalnum((unsigned char)tag[at])||tag[at]=='-'))++at;
  auto name=lower(tag.substr(start,at-start));if(name.empty())continue;
  if(!hidden.empty()){if(name==hidden){if(close){if(!--hiddenDepth)hidden.clear();}else ++hiddenDepth;}continue;}
  if(name=="script"||name=="style"||name=="head"||name=="iframe"||name=="object"||name=="svg"||name=="form"||name=="noscript"){
   if(!close){hidden=name;hiddenDepth=1;}continue;
  }
  auto attrs=attributes(tag,at);
  auto css=lower(attrs["style"]);
  if(!close&&(css.find("display:none")!=css.npos||css.find("display: none")!=css.npos||attrs.count("hidden"))){
   if(name!="img"&&name!="br"&&name!="hr"){hidden=name;hiddenDepth=1;}continue;
  }
  if(name=="b"||name=="strong"){if(close){if(strong)--strong;}else ++strong;}
  else if(name=="i"||name=="em"){if(close){if(emphasis)--emphasis;}else ++emphasis;}
  else if(name=="u"){if(close){if(underline)--underline;}else ++underline;}
  else if(name=="a"){href=close?"":attrs["href"];if(href.rfind("http://",0)!=0&&href.rfind("https://",0)!=0&&href.rfind("mailto:",0)!=0)href.clear();}
  else if(name=="img"&&!close){
   flush();MailBlock image;image.source=attrs["src"];image.alt=attrs["alt"];
   if(image.source.size()<=4096&&!image.source.empty())blocks.push_back(std::move(image));
  }else if(name=="br"){text("\n");if(!pre){if(!block.runs.empty())block.runs.back().text+='\n';}}
  else if(name=="td"||name=="th"){if(close)text(" | ");}
  else if(name=="p"||name=="div"||name=="section"||name=="article"||name=="tr"||name=="table"||name=="ul"||name=="ol"||name=="li"||name=="blockquote"||name=="pre"||(name.size()==2&&name[0]=='h'&&name[1]>='1'&&name[1]<='6')){
   flush();if(name=="pre")pre=close?0:1;
   if(!close){block.heading=name[0]=='h'?unsigned(name[1]-'0'):0;block.quote=name=="blockquote";
    block.align=lower(attrs["align"]);if(css.find("text-align:center")!=css.npos||css.find("text-align: center")!=css.npos)block.align="center";else if(css.find("text-align:right")!=css.npos||css.find("text-align: right")!=css.npos)block.align="right";
    if(name=="li")text("• ");}
  }
 }
 flush();if(limited||blocks.size()>=48){MailBlock note;note.runs.push_back({"[Conteúdo visual limitado; use Texto para leitura alternativa]"});blocks.push_back(std::move(note));}
 return blocks;
}
}
