// SPDX-License-Identifier: MIT
#include "browser_cookies.hpp"
#include "network.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <limits>
namespace tab5 {
namespace {
std::string lower(std::string s){for(auto &c:s)c=char(std::tolower((unsigned char)c));return s;}
std::string trim(std::string s){auto a=s.find_first_not_of(" \t"),b=s.find_last_not_of(" \t");return a==s.npos?"":s.substr(a,b-a+1);}
std::string host(const std::string &url){auto a=url.find("://");if(a==url.npos)return {};a+=3;return lower(url.substr(a,url.find_first_of("/:?#",a)-a));}
std::string path(const std::string &url){auto a=url.find('/',url.find("://")+3);return a==url.npos?"/":url.substr(a,url.find_first_of("?#",a)-a);}
bool clean(const std::string &s){for(unsigned char c:s)if(c<=32||c>=127||c==';')return false;return true;}
int64_t date(const std::string &s){char month[4]={};int d,y,h,m,sec;if(sscanf(s.c_str(),"%*3s, %d %3s %d %d:%d:%d GMT",&d,month,&y,&h,&m,&sec)!=6||y<1970||y>9999||d<1||d>31||h>23||h<0||m<0||m>59||sec<0||sec>60)return -1;const std::string months="JanFebMarAprMayJunJulAugSepOctNovDec";auto at=months.find(month);if(at==months.npos||at%3)return -1;unsigned mo=unsigned(at/3)+1;y-=mo<=2;int era=y/400;unsigned yy=unsigned(y-era*400),dy=(153*(mo+(mo>2?-3:9))+2)/5+d-1;return (int64_t(era)*146097+yy*365+yy/4-yy/100+dy-719468)*86400+h*3600+m*60+sec;}
}
std::string BrowserCookies::origin(const std::string &u){if(!Network::validUrl(u))return {};auto end=u.find_first_of("/?#",u.find("://")+3);return lower(u.substr(0,end));}
bool BrowserCookies::set(const std::string &url,const std::string &input,int64_t now,bool script){if(!Network::validUrl(url)||input.size()>2048||input.find_first_of("\r\n\t")!=input.npos)return false;
 Cookie c;c.host=host(url);auto requestPath=path(url);auto slash=requestPath.rfind('/');c.path=slash==0?"/":requestPath.substr(0,slash);size_t end=input.find(';');auto pair=input.substr(0,end);auto eq=pair.find('=');if(eq==pair.npos)return false;c.name=trim(pair.substr(0,eq));c.value=trim(pair.substr(eq+1));if(c.name.empty()||c.name.size()>80||c.value.size()>1024||!clean(c.name)||!clean(c.value)||c.name.find_first_of("()<>@,:\\\"/[]?={}")!=c.name.npos)return false;bool age=false;int64_t expires=0;while(end!=input.npos){size_t a=end+1;end=input.find(';',a);auto part=trim(input.substr(a,end==input.npos?end:end-a));auto e=part.find('=');auto key=lower(trim(part.substr(0,e))),v=e==part.npos?"":trim(part.substr(e+1));if(key=="domain")return false;
 if(key=="path"&&!v.empty()&&v[0]=='/'&&v.size()<=256)c.path=v;else if(key=="secure")c.secure=true;else if(key=="httponly")c.httpOnly=true;else if(key=="samesite")c.sameSite=lower(v);else if(key=="max-age"){char *tail=nullptr;auto seconds=strtoll(v.c_str(),&tail,10);if(tail&&tail!=v.c_str()&&!*tail){age=true;c.expiry=seconds<=0?1:now+std::min<int64_t>(seconds,31536000);}}else if(key=="expires")expires=date(v);}
 if(!age&&expires>0){c.expiry=expires;}
 if(script&&c.httpOnly)return false;
 if(c.secure&&url.rfind("https://",0)!=0)return false;
 if(c.sameSite=="none"&&!c.secure)return false;
 if(c.name.rfind("__Secure-",0)==0&&!c.secure)return false;
 if(c.name.rfind("__Host-",0)==0&&(!c.secure||c.path!="/"))return false;
 for(auto it=cookies_.begin();it!=cookies_.end();){if(it->expiry&&it->expiry<=now)it=cookies_.erase(it);else ++it;}
 for(auto it=cookies_.begin();it!=cookies_.end();++it)if(it->host==c.host&&it->path==c.path&&it->name==c.name){if((script&&it->httpOnly)||(it->secure&&url.rfind("https://",0)!=0))return false;cookies_.erase(it);break;}
 if(c.expiry&&c.expiry<=now){return true;}
 if(cookies_.size()>=64){cookies_.erase(cookies_.begin());}
 cookies_.push_back(std::move(c));return true;}
std::string BrowserCookies::header(const std::string &url,int64_t now,bool script,const std::string &initiator,bool safe)const{auto h=host(url),p=path(url);bool secure=url.rfind("https://",0)==0,cross=!initiator.empty()&&origin(initiator)!=origin(url);std::vector<const Cookie*> found;for(const auto &c:cookies_)if(c.host==h&&(!c.expiry||c.expiry>now)&&(!c.secure||secure)&&(!script||!c.httpOnly)&&p.rfind(c.path,0)==0&&(c.path.back()=='/'||p.size()==c.path.size()||p[c.path.size()]=='/')&&(!cross||(c.sameSite!="strict"&&(safe||c.sameSite=="none"))))found.push_back(&c);std::stable_sort(found.begin(),found.end(),[](auto a,auto b){return a->path.size()>b->path.size();});std::string out;for(auto *c:found){if(out.size()+c->name.size()+c->value.size()+3>4096)break;if(!out.empty())out+="; ";out+=c->name+"="+c->value;}return out;}
std::string BrowserCookies::save()const{std::string s="T5C1\n";for(const auto &c:cookies_)s+=c.host+'\t'+c.path+'\t'+c.name+'\t'+c.value+'\t'+std::to_string(c.expiry)+'\t'+(c.secure?"1":"0")+'\t'+(c.httpOnly?"1":"0")+'\t'+c.sameSite+'\n';return s;}
bool BrowserCookies::load(const std::string &s,int64_t now){cookies_.clear();if(s.rfind("T5C1\n",0)!=0||s.size()>100000)return false;size_t a=5;while(a<s.size()){auto end=s.find('\n',a);if(end==s.npos)return false;auto line=s.substr(a,end-a);a=end+1;std::vector<std::string> v;size_t i=0;for(;;){auto t=line.find('\t',i);v.push_back(line.substr(i,t==line.npos?t:t-i));if(t==line.npos)break;i=t+1;}if(v.size()!=8||v[0].empty()||v[1].empty()||v[1][0]!='/'||!clean(v[0])||v[0].find_first_of("/:?#@")!=v[0].npos)return false;std::string header=v[2]+"="+v[3]+"; Path="+v[1];if(v[5]=="1")header+="; Secure";if(v[6]=="1")header+="; HttpOnly";if(!v[7].empty())header+="; SameSite="+v[7];char *tail=nullptr;auto expires=strtoll(v[4].c_str(),&tail,10);if(!tail||tail==v[4].c_str()||*tail||expires<0)return false;
 if(expires&&expires<=now){continue;}
 if(expires){header+="; Max-Age="+std::to_string(expires-now);}
 if(!set(std::string(v[5]=="1"?"https://":"http://")+v[0]+v[1],header,now))return false;}return true;}
}
