// SPDX-License-Identifier: MIT
#include "browser_web.hpp"
#include "duktape.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <algorithm>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
namespace {
struct State {size_t live=0;bool expired=false;std::chrono::steady_clock::time_point deadline;const std::function<bool()> *cancelled=nullptr;};
struct alignas(std::max_align_t) Block{size_t size;};
constexpr size_t budget=1536*1024;
void *alloc(void *u,duk_size_t n){auto &s=*static_cast<State*>(u);if(!n||n>budget-s.live)return nullptr;
#ifdef ESP_PLATFORM
 auto *p=static_cast<Block*>(heap_caps_malloc(n+sizeof(Block),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
 auto *p=static_cast<Block*>(std::malloc(n+sizeof(Block)));
#endif
 if(!p){return nullptr;}
 p->size=n;s.live+=n;return p+1;}
void release(void *u,void *p){if(p){auto *b=static_cast<Block*>(p)-1;static_cast<State*>(u)->live-=b->size;std::free(b);}}
void *resize(void *u,void *p,duk_size_t n){if(!n){release(u,p);return nullptr;}if(!p)return alloc(u,n);auto *old=static_cast<Block*>(p)-1;auto *next=alloc(u,n);if(next){std::memcpy(next,p,std::min(n,old->size));release(u,p);}return next;}
std::string quoted(const std::string &s){std::string out="\"";const char *h="0123456789abcdef";for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=h[c>>4];out+=h[c&15];}else out+=char(c);}return out+'"';}
// All guest execution, including getters during export, remains inside pcall.
const char bootstrap[]=R"JS((function(html,code,url,cookies){
 var original=html, written='', mutations=[], nodes={}, cookieWrites=[];
 function escape(s){return String(s).split('&').join('&amp;').split('<').join('&lt;').split('>').join('&gt;').split('"').join('&quot;');}
 function text(s){var out='',tag=false;for(var i=0;i<s.length;i++){if(s[i]==='<')tag=true;else if(s[i]==='>')tag=false;else if(!tag)out+=s[i];}return out;}
 var document={title:'',URL:url,write:function(s){if(written.length<32768)written+=String(s);},writeln:function(s){this.write(String(s)+'\n');},getElementById:function(id){
  id=String(id);if(Object.prototype.hasOwnProperty.call(nodes,id))return nodes[id];
  var needle='id="'+escape(id)+'"', at=html.indexOf(needle);if(at<0)return null;
  var end=html.indexOf('>',at), start=html.lastIndexOf('<',at), tag=html.slice(start+1,end).split(' ')[0], close=html.indexOf('</'+tag+'>',end);
  if(close<0)return null;
  var node={textContent:text(html.slice(end+1,close)),value:''};nodes[id]=node;mutations.push({node:node,start:end+1,end:close,old:node.textContent});return node;
 }};
 Object.defineProperty(document,'cookie',{get:function(){return cookies;},set:function(v){if(cookieWrites.length<16){v=String(v);if(v.length<=2048)cookieWrites.push(v);}}});
 var window={document:document}, location={href:url};window.window=window;window.location=location;
 var console={log:function(){},warn:function(){},error:function(){}};
 eval(code);
 mutations.sort(function(a,b){return b.start-a.start;});var boundary=html.length;
 for(var i=0;i<mutations.length;i++){var m=mutations[i];if(m.end<=boundary&&m.node.textContent!==m.old){html=html.slice(0,m.start)+escape(String(m.node.textContent).slice(0,8192))+html.slice(m.end);boundary=m.start;}}
 if(written){var cut=html.indexOf('</body>');html=cut<0?html+written:html.slice(0,cut)+written+html.slice(cut);}
 if(document.title)html=html.replace('<body>','<body><h1>'+escape(String(document.title).slice(0,160))+'</h1>');
 return [html,cookieWrites.join('\n')];
}))JS";
struct Run {const char *source;size_t size;std::string *html,*cookies;};
duk_ret_t execute(duk_context *ctx,void *u){auto &r=*static_cast<Run*>(u);duk_eval_lstring(ctx,r.source,r.size);duk_get_prop_index(ctx,-1,0);duk_size_t n;const char *v=duk_get_lstring(ctx,-1,&n);if(v&&n<=65536)r.html->assign(v,n);duk_pop(ctx);duk_get_prop_index(ctx,-1,1);v=duk_get_lstring(ctx,-1,&n);if(v&&n<=32768)r.cookies->assign(v,n);return 0;}
}
extern "C" int tab5_js_timeout(void *u){auto &s=*static_cast<State*>(u);if(s.expired)return 1;s.expired=std::chrono::steady_clock::now()>=s.deadline||(s.cancelled&&*s.cancelled&&(*s.cancelled)());
#ifdef ESP_PLATFORM
 vTaskDelay(1);
#endif
 return s.expired;}
namespace tab5 {
BrowserScriptResult browserScript(const std::string &html,const std::string &script,const std::string &url,const std::string &cookies,const std::function<bool()> &cancelled){BrowserScriptResult out;out.html=html;if(script.empty())return out;if(script.size()>32768||html.size()>65536){out.error="JavaScript excede o limite";return out;}State state;state.deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(300);state.cancelled=&cancelled;auto *ctx=duk_create_heap(alloc,resize,release,&state,nullptr);if(!ctx){out.error="Sem memória para JavaScript";return out;}try{std::string source=std::string(bootstrap)+"("+quoted(html)+","+quoted(script)+","+quoted(url)+","+quoted(cookies)+")";std::string changed,writes;Run run{source.data(),source.size(),&changed,&writes};if(duk_safe_call(ctx,execute,&run,0,0)!=DUK_EXEC_SUCCESS)out.error=state.expired?"JavaScript interrompido por tempo/cancelamento":"JavaScript incompatível ou limite de memória";else if(!changed.empty()){BrowserWeb safe;safe.feed(changed.data(),changed.size());out.html=safe.finish();out.cookies=std::move(writes);out.changed=out.html!=html;}}catch(...){duk_destroy_heap(ctx);throw;}duk_destroy_heap(ctx);return out;}
}
