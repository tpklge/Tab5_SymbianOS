#include "readable_document.hpp"
#include "document_input.hpp"
// SPDX-License-Identifier: MIT
#include "html_engine.hpp"
#include "font_memory.hpp"
#include "litehtml.h"
#include "litehtml/render_item.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <cstring>
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
extern "C" bool tab5_html_alloc_scope(bool);
extern "C" void *tab5_html_malloc(size_t);
extern "C" void tab5_html_free(void *);
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(n,u) tab5::htmlFontAllocate(n,u)
#define STBTT_free(p,u) tab5::HtmlFontMemory::release(p)
#include "stb_truetype_htcw.h"
namespace tab5 {
namespace {
using namespace litehtml;
using Clock=std::chrono::steady_clock;
struct AllocScope {bool old=tab5_html_alloc_scope(true);~AllocScope(){tab5_html_alloc_scope(old);}};
struct Font {int size=20,weight=400,decoration=0;bool italic=false;float scale=1;int ascent=0,descent=0;};
struct Glyph {int width=0,height=0,xoff=0,yoff=0;std::vector<unsigned char> alpha;};
struct Clip {position box;border_radiuses radius;};
unsigned nextCodepoint(const char *text,size_t &at){
 unsigned c=static_cast<unsigned char>(text[at++]);if(c<128)return c;
 unsigned bytes=c<0xe0?2:c<0xf0?3:4;unsigned out=c&((1<<(7-bytes))-1);
 for(unsigned i=1;i<bytes;++i){auto b=static_cast<unsigned char>(text[at]);if((b&0xc0)!=0x80)return 0xfffd;out=(out<<6)|(b&63);++at;}return out;
}
web_color mix(web_color a,web_color b,float t){t=std::max(0.f,std::min(1.f,t));return {byte(a.red+(b.red-a.red)*t),byte(a.green+(b.green-a.green)*t),byte(a.blue+(b.blue-a.blue)*t),byte(a.alpha+(b.alpha-a.alpha)*t)};}
bool inRounded(float x,float y,const position &p,const border_radiuses &r){
 if(x<p.x||y<p.y||x>=p.right()||y>=p.bottom())return false;
 float rx=0,ry=0,cx=0,cy=0;
 if(x<p.x+r.top_left_x&&y<p.y+r.top_left_y){rx=r.top_left_x;ry=r.top_left_y;cx=p.x+rx;cy=p.y+ry;}
 else if(x>p.right()-r.top_right_x&&y<p.y+r.top_right_y){rx=r.top_right_x;ry=r.top_right_y;cx=p.right()-rx;cy=p.y+ry;}
 else if(x<p.x+r.bottom_left_x&&y>p.bottom()-r.bottom_left_y){rx=r.bottom_left_x;ry=r.bottom_left_y;cx=p.x+rx;cy=p.bottom()-ry;}
 else if(x>p.right()-r.bottom_right_x&&y>p.bottom()-r.bottom_right_y){rx=r.bottom_right_x;ry=r.bottom_right_y;cx=p.right()-rx;cy=p.bottom()-ry;}
 if(rx>0&&ry>0){float dx=(x-cx)/rx,dy=(y-cy)/ry;return dx*dx+dy*dy<=1;}return true;
}
class Container final:public document_container {
public:
 HtmlSource source;HtmlFontMemory fontMemory;stbtt_fontinfo ttf{};std::map<unsigned,Font> fonts;
 std::map<uint64_t,Glyph> glyphs;std::map<std::string,std::shared_ptr<const Picture>> images;
 std::vector<Clip> clips;std::vector<HtmlAsset> pending;std::set<std::string> requested;
 std::shared_ptr<Picture> canvas;unsigned paintedPage=~0u;document::ptr doc;Clock::time_point deadline,lastYield;unsigned pulses=0;
 explicit Container(const HtmlSource &s,std::string input):source(s){
  source.html=std::move(input);
  if(source.font.empty()||!stbtt_InitFont(&ttf,reinterpret_cast<const unsigned char*>(source.font.data()),0))throw std::runtime_error("Fonte do cartão inválida");
  ttf.userdata=&fontMemory;
 }
 void begin(){deadline=Clock::now()+std::chrono::seconds(12);lastYield=Clock::now();pulses=0;}
 void pulse(){
  if(source.cancelled&&source.cancelled())throw std::runtime_error("Renderização cancelada");
  if((++pulses%64)!=0){return;}
  auto now=Clock::now();if(now>deadline)throw std::runtime_error("Documento excedeu o prazo de renderização; use Texto");
#ifdef ESP_PLATFORM
  if(now-lastYield>std::chrono::milliseconds(20)){vTaskDelay(1);lastYield=Clock::now();}
#endif
 }
 void missing(const std::string &raw,bool css,const std::string &base=""){auto url=source.resolve?source.resolve(raw,base.empty()?source.base:base):raw;if(url.empty()||pending.size()>=16||!requested.insert((css?"c:":"i:")+url).second)return;pending.push_back({url,css});}
 uint_ptr create_font(const font_description &desc,const document*,font_metrics *fm)override{
  pulse();unsigned size=unsigned(std::max(8.f,std::min(96.f,desc.size)));unsigned id=(size<<8)+(desc.weight>=600?1:0)+(desc.style==font_style_italic?2:0)+unsigned(desc.decoration_line)*4;
  auto &f=fonts[id];f.size=size;f.weight=desc.weight;f.italic=desc.style==font_style_italic;f.decoration=desc.decoration_line;f.scale=stbtt_ScaleForMappingEmToPixels(&ttf,float(size));
  int a,d,g;stbtt_GetFontVMetrics(&ttf,&a,&d,&g);f.ascent=int(std::ceil(a*f.scale));f.descent=int(std::ceil(-d*f.scale));
  fm->font_size=size;fm->height=(a-d+g)*f.scale;fm->ascent=f.ascent;fm->descent=f.descent;fm->x_height=size*0.5f;fm->ch_width=size*0.6f;fm->draw_spaces=true;fm->sub_shift=size*.2f;fm->super_shift=size*.35f;
  return reinterpret_cast<uint_ptr>(&f);
 }
 void delete_font(uint_ptr)override{}
 pixel_t text_width(const char *text,uint_ptr handle)override{
  pulse();const auto &f=*reinterpret_cast<Font*>(handle);float width=0;size_t at=0;unsigned previous=0;
  while(text[at]){pulse();unsigned c=nextCodepoint(text,at);int advance,bearing;stbtt_GetCodepointHMetrics(&ttf,c,&advance,&bearing);width+=(advance+stbtt_GetCodepointKernAdvance(&ttf,previous,c))*f.scale;previous=c;}return width;
 }
 void pixel(int x,int y,web_color color){
  if(!color.alpha||x<0||y<0||x>=int(source.width)||y>=int(source.height))return;
  for(const auto &clip:clips)if(!inRounded(x+.5f,y+.5f,clip.box,clip.radius))return;
  size_t at=(size_t(y)*source.width+x)*2;auto *p=reinterpret_cast<unsigned char*>(canvas->pixels.data());
  if(color.alpha<255){unsigned old=p[at]|(unsigned(p[at+1])<<8);web_color before(byte(((old>>11)&31)*255/31),byte(((old>>5)&63)*255/63),byte((old&31)*255/31));color=mix(before,color,color.alpha/255.f);}
  unsigned rgb=((color.red>>3)<<11)|((color.green>>2)<<5)|(color.blue>>3);p[at]=rgb&255;p[at+1]=rgb>>8;
 }
 template<class Color>void paint(const position &box,const border_radiuses &radius,Color color){
  if(!std::isfinite(box.x)||!std::isfinite(box.y)||!std::isfinite(box.width)||!std::isfinite(box.height))return;
  int x0=int(std::floor(std::max(0.f,std::min(float(source.width),box.x)))),y0=int(std::floor(std::max(0.f,std::min(float(source.height),box.y)))),x1=int(std::ceil(std::max(0.f,std::min(float(source.width),box.right())))),y1=int(std::ceil(std::max(0.f,std::min(float(source.height),box.bottom()))));
  for(int y=y0;y<y1;++y){pulse();for(int x=x0;x<x1;++x)if(inRounded(x+.5f,y+.5f,box,radius))pixel(x,y,color(float(x)+.5f,float(y)+.5f));}
 }
 void draw_text(uint_ptr,const char *text,uint_ptr handle,web_color color,const position &pos)override{
  pulse();if(!std::isfinite(pos.x)||!std::isfinite(pos.y)||pos.bottom()<0||pos.y>=source.height||pos.x>=source.width||pos.right()<0)return;const auto &f=*reinterpret_cast<Font*>(handle);float x=pos.x;size_t at=0;unsigned prev=0;
  while(text[at]){pulse();auto c=nextCodepoint(text,at);x+=stbtt_GetCodepointKernAdvance(&ttf,prev,c)*f.scale;prev=c;uint64_t key=(uint64_t(f.size)<<40)|(uint64_t(c));
   auto found=glyphs.find(key);if(found==glyphs.end()){
    if(glyphs.size()>=768){glyphs.clear();}
    Glyph glyph;int x0,y0,x1,y1;stbtt_GetCodepointBitmapBox(&ttf,c,f.scale,f.scale,&x0,&y0,&x1,&y1);glyph.width=std::max(0,x1-x0);glyph.height=std::max(0,y1-y0);glyph.xoff=x0;glyph.yoff=y0;
    if(glyph.width<=192&&glyph.height<=192&&glyph.width>0&&glyph.height>0){glyph.alpha.resize(glyph.width*glyph.height);stbtt_MakeCodepointBitmap(&ttf,glyph.alpha.data(),glyph.width,glyph.height,glyph.width,f.scale,f.scale,c);}found=glyphs.emplace(key,std::move(glyph)).first;
   }
   const auto &g=found->second;if(x>-512&&x<int(source.width)+512)for(int yy=0;yy<g.height&&!g.alpha.empty();++yy)for(int xx=0;xx<g.width;++xx){auto col=color;col.alpha=unsigned(col.alpha)*g.alpha[yy*g.width+xx]/255;int slant=f.italic?(g.height-yy)/5:0;int px=int(x)+g.xoff+xx+slant,py=int(pos.y)+f.ascent+g.yoff+yy;pixel(px,py,col);if(f.weight>=600)pixel(px+1,py,col);}
   int advance,bearing;stbtt_GetCodepointHMetrics(&ttf,c,&advance,&bearing);x+=advance*f.scale;
  }
  if(f.decoration&text_decoration_line_underline)paint(position(pos.x,pos.y+f.ascent+2,pos.width,1),{},[&](float,float){return color;});
  if(f.decoration&text_decoration_line_line_through)paint(position(pos.x,pos.y+f.ascent*.6f,pos.width,1),{},[&](float,float){return color;});
 }
 pixel_t pt_to_px(float pt)const override{return pt*96.f/72.f;}
 pixel_t get_default_font_size()const override{return 20;}
 const char *get_default_font_name()const override{return "sans-serif";}
 void draw_list_marker(uint_ptr,const list_marker &m)override{paint(m.pos,{},[&](float,float){return m.color;});}
 std::string imageKey(const std::string &url,const std::string &base){auto actual=base.empty()?source.base:base;auto absolute=source.resolve?source.resolve(url,actual):url;return absolute.empty()?actual+"\n"+url:absolute;}
 void load_image(const char *url,const char *base,bool)override{
  pulse();if(!url||images.size()>=16)return;std::string actual=base&&*base?std::string(base):source.base;std::string key=imageKey(url,actual);if(images.count(key))return;
  auto picture=source.image?source.image(url,actual):nullptr;if(!picture)missing(url,false,actual);images[key]=picture;
 }
 void get_image_size(const char *url,const char *base,size &out)override{load_image(url,base,false);auto found=images.find(imageKey(url,base&&*base?base:source.base));if(found!=images.end()&&found->second){out.width=found->second->width;out.height=found->second->height;}else {out.width=32;out.height=24;}}
 void draw_image(uint_ptr,const background_layer &layer,const std::string &url,const std::string &base)override{
  load_image(url.c_str(),base.c_str(),false);auto found=images.find(imageKey(url,base));if(found==images.end()||!found->second)return;auto &pic=*found->second;
  clips.push_back({layer.clip_box,{}});const auto origin=layer.origin_box;
  paint(layer.border_box,layer.border_radius,[&](float x,float y){float dx=x-origin.x,dy=y-origin.y;
   bool repeatX=layer.repeat==background_repeat_repeat||layer.repeat==background_repeat_repeat_x,repeatY=layer.repeat==background_repeat_repeat||layer.repeat==background_repeat_repeat_y;
   if(origin.width<=0||origin.height<=0||(!repeatX&&(dx<0||dx>=origin.width))||(!repeatY&&(dy<0||dy>=origin.height)))return web_color(0,0,0,0);
   dx=std::fmod(std::fmod(dx,origin.width)+origin.width,origin.width);dy=std::fmod(std::fmod(dy,origin.height)+origin.height,origin.height);
   auto px=std::min(pic.width-1,unsigned(dx*pic.width/origin.width)),py=std::min(pic.height-1,unsigned(dy*pic.height/origin.height));size_t at=(size_t(py)*pic.width+px)*2;auto *p=reinterpret_cast<const unsigned char*>(pic.pixels.data());unsigned rgb=p[at]|unsigned(p[at+1])<<8;return web_color(byte(((rgb>>11)&31)*255/31),byte(((rgb>>5)&63)*255/63),byte((rgb&31)*255/31),pic.alpha.empty()?255:static_cast<unsigned char>(pic.alpha[size_t(py)*pic.width+px]));});clips.pop_back();
 }
 void draw_solid_fill(uint_ptr,const background_layer &layer,const web_color &color)override{clips.push_back({layer.clip_box,{}});paint(layer.border_box,layer.border_radius,[&](float,float){return color;});clips.pop_back();}
 web_color gradient(const background_layer::gradient_base &g,float t){if(g.color_points.empty())return {0,0,0,0};if(t<=g.color_points.front().offset)return g.color_points.front().color;for(size_t i=1;i<g.color_points.size();++i)if(t<=g.color_points[i].offset){float len=g.color_points[i].offset-g.color_points[i-1].offset;return mix(g.color_points[i-1].color,g.color_points[i].color,len>0?(t-g.color_points[i-1].offset)/len:1);}return g.color_points.back().color;}
 void draw_linear_gradient(uint_ptr,const background_layer &layer,const background_layer::linear_gradient &g)override{clips.push_back({layer.clip_box,{}});paint(layer.border_box,layer.border_radius,[&](float x,float y){float dx=g.end.x-g.start.x,dy=g.end.y-g.start.y,len=dx*dx+dy*dy;return gradient(g,len>0?((x-layer.origin_box.x-g.start.x)*dx+(y-layer.origin_box.y-g.start.y)*dy)/len:0);});clips.pop_back();}
 void draw_radial_gradient(uint_ptr,const background_layer &layer,const background_layer::radial_gradient &g)override{clips.push_back({layer.clip_box,{}});paint(layer.border_box,layer.border_radius,[&](float x,float y){float dx=(x-layer.origin_box.x-g.position.x)/std::max(1.f,g.radius.x),dy=(y-layer.origin_box.y-g.position.y)/std::max(1.f,g.radius.y);return gradient(g,std::sqrt(dx*dx+dy*dy));});clips.pop_back();}
 void draw_conic_gradient(uint_ptr,const background_layer &layer,const background_layer::conic_gradient &g)override{clips.push_back({layer.clip_box,{}});paint(layer.border_box,layer.border_radius,[&](float x,float y){float t=std::atan2(x-layer.origin_box.x-g.position.x,-(y-layer.origin_box.y-g.position.y))*180.f/3.14159265f-g.angle;return gradient(g,std::fmod(t+720.f,360.f)/360.f);});clips.pop_back();}
 void draw_borders(uint_ptr,const borders &b,const position &p,bool)override{
  auto edge=[&](const border &e,position box,bool horizontal){if(e.style==border_style_none||e.width<=0)return;paint(box,{},[&](float x,float y){if(e.style==border_style_dashed||e.style==border_style_dotted){int step=std::max(1,int(e.width*(e.style==border_style_dotted?1:3)));if(int(horizontal?x:y)/step%2)return web_color(0,0,0,0);}return e.color;});};
  clips.push_back({p,b.radius});edge(b.top,{p.x,p.y,p.width,b.top.width},true);edge(b.bottom,{p.x,p.bottom()-b.bottom.width,p.width,b.bottom.width},true);edge(b.left,{p.x,p.y,b.left.width,p.height},false);edge(b.right,{p.right()-b.right.width,p.y,b.right.width,p.height},false);clips.pop_back();
 }
 void set_caption(const char*)override{}
 void set_base_url(const char *url)override{if(url){auto absolute=source.resolve?source.resolve(url,source.base):std::string(url);if(!absolute.empty())source.base=absolute;}}
 void link(const std::shared_ptr<document>&,const element::ptr&)override{}
 void on_anchor_click(const char*,const element::ptr&)override{}
 void on_mouse_event(const element::ptr&,mouse_event)override{}
 void set_cursor(const char*)override{}
 void transform_text(std::string &text,text_transform transform)override{bool start=true;for(char &c:text){if(static_cast<unsigned char>(c)>=128)continue;if(transform==text_transform_uppercase||(transform==text_transform_capitalize&&start))c=std::toupper(static_cast<unsigned char>(c));else if(transform==text_transform_lowercase)c=std::tolower(static_cast<unsigned char>(c));start=std::isspace(static_cast<unsigned char>(c));}}
 void import_css(std::string &text,const std::string &url,std::string &base)override{pulse();auto absolute=source.resolve?source.resolve(url,base.empty()?source.base:base):url;text=source.resource?source.resource(absolute,"",true):"";base=absolute;if(text.empty())missing(absolute,true);}
 void set_clip(const position &p,const border_radiuses &r)override{clips.push_back({p,r});}
 void del_clip()override{if(!clips.empty())clips.pop_back();}
 void get_viewport(position &p)const override{p={0,0,float(source.width),float(source.height)};}
 element::ptr create_element(const char*,const string_map&,const std::shared_ptr<document>&)override{return {};}
 void tab5_phase(const char *stage)override{pulse();if(source.phase)source.phase(stage);}
 void get_media_features(media_features &m)const override{m.type=media_type_screen;m.width=m.device_width=source.width;m.height=m.device_height=source.height;m.color=8;m.resolution=96;}
 void get_language(std::string &language,std::string &culture)const override{language="pt";culture="BR";}
};
}
struct HtmlEngine::Impl {std::unique_ptr<Container> container;std::string key,failedKey,failure;bool simplified=false,truncated=false;};
HtmlEngine::HtmlEngine():state_(new Impl){}
HtmlEngine::~HtmlEngine()=default;
void HtmlEngine::reset(){state_->container.reset();state_->key.clear();state_->failedKey.clear();state_->failure.clear();state_->simplified=state_->truncated=false;}
HtmlResult HtmlEngine::render(const HtmlSource &source){
 AllocScope memory;HtmlResult result;
 try{
  if(source.html.size()>1024*1024||source.width<200||source.width>1280||source.height<100||source.height>640)throw std::runtime_error("Documento acima dos limites do leitor");
  auto key=source.key+"/"+std::to_string(source.width)+"/"+std::to_string(source.height);
  if(state_->failedKey==key){result.error=state_->failure;return result;}
  if(source.phase)source.phase("prepare");
  if(!state_->container||state_->key!=key){
   state_->container.reset();state_->key.clear();
   size_t elements=0;auto input=htmlDocumentInput(source.html,&elements);state_->simplified=elements>512;state_->truncated=false;
   if(state_->simplified){if(source.phase)source.phase("reader");auto reading=htmlReadableDocument(input);input=std::move(reading.html);state_->truncated=reading.truncated;}
   auto next=std::make_unique<Container>(source,std::move(input));next->begin();if(source.phase)source.phase("parse");next->doc=document::createFromString(estring(next->source.html,encoding::utf_8),next.get(),source.masterCss);if(!next->doc)throw std::runtime_error("Falha ao interpretar HTML");if(source.phase)source.phase("layout");next->doc->render(source.width);state_->container=std::move(next);state_->key=key;
  }
  result.simplified=state_->simplified;result.truncated=state_->truncated;
  auto &c=*state_->container;c.source.cancelled=source.cancelled;c.begin();auto height=c.doc->height();if(!std::isfinite(height)||height>source.height*256)throw std::runtime_error("Documento excede 256 páginas");height=std::max(0.f,height);result.pages=std::max(1u,unsigned(std::ceil(height/source.height)));if(result.pages>256)throw std::runtime_error("Documento excede 256 páginas");result.page=std::min(source.page,result.pages-1);
  if(source.phase)source.phase("links");
  if(c.source.resolve) for(const auto &element:c.doc->root()->select_all("a[href]")) {
   if(result.links.size()>=128)break;
   auto href=element->get_attr("href","");auto url=c.source.resolve(href,c.source.base);
   if(url.empty()||url.size()>1024)continue;
   std::string text;element->get_text(text);if(text.size()>160)text=url;
   result.links.push_back({url,text.empty()?url:text});
   std::vector<litehtml::element::ptr> pending{element};
   while(!pending.empty()&&result.hotspots.size()<512){auto node=pending.back();pending.pop_back();
    if(!node->children().empty()){for(const auto &child:node->children())pending.push_back(child);continue;}
    node->run_on_renderers([&](const std::shared_ptr<render_item> &item){auto box=item->get_placement();int x=std::max(0,int(std::floor(box.x))),y=std::max(0,int(std::floor(box.y-result.page*source.height)));int right=std::min(int(source.width),int(std::ceil(box.right()))),bottom=std::min(int(source.height),int(std::ceil(box.bottom()-result.page*source.height)));if(right>x&&bottom>y&&result.hotspots.size()<512)result.hotspots.push_back({x,y,right-x,bottom-y,url});return true;});
   }
  }
  c.pulse();if(c.canvas&&c.paintedPage==result.page){result.picture=c.canvas;result.pending=c.pending;if(source.phase)source.phase("ready");return result;}
  if(source.phase)source.phase("draw");
  c.canvas=std::make_shared<Picture>();c.canvas->width=source.width;c.canvas->height=source.height;c.canvas->pixels.assign(source.width*source.height*2,char(0xff));c.clips.clear();position clip(0,0,source.width,source.height);c.doc->draw(0,0,-float(result.page*source.height),&clip);c.paintedPage=result.page;result.picture=c.canvas;result.pending=c.pending;if(source.phase)source.phase("ready");return result;
 }catch(const std::exception &e){result.error=dynamic_cast<const std::bad_alloc*>(&e)?"Memória insuficiente para renderizar HTML":e.what();reset();state_->failedKey=source.key+"/"+std::to_string(source.width)+"/"+std::to_string(source.height);state_->failure=result.error;if(source.phase)source.phase("error");return result;}catch(...){result.error="Falha no motor HTML";reset();if(source.phase)source.phase("error");return result;}
}
}
