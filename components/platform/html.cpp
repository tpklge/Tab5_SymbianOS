#include "html_allocation_scope.hpp"
// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "http_fetch.hpp"
#include "browser_profile.hpp"
#include "browser_document.hpp"
#include "browser_web.hpp"
#include "browser_cookies.hpp"
#include <climits>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-__LONG_LONG_MAX__ - 1LL)
#define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)
#endif
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
namespace tab5 {
namespace {
std::string field(lua_State *L,const char *name,size_t maximum){lua_getfield(L,-1,name);size_t n=0;const char *p=lua_tolstring(L,-1,&n);std::string out=p&&n<=maximum?std::string(p,n):"";lua_pop(L,1);return out;}
std::string resolve(const std::string &url,const std::string &base){if(Network::validUrl(url))return url;std::string resolved,error;if(!base.empty()&&resolveHttpRedirect(base,url,resolved,error)&&Network::validUrl(resolved))return resolved;return {};}
}
bool Runtime::prepareHtml(lua_State *L,View &view){
 HtmlAllocationScope allocation;
 try {
 const auto id=field(L,"mailId",24);const bool email=!id.empty();const bool mobile=current_=="browser"&&!email;
 if(!capability(email?"mail":"network")){error="HTML capability denied";return false;}
 MailMessage message;HtmlSource source;
 if(email){if(!readEventMail(id,message)){view.layout="diagnostic";view.text="Mensagem não disponível";return true;}source.html=message.html;}
 else {
  auto path=field(L,"htmlPath",240);
  if(mobile&&path.empty()){view.layout="diagnostic";view.readonly=true;view.refreshMs=0;view.text="Atualize a pasta SymbianOS junto com o BIN para usar o navegador seguro.";return true;}
  if(!path.empty()) {
   if(path.rfind("/data/browser/cache/",0)!=0 || !permission(path,false) || !store_.read(path,source.html,1024*1024+1025))return false;
   auto cut=source.html.find('\n');if(cut==source.html.npos||cut>1024)return false;
   source.base=source.html.substr(0,cut);
   if(mobile){
    htmlLast_={};
    BrowserDocumentData document;std::string resolved;
    if(!BrowserDocument::readRecord(source.html,resolved,document)){view.layout="diagnostic";view.readonly=true;view.text="Cache antigo ou inválido. Recarregue esta página.";view.refreshMs=0;return true;}
    source.base=resolved;if(!document.base.empty()){auto absolute=resolve(document.base,source.base);if(!absolute.empty())source.base=absolute;}
    htmlLast_={};for(const auto &link:document.links){auto url=resolve(link.url,source.base);if(!url.empty())htmlLast_.links.push_back({url,link.label});}
    std::string interrupted;bool blocked=store_.read("/data/browser/render-pending.txt",interrupted,128)&&interrupted==sha256(document.markup+"\n"+source.base);
    if(blocked){std::string trace;if(store_.read("/logs/browser-render.txt",trace,4096)&&trace.find("stage=native-reader")==trace.npos)store_.write("/logs/browser-fault.txt",trace);view.body+=" · HTML anterior interrompido; leitura alternativa";}
    if(field(L,"htmlMode",16)!="rich"||document.markup.empty()||blocked){
    std::string text=document.title.empty()?document.text:document.title+"\n\n"+document.text;
    if(text.empty())text="Esta página não forneceu texto legível. Use Links ou outro endereço.";
    std::vector<std::string> pages;for(size_t at=0;at<text.size();){size_t end=std::min(at+4096,text.size());while(end<text.size()&&end>at&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;pages.push_back(text.substr(at,end-at));at=end;}
    lua_getfield(L,-1,"htmlPage");auto requested=lua_tointeger(L,-1);lua_pop(L,1);htmlLast_.pages=pages.size();htmlLast_.page=std::min(unsigned(std::max<lua_Integer>(0,requested)),htmlLast_.pages-1);
    view.documentLinks=htmlLast_.links;html_.reset();
    view.layout="reader";view.field=true;view.readonly=true;view.refreshMs=0;view.picture.reset();view.text=pages[htmlLast_.page];
    view.body+=" · Leitura segura "+std::string(browserProfile::build)+" · "+std::to_string(htmlLast_.page+1)+" / "+std::to_string(htmlLast_.pages);if(document.truncated)view.body+=" · trecho da página";
    try{store_.write("/logs/browser-render.txt","runtime=1.0.0\napp=browser\nbuild="+std::string(browserProfile::build)+"\nstage=native-reader\ntext_bytes="+std::to_string(text.size())+"\n");}catch(...){}
    return true;
    }
    source.html=std::move(document.markup);
    if(!document.scriptError.empty())view.body+=" · "+document.scriptError;
   }
   if(!mobile)source.html.erase(0,cut+1);
  } else source.html=field(L,"html",96*1024);
 }
 if(!mobile){BrowserWeb bounded;bounded.feed(source.html.data(),source.html.size());source.html=bounded.finish();}
 if(source.base.empty())source.base=field(L,"baseUrl",4096);
 lua_getfield(L,-1,"htmlPage");if(lua_isinteger(L,-1))source.page=std::max<lua_Integer>(0,std::min<lua_Integer>(255,lua_tointeger(L,-1)));lua_pop(L,1);
 lua_getfield(L,-1,"htmlRevision");auto revision=lua_tointeger(L,-1);lua_pop(L,1);
 auto nativeFallback=[&](const std::string &reason){
  BrowserDocument parser;parser.feed(source.html.data(),source.html.size());auto doc=parser.finish();html_.reset();htmlLast_={};
  for(const auto &link:doc.links){auto url=resolve(link.url,source.base);if(!url.empty())htmlLast_.links.push_back({url,link.label});}
  auto text=email&&!message.body.empty()?message.body:doc.text;if(text.empty())text="Conteúdo indisponível em texto.";
  std::vector<std::string> pages;for(size_t at=0;at<text.size();){size_t end=std::min(at+4096,text.size());while(end<text.size()&&end>at&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;pages.push_back(text.substr(at,end-at));at=end;}
  htmlLast_.pages=pages.size();htmlLast_.page=std::min(source.page,htmlLast_.pages-1);view.layout="reader";view.field=true;view.readonly=true;view.picture.reset();view.htmlHotspots.clear();view.documentLinks=htmlLast_.links;view.text=pages[htmlLast_.page];view.refreshMs=0;view.body+=" · "+reason+" · "+std::to_string(htmlLast_.page+1)+" / "+std::to_string(htmlLast_.pages);return true;
 };
 const auto guardPath=mobile?std::string("/data/browser/render-pending.txt"):"/logs/html-render-pending-"+current_+".txt";
 const auto contentKey=sha256(source.html+"\n"+source.base);std::string previousGuard;
 if(store_.read(guardPath,previousGuard,128)&&previousGuard==contentKey)return nativeFallback("HTML anterior interrompido; leitura alternativa");
#ifdef ESP_PLATFORM
 if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)<48*1024||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)<16*1024)return nativeFallback("Memória interna reservada; leitura alternativa");
#endif
 if(htmlFont_.empty()&&!asset("theme.ttf",htmlFont_,2*1024*1024))return false;
 if(htmlCss_.empty()&&!asset("html-default.css",htmlCss_,65536))return false;
 source.font=htmlFont_;source.masterCss=htmlCss_;source.width=mobile?browserProfile::width:email?1108:1184;source.height=mobile?browserProfile::height:352;
 source.key=sha256(source.html+"\n"+source.base+"\n"+std::to_string(revision));
 source.cancelled=[this]{return operationCancelled();};source.resolve=resolve;
 source.phase=[this,bytes=source.html.size()](const char *stage){
#ifdef ESP_PLATFORM
  try {
   const auto prior=store_.lastError();
   std::string record="runtime=1.0.0\nbuild="+std::string(browserProfile::build)+"\napp="+current_+"\nstage="+stage+"\nhtml_bytes="+std::to_string(bytes)+
    "\ninternal_free="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL))+
    "\ninternal_largest="+std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))+
    "\npsram_free="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))+
    "\nworker_stack_free_bytes="+std::to_string(uxTaskGetStackHighWaterMark(nullptr))+"\n";
   store_.write("/logs/browser-render.txt",record);store_.restoreDiagnosticError(prior);
  }catch(...){/* Diagnostics cannot turn a recoverable render error into a reset. */}
