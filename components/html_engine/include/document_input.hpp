// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <stdexcept>
namespace tab5 {
// Tokenize only enough to bound element count and omit inert scripts/comments.
// This is not a security sanitizer; the renderer never executes JavaScript.
inline std::string htmlDocumentInput(const std::string &input,size_t *elementCount=nullptr) {
 constexpr size_t maximumBytes=1024*1024,maximumElements=8192;
 if(input.size()>maximumBytes)throw std::runtime_error("Documento acima de 1 MiB");
 auto lower=[](char c){return c>='A'&&c<='Z'?char(c+32):c;};
 auto letter=[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');};
 auto space=[](char c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f';};
 auto equal=[&](size_t at,const std::string &name){if(at+name.size()>input.size())return false;for(size_t j=0;j<name.size();++j)if(lower(input[at+j])!=name[j])return false;return true;};
 auto tagEnd=[&](size_t at){char quote=0;for(;at<input.size();++at){char c=input[at];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')return at+1;}return input.size();};
 auto rawEnd=[&](size_t at,const std::string &name){for(;;){at=input.find("</",at);if(at==input.npos)return input.size();auto next=at+2+name.size();if(equal(at+2,name)&&next<input.size()&&(space(input[next])||input[next]=='>'||input[next]=='/'))return at;at+=2;}};
 std::string out;out.reserve(input.size()<65536?input.size():65536);size_t at=0,elements=0;
 while(at<input.size()) {
  auto next=input.find('<',at);if(next==input.npos){out.append(input,at,input.size()-at);break;}
  out.append(input,at,next-at);at=next;
  if(input.compare(at,4,"<!--")==0){auto end=input.find("-->",at+4);at=end==input.npos?input.size():end+3;continue;}
  auto start=at+1;bool closing=start<input.size()&&input[start]=='/';if(closing)++start;
  if(start>=input.size()||!letter(input[start])){out+=input[at++];continue;}
  auto nameEnd=start;while(nameEnd<input.size()&&(letter(input[nameEnd])||(input[nameEnd]>='0'&&input[nameEnd]<='9')||input[nameEnd]==':'||input[nameEnd]=='-'||input[nameEnd]=='_'))++nameEnd;
  if(nameEnd<input.size()&&!space(input[nameEnd])&&input[nameEnd]!='>'&&input[nameEnd]!='/'){out+=input[at++];continue;}
  auto end=tagEnd(nameEnd);std::string name=input.substr(start,nameEnd-start);for(auto &c:name)c=lower(c);
  if(!closing&&name=="script") {
   auto stop=rawEnd(end,name);at=stop==input.size()?stop:tagEnd(stop+2+name.size());continue;
  }
  if(!closing&&++elements>maximumElements)throw std::runtime_error("Documento excede 8192 elementos HTML");
  out.append(input,at,end-at);at=end;
  // Raw-text/RCDATA content cannot contain HTML children. Preserve it.
  if(!closing&&name=="plaintext"){out.append(input,at,input.size()-at);break;}
  if(!closing&&(name=="style"||name=="textarea"||name=="title"||name=="xmp"||name=="iframe"||name=="noembed"||name=="noframes")) {auto stop=rawEnd(at,name);out.append(input,at,stop-at);at=stop;}
 }
 if(elementCount)*elementCount=elements;
 return out;
}
}
