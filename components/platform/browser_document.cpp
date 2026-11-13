// SPDX-License-Identifier: MIT
#include "browser_document.hpp"
#include "mail_document.hpp"
#include <algorithm>
#include <cctype>
namespace tab5 {
namespace {
char lower(char c){return c>='A'&&c<='Z'?char(c+32):c;}
bool space(char c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f';}
std::string clipped(std::string s,size_t size){if(s.size()>size){while(size&&(static_cast<unsigned char>(s[size])&0xc0)==0x80)--size;s.resize(size);}return s;}
std::string utf8(const std::string &s){std::string out;for(size_t i=0;i<s.size();){unsigned char c=s[i];size_t n=c<128?1:c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;bool valid=n&&i+n<=s.size();for(size_t j=1;valid&&j<n;++j)valid=(static_cast<unsigned char>(s[i+j])&0xc0)==0x80;if(valid&&n>=3){auto b=static_cast<unsigned char>(s[i+1]);valid=!(c==0xe0&&b<0xa0)&&!(c==0xed&&b>=0xa0)&&!(c==0xf0&&b<0x90)&&!(c==0xf4&&b>=0x90);}if(valid&&c){out.append(s,i,n);i+=n;}else{out+="\xef\xbf\xbd";++i;}}return out;}
bool safeUrl(const std::string &s){if(s.empty()||s.size()>1024||utf8(s)!=s)return false;for(unsigned char c:s)if(c<=32||c==127||c=='\\')return false;auto colon=s.find(':');if(colon==s.npos||colon>s.find_first_of("/?#"))return true;auto scheme=s.substr(0,colon);for(auto &c:scheme)c=lower(c);return scheme=="http"||scheme=="https";}
std::string attr(const std::string &tag,const std::string &wanted){
 size_t i=1;while(i<tag.size()&&!space(tag[i])&&tag[i]!='>')++i;
 while(i<tag.size()){while(i<tag.size()&&(space(tag[i])||tag[i]=='/'))++i;auto start=i;while(i<tag.size()&&(std::isalnum(static_cast<unsigned char>(tag[i]))||tag[i]=='-'||tag[i]==':'))++i;if(i==start){++i;continue;}auto key=tag.substr(start,i-start);for(auto &c:key)c=lower(c);while(i<tag.size()&&space(tag[i]))++i;std::string value;
  if(i<tag.size()&&tag[i]=='='){++i;while(i<tag.size()&&space(tag[i]))++i;char q=i<tag.size()&&(tag[i]=='\''||tag[i]=='"')?tag[i++]:0;start=i;while(i<tag.size()&&(q?tag[i]!=q:!space(tag[i])&&tag[i]!='>'))++i;if(i-start<=1024)value=mail::htmlEntityText(tag.substr(start,i-start));if(q&&i<tag.size())++i;}
  if(key==wanted)return value;
 }return {};
}
std::string encode(const std::string &s){static const char hex[]="0123456789ABCDEF";std::string out;for(unsigned char c:s){if(c=='%'||c=='\n'||c=='\r'||c=='\t'){out+='%';out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out;}
bool decode(const std::string &s,std::string &out,size_t limit){out.clear();auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';c=lower(c);return c>='a'&&c<='f'?c-'a'+10:-1;};for(size_t i=0;i<s.size();++i){char c=s[i];if(c=='%'){if(i+2>=s.size())return false;int a=hex(s[i+1]),b=hex(s[i+2]);if(a<0||b<0)return false;c=char((a<<4)|b);i+=2;}if(!c||out.size()>=limit)return false;out+=c;}return utf8(out)==out;}
}
void BrowserDocument::resetContent(){data_.text.clear();data_.links.clear();data_.truncated=false;breaks_=0;anchor_=false;href_.clear();label_.clear();}
void BrowserDocument::line(){if(!data_.text.empty()&&!space(data_.text.back())&&data_.text.size()<32768){if(breaks_<192){data_.text+='\n';++breaks_;}else data_.text+=' ';}}
void BrowserDocument::closeLink(){if(anchor_&&!href_.empty()&&data_.links.size()<128)data_.links.push_back({href_,label_.empty()?href_:label_});anchor_=false;href_.clear();label_.clear();}
void BrowserDocument::text(const std::string &raw){if(head_||skipDepth_||mainDone_||data_.truncated)return;auto decoded=utf8(mail::htmlEntityText(raw));for(size_t i=0;i<decoded.size();++i){unsigned char c=decoded[i];size_t width=c<128?1:(c&0xc0)==0x80?0:c<0xe0?2:c<0xf0?3:4;if(width&&data_.text.size()+width>32768){data_.truncated=true;break;}if(c<32&&c!='\t'&&c!='\r'&&c!='\n')continue;if(space(char(c))){if(data_.text.empty()||space(data_.text.back()))continue;c=' ';}data_.text+=char(c);if(anchor_&&label_.size()<164)label_+=char(c);}}
void BrowserDocument::flush(bool final){size_t cut=text_.size();if(!final){auto amp=text_.rfind('&');if(amp!=text_.npos&&text_.size()-amp<16&&text_.find(';',amp)==text_.npos)cut=amp;size_t end=cut;while(end&&(static_cast<unsigned char>(text_[end-1])&0xc0)==0x80)--end;if(end&&static_cast<unsigned char>(text_[end-1])>=0xc0){auto c=static_cast<unsigned char>(text_[end-1]);size_t width=c<0xe0?2:c<0xf0?3:4;if(cut-(end-1)<width)cut=end-1;}}
 if(cut){text(text_.substr(0,cut));text_.erase(0,cut);}}
void BrowserDocument::tag(){
 size_t i=1;bool closing=i<token_.size()&&token_[i]=='/';if(closing)++i;auto start=i;while(i<token_.size()&&(std::isalnum(static_cast<unsigned char>(token_[i]))||token_[i]=='-'||token_[i]==':'))++i;auto name=token_.substr(start,i-start);for(auto &c:name)c=lower(c);if(name.empty())return;
 if(!closing&&(name=="script"||name=="style"||name=="title")){raw_=name;rawPattern_="</"+name;rawMatch_=0;mode_=3;if(name=="title"){title_.clear();titleStopped_=false;}return;}
 if(skipDepth_){if(name==skipTag_){if(closing)--skipDepth_;else if(skipDepth_<128)++skipDepth_;}return;}
 if(!closing&&(name=="nav"||name=="footer"||name=="form"||name=="button"||name=="svg"||name=="template"||name=="iframe")){skipTag_=name;skipDepth_=1;return;}
 if(name=="head"){head_=!closing;return;}if(name=="body"&&!closing){head_=false;if(!main_)resetContent();return;}
 if(name=="main"){if(!closing&&!main_){main_=true;mainDone_=false;head_=false;resetContent();}else if(closing&&main_){closeLink();mainDone_=true;}return;}
 if(discard_||mainDone_)return;
 if(name=="base"&&!closing&&data_.base.empty()){auto url=attr(token_,"href");if(safeUrl(url))data_.base=url;}
 if(head_)return;
 if(name=="a"){closeLink();if(!closing&&data_.links.size()<128){auto url=attr(token_,"href");if(safeUrl(url)){anchor_=true;href_=url;}}}
 if(name=="img"&&!closing)text(attr(token_,"alt"));
 if(name=="br"||name=="p"||name=="div"||name=="li"||name=="article"||name=="section"||name=="tr"||(name.size()==2&&name[0]=='h'&&name[1]>='1'&&name[1]<='6'))line();
}
void BrowserDocument::feed(const char *p,size_t size){for(size_t k=0;k<size;++k){char c=p[k];if(mode_==3){if(raw_=="title"&&!titleStopped_){if(title_.size()<512)title_+=c;else titleStopped_=true;}if(rawMatch_<rawPattern_.size()){if(lower(c)==rawPattern_[rawMatch_])++rawMatch_;else rawMatch_=c=='<'?1:0;}else if(space(c)||c=='>'||c=='/'){if(raw_=="title"&&!titleStopped_&&title_.size()>=rawPattern_.size()+1)title_.resize(title_.size()-rawPattern_.size()-1);token_=rawPattern_;token_+=c;quote_=0;discard_=false;mode_=c=='>'?0:1;raw_.clear();if(c=='>')token_.clear();}else rawMatch_=c=='<'?1:0;continue;}
 if(mode_==2){token_+=c;if(token_.size()>3)token_.erase(0,token_.size()-3);if(token_=="-->"){mode_=0;token_.clear();}continue;}
 if(mode_==1){if(token_.size()<8192)token_+=c;else discard_=true;if(token_.size()==2&&c!='/'&&c!='!'&&c!='?'&&!std::isalpha(static_cast<unsigned char>(c))){mode_=0;text(token_);token_.clear();continue;}if(token_=="<!--"){mode_=2;token_.clear();continue;}if(quote_){if(c==quote_)quote_=0;}else if(c=='\''||c=='"')quote_=c;else if(c=='>'){mode_=0;tag();token_.clear();}continue;}
 if(c=='<'){flush(true);mode_=1;token_="<";quote_=0;discard_=false;}else {text_+=c;if(text_.size()>=4096)flush(false);}
 }flush(false);}
BrowserDocumentData BrowserDocument::finish(){flush(true);closeLink();data_.title=clipped(utf8(mail::htmlEntityText(title_)),160);for(auto &c:data_.title)if(space(c))c=' ';for(auto &link:data_.links)link.label=clipped(utf8(link.label),160);return data_;}
std::string BrowserDocument::record(const std::string &url,const BrowserDocumentData &d){std::string out=url+"\nT5B2\nT\t"+encode(d.title)+"\nB\t"+encode(d.base)+"\nP\t"+encode(d.text)+"\nX\t"+(d.truncated?"1":"0")+"\n";if(!d.markup.empty())out+="H\t"+encode(utf8(d.markup))+"\n";if(!d.scriptError.empty())out+="J\t"+encode(d.scriptError)+"\n";for(const auto &l:d.links)out+="L\t"+encode(l.url)+"\t"+encode(l.label)+"\n";return out;}
bool BrowserDocument::readRecord(const std::string &record,std::string &url,BrowserDocumentData &d){d={};auto cut=record.find('\n');if(cut==record.npos||cut>1024||record.compare(cut+1,5,"T5B2\n")!=0)return false;url=record.substr(0,cut);unsigned fields=0;size_t at=cut+6;while(at<record.size()){auto end=record.find('\n',at);if(end==record.npos)return false;auto line=record.substr(at,end-at);at=end+1;if(line.size()<2||line[1]!='\t')return false;auto value=line.substr(2);unsigned bit=0;bool ok=false;switch(line[0]){case 'T':bit=1;ok=decode(value,d.title,160);break;case 'B':bit=2;ok=decode(value,d.base,1024);break;case 'P':bit=4;ok=decode(value,d.text,32768);break;case 'X':bit=8;ok=value=="0"||value=="1";d.truncated=value=="1";break;case 'H':bit=16;ok=decode(value,d.markup,65536);break;case 'J':bit=32;ok=decode(value,d.scriptError,256);break;case 'L':{auto sep=value.find('\t');if(sep==value.npos||d.links.size()>=128)return false;BrowserLink l;ok=decode(value.substr(0,sep),l.url,1024)&&safeUrl(l.url)&&decode(value.substr(sep+1),l.label,160);if(ok)d.links.push_back(std::move(l));break;}default:return false;}if(!ok||(bit&&(fields&bit)))return false;fields|=bit;}return (fields&15)==15;}
}