#else
  (void)stage;(void)bytes;
#endif
 };
 source.resource=[this,mobile](const std::string &url,const std::string &base,bool css){std::string out;auto absolute=resolve(url,base);if(!absolute.empty())store_.read(Network::imageCachePath(absolute,mobile),out,css?65536:256*1024);return out;};
 auto images=std::make_shared<std::vector<MailImage>>(std::move(message.images));auto decodedBytes=std::make_shared<size_t>(0);
 source.image=[this,images,decodedBytes,mobile](const std::string &url,const std::string &base)->std::shared_ptr<const Picture>{
  if(*decodedBytes>=4*1024*1024){return {};}
  std::string encoded;
  if(url.rfind("cid:",0)==0){auto cid=url.substr(4);for(const auto &image:*images)if(image.cid==cid){encoded=image.data;break;}}
  else if(url.rfind("data:image/",0)==0){auto at=url.find(',');if(at<80&&url.substr(0,at).find(";base64")!=url.npos&&url.size()<768*1024)encoded=mail::unbase64(url.substr(at+1));}
  else{auto absolute=resolve(url,base);if(!absolute.empty())store_.read(Network::imageCachePath(absolute,mobile),encoded,256*1024);}
  std::shared_ptr<const Picture> picture;if(!encoded.empty()&&media_.imageData(encoded,2,&picture)){if(*decodedBytes+picture->pixels.size()<=4*1024*1024){*decodedBytes+=picture->pixels.size();return picture;}}return {};
 };
 if(!store_.write(guardPath,contentKey))return nativeFallback("Sem marcador de recuperação; leitura alternativa");
 htmlLast_=html_.render(source);store_.remove(guardPath);
 if(!htmlLast_.picture){auto reason=htmlLast_.error;return nativeFallback(reason.empty()?"Leitura alternativa":reason);}
 view.htmlHotspots=htmlLast_.hotspots;
 if(htmlLast_.picture){if(mobile)view.body+=" · Móvel";if(htmlLast_.simplified)view.body+=" · Leitura leve";if(htmlLast_.truncated)view.body+=" · trecho da página";view.picture=htmlLast_.picture;view.body+=(view.body.empty()?"":" · ")+std::to_string(htmlLast_.page+1)+" / "+std::to_string(htmlLast_.pages)+" · HTML/CSS";if(!htmlLast_.pending.empty())view.body+=" · "+std::to_string(htmlLast_.pending.size())+" recursos externos pendentes";}
 else{view.refreshMs=0;view.layout="diagnostic";view.readonly=true;view.text=htmlLast_.error+"\n\n"+(email?message.body:"Use Texto para a leitura alternativa.");}
 return true;
 }catch(const std::bad_alloc &){
  html_.reset();htmlLast_={};view.picture.reset();view.refreshMs=0;view.layout="diagnostic";view.readonly=true;view.text="Memória insuficiente para preparar esta página. Volte e tente outro endereço.";return true;
 }catch(const std::exception &){
  html_.reset();htmlLast_={};view.picture.reset();view.refreshMs=0;view.layout="diagnostic";view.readonly=true;view.text="Não foi possível preparar esta página.";return true;
 }
}
int Runtime::htmlApi(lua_State *L){
 auto *r=static_cast<Runtime *>(lua_touserdata(L,lua_upvalueindex(1)));
 if(!r->capability("mail")&&!r->capability("network"))return luaL_error(L,"HTML capability denied");
 std::string op=luaL_optstring(L,1,"status");
 if(op=="clear_sessions"&&r->current_=="browser"){
  if(r->network_.status().busy){lua_pushboolean(L,false);return 1;}
  std::vector<File> files;bool ok=true;if(r->store_.listDir("/data/browser/cache",files))for(const auto &f:files)if(!f.directory)ok=r->store_.remove(f.path)&&ok;
  files.clear();if(r->store_.listDir("/data/browser/assets",files))for(const auto &f:files)if(!f.directory)ok=r->store_.remove(f.path)&&ok;
  std::string saved;if(r->store_.read("/data/browser/cookies.tsv",saved,100000))ok=r->store_.remove("/data/browser/cookies.tsv")&&ok;
  lua_pushboolean(L,ok);return 1;
 }
 if((op=="forms"||op=="submit")&&r->current_=="browser"){
  size_t n=0;const char *arg=luaL_checklstring(L,2,&n);std::string path(arg,n),record,url;BrowserDocumentData doc;
  if(n>240||path.rfind("/data/browser/cache/",0)!=0||!r->permission(path,false)||!r->store_.read(path,record,1024*1024)||!BrowserDocument::readRecord(record,url,doc)){lua_pushnil(L);return 1;}
  BrowserWeb parser;parser.feed(doc.markup.data(),doc.markup.size());const auto &forms=parser.forms();
  auto base=url;if(!doc.base.empty()){auto b=resolve(doc.base,url);if(!b.empty())base=b;}
  if(op=="forms"){
   lua_newtable(L);unsigned index=0;for(const auto &f:forms){auto action=f.action.empty()?url:resolve(f.action,base);lua_newtable(L);lua_pushstring(L,action.c_str());lua_setfield(L,-2,"action");lua_pushstring(L,f.method.c_str());lua_setfield(L,-2,"method");lua_newtable(L);unsigned fi=0;for(const auto &v:f.fields){lua_newtable(L);lua_pushstring(L,v.name.c_str());lua_setfield(L,-2,"name");lua_pushstring(L,v.label.c_str());lua_setfield(L,-2,"label");lua_pushstring(L,v.type.c_str());lua_setfield(L,-2,"type");lua_pushstring(L,v.value.c_str());lua_setfield(L,-2,"value");lua_rawseti(L,-2,++fi);}lua_setfield(L,-2,"fields");lua_rawseti(L,-2,++index);}return 1;
  }
  auto index=luaL_checkinteger(L,3);if(index<1||size_t(index)>forms.size()){lua_pushboolean(L,false);return 1;}const auto &form=forms[size_t(index)-1];std::vector<std::string> values;luaL_checktype(L,4,LUA_TTABLE);
  for(unsigned i=1;i<=12;++i){lua_rawgeti(L,4,i);size_t size=0;const char *value=lua_tolstring(L,-1,&size);if(size>1024){lua_pop(L,1);lua_pushboolean(L,false);return 1;}values.emplace_back(value?std::string(value,size):"");lua_pop(L,1);}
  auto action=form.action.empty()?url:resolve(form.action,base);auto body=BrowserWeb::formData(form,values);std::string profile;
  if(form.method=="GET"){action=action.substr(0,action.find('?'));if(!body.empty())action+="?"+body;body.clear();}
  bool insecurePassword=false;for(const auto &v:form.fields)if(v.type=="password"&&action.rfind("https://",0)!=0)insecurePassword=true;
  bool ok=!insecurePassword&&!(r->store_.read("/config/public/profile",profile)&&profile=="offline")&&!(url.rfind("https://",0)==0&&action.rfind("http://",0)==0)&&r->network_.browse(action,form.method,body,url);
  lua_pushboolean(L,ok);lua_pushstring(L,action.c_str());return 2;
 }

 if(op=="cancel"){r->network_.cancel();lua_pushboolean(L,true);return 1;}
 if(op=="resources"){
  
  std::string profile;r->store_.read("/config/public/profile",profile);std::vector<std::pair<std::string,bool>> requests;
  for(const auto &asset:r->htmlLast_.pending)if(Network::validUrl(asset.url))requests.emplace_back(asset.url,asset.css);
  bool ok=profile!="offline"&&r->network_.documentAssets(requests,r->current_=="browser");lua_pushboolean(L,ok);return 1;
 }
 lua_newtable(L);
 lua_newtable(L);size_t index=0;for(const auto &link:r->htmlLast_.links){lua_newtable(L);lua_pushlstring(L,link.url.data(),link.url.size());lua_setfield(L,-2,"url");lua_pushlstring(L,link.label.data(),link.label.size());lua_setfield(L,-2,"label");lua_rawseti(L,-2,++index);}lua_setfield(L,-2,"links");
 lua_pushinteger(L,r->htmlLast_.pages);lua_setfield(L,-2,"pages");lua_pushinteger(L,r->htmlLast_.page);lua_setfield(L,-2,"page");lua_pushinteger(L,r->htmlLast_.pending.size());lua_setfield(L,-2,"pending");lua_pushstring(L,r->current_=="browser"?(r->htmlLast_.picture?"litehtml limitado":"leitor nativo"):"litehtml 0.10");lua_setfield(L,-2,"engine");return 1;
}
}
