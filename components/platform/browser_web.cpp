// SPDX-License-Identifier: MIT
#include "browser_web.hpp"
#include "mail_document.hpp"
#include <algorithm>
#include <cctype>
namespace tab5 {
namespace {
std::string lower(std::string s){for(auto &c:s)c=char(std::tolower((unsigned char)c));return s;}
bool space(char c){return c==' '||c=='\n'||c=='\r'||c=='\t'||c=='\f';}
std::string name(const std::string &s){size_t a=s.size()>1&&s[1]=='/'?2:1,b=a;while(b<s.size()&&(std::isalnum((unsigned char)s[b])||s[b]=='-'))++b;return lower(s.substr(a,b-a));}
bool has(const std::string &list,const std::string &key){return list.find("|"+key+"|")!=list.npos;}
std::string css(const std::string &s){
 // Keep declarations in the supported mobile subset. Excludes imports, URLs,
 // generated content, fixed overlays and arbitrarily large geometry/fonts.
 std::string out;size_t at=0;unsigned count=0;
 while(at<s.size()&&count++<32){auto end=s.find(';',at);if(end==s.npos)end=s.size();auto item=s.substr(at,end-at);at=end+1;auto colon=item.find(':');if(colon==item.npos||item.size()>160)continue;auto key=lower(item.substr(0,colon));key.erase(std::remove_if(key.begin(),key.end(),space),key.end());auto value=item.substr(colon+1);auto low=lower(value);
 if(!has("|color|background-color|font-weight|font-style|text-align|text-decoration|display|margin|margin-top|margin-bottom|padding|border|border-color|border-width|",key))continue;
 if(low.find_first_of("{}@<\\")!=low.npos||low.find("url")!=low.npos||low.find("expression")!=low.npos)continue;
 bool large=false;for(size_t i=0;i<value.size();++i)if(std::isdigit((unsigned char)value[i])){unsigned n=0;while(i<value.size()&&std::isdigit((unsigned char)value[i])){n=std::min(1000u,n*10+unsigned(value[i]-'0'));++i;}if(n>64&&key!="color"&&key!="background-color"&&key!="border-color")large=true;}if(!large)out+=key+":"+value+";";
 }return out;
}
std::string stylesheet(const std::string &s){std::string out;size_t at=0;unsigned count=0;while(at<s.size()&&count++<32){auto a=s.find('{',at),b=s.find('}',a);if(a==s.npos||b==s.npos)break;auto selector=s.substr(at,a-at);if(selector.size()<=128&&selector.find_first_of("@{}<>\\") == selector.npos){auto declarations=css(s.substr(a+1,b-a-1));if(!declarations.empty())out+=selector+"{"+declarations+"}";}at=b+1;}return out;}
}
std::string BrowserWeb::escape(const std::string &s){std::string o;for(char c:s){if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\"')o+="&quot;";else if(c)o+=c;}return o;}
std::string BrowserWeb::attribute(const std::string &s,const std::string &key){size_t i=1;while(i<s.size()&&!space(s[i])&&s[i]!='>')++i;while(i<s.size()){while(i<s.size()&&(space(s[i])||s[i]=='/'))++i;size_t a=i;while(i<s.size()&&(std::isalnum((unsigned char)s[i])||s[i]=='-'||s[i]==':'))++i;if(a==i){++i;continue;}auto k=lower(s.substr(a,i-a));while(i<s.size()&&space(s[i]))++i;std::string v;if(i<s.size()&&s[i]=='='){++i;while(i<s.size()&&space(s[i]))++i;char q=i<s.size()&&(s[i]=='\''||s[i]=='"')?s[i++]:0;a=i;while(i<s.size()&&(q?s[i]!=q:!space(s[i])&&s[i]!='>'))++i;if(i-a<=1024)v=mail::htmlEntityText(s.substr(a,i-a));if(q&&i<s.size())++i;}if(k==key)return key=="disabled"?"1":v;}return {};}
void BrowserWeb::append(const std::string &s){if(html_.size()+s.size()<=32768)html_+=s;else clipped_=true;}
void BrowserWeb::consume(){auto n=name(token_);bool closing=token_.size()>1&&token_[1]=='/';
 if(skipDepth_){if(n==skip_){if(closing)--skipDepth_;else if(skipDepth_<128)++skipDepth_;}return;}
 if(!closing&&has("|nav|footer|svg|template|iframe|",n)){skip_=n;skipDepth_=1;return;}
 if(n=="main"){if(!closing&&!main_){main_=true;html_.clear();stack_.clear();nodes_=0;clipped_=false;}else if(closing&&main_)mainDone_=true;}
 if(mainDone_&&n!="main")return;
 if(!closing&&(n=="script"||n=="style"||n=="textarea"||n=="title")){rawName_=n;raw_.clear();rawOverflow_=false;auto type=lower(attribute(token_,"type"));rawScript_=attribute(token_,"src").empty()&&(type.empty()||type=="text/javascript"||type=="application/javascript");return;}
 if(closing){if(n=="form")form_=-1;auto it=std::find(stack_.rbegin(),stack_.rend(),n);if(it!=stack_.rend()){size_t count=size_t(it-stack_.rbegin())+1;while(count--){append("</"+stack_.back()+">");stack_.pop_back();}}return;}
 if(n=="form"){
  form_=-1;if(forms_.size()<8){BrowserForm f;f.action=attribute(token_,"action");f.method=lower(attribute(token_,"method"))=="post"?"POST":"GET";auto encoding=lower(attribute(token_,"enctype"));if(!encoding.empty()&&encoding!="application/x-www-form-urlencoded")return;f.label=attribute(token_,"name");forms_.push_back(std::move(f));form_=int(forms_.size()-1);append("<form action=\""+escape(forms_.back().action)+"\" method=\""+forms_.back().method+"\">");stack_.push_back("form");}return;
 }
 if(n=="input"&&form_>=0&&forms_[form_].fields.size()<12&&attribute(token_,"disabled").empty()){BrowserField f;f.name=attribute(token_,"name");f.value=attribute(token_,"value");f.type=lower(attribute(token_,"type"));if(f.type.empty())f.type="text";if(f.type!="hidden")f.value.erase(std::remove_if(f.value.begin(),f.value.end(),[](char c){return c=='\n'||c=='\r';}),f.value.end());f.label=attribute(token_,"placeholder");if(f.label.empty())f.label=f.name;if(!f.name.empty()&&f.name.size()<=80&&f.value.size()<=1024&&has("|text|password|email|search|hidden|submit|number|",f.type)){append("<input name=\""+escape(f.name)+"\" value=\""+escape(f.value)+"\" type=\""+escape(f.type)+"\" placeholder=\""+escape(f.label)+"\">");forms_[form_].fields.push_back(std::move(f));}return;}
 if(!has("|p|div|span|h1|h2|h3|h4|b|strong|em|i|u|a|img|br|hr|ul|ol|li|blockquote|pre|code|section|article|main|",n))return;
 if(clipped_||nodes_>=256){clipped_=true;return;}if(stack_.size()>=24)return;++nodes_;std::string out="<"+n;
 for(const auto &key:{"id","class","href","src","alt","style"}){auto v=attribute(token_,key);if(v.empty())continue;if(std::string(key)=="style")v=css(v);if((std::string(key)=="href"&&n!="a")||(std::string(key)=="src"&&n!="img"))continue;if(v.size()>1024)continue;out+=" "+std::string(key)+"=\""+escape(v)+"\"";}
 if(n=="img")out+=" width=280 height=160";
 append(out+">");if(n!="img"&&n!="br"&&n!="hr")stack_.push_back(n);
}
void BrowserWeb::feed(const char *p,size_t size){for(size_t i=0;i<size;++i){char c=p[i];
 if(!rawName_.empty()){
  if(raw_.size()<65536)raw_+=c;else {raw_.erase(0,raw_.size()-16);clipped_=true;rawOverflow_=true;}
  auto close="</"+rawName_+">";
  if(raw_.size()>=close.size()&&lower(raw_.substr(raw_.size()-close.size()))==close){raw_.resize(raw_.size()-close.size());if(!rawOverflow_&&rawName_=="style"&&css_.size()+raw_.size()<=8192)css_+=stylesheet(raw_);if(!rawOverflow_&&rawScript_&&rawName_=="script"&&script_.size()+raw_.size()<=32768)script_+=raw_+"\n";rawName_.clear();raw_.clear();}continue;
 }
 if(comment_){token_+=c;if(token_.size()>3)token_.erase(0,token_.size()-3);if(token_=="-->"){comment_=false;token_.clear();}continue;}
 if(tag_){if(token_.size()<4096)token_+=c;else clipped_=true;if(token_=="<!--"){tag_=false;comment_=true;continue;}if(quote_){if(c==quote_)quote_=0;}else if(c=='\''||c=='"')quote_=c;else if(c=='>'){tag_=false;consume();token_.clear();}continue;}
 if(c=='<'){tag_=true;quote_=0;token_="<";}else if(!clipped_&&!skipDepth_&&!mainDone_){if(html_.size()<32768){if(c)html_+=c;}else clipped_=true;}
 }}
std::string BrowserWeb::finish(){while(!stack_.empty()){if(html_.size()+stack_.back().size()+3<=33792)html_+="</"+stack_.back()+">";stack_.pop_back();}return "<html><head><style>"+css_+"</style></head><body>"+html_+"</body></html>";}
std::string BrowserWeb::formData(const BrowserForm &f,const std::vector<std::string> &values){auto enc=[](const std::string &s){const char *h="0123456789ABCDEF";std::string out;for(unsigned char c:s){if(std::isalnum(c)||c=='-'||c=='_'||c=='.'||c=='*')out+=char(c);else if(c==' ')out+='+';else{out+='%';out+=h[c>>4];out+=h[c&15];}}return out;};std::string out;size_t i=0;bool submitted=false;for(const auto &v:f.fields){if(v.type=="submit"){if(submitted)continue;submitted=true;}std::string value=(v.type=="hidden"||v.type=="submit")?v.value:i<values.size()?values[i++]:"";if(!out.empty())out+='&';out+=enc(v.name)+"="+enc(value);}return out;}
}
