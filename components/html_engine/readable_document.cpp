// SPDX-License-Identifier: MIT
#include "readable_document.hpp"
#include "gumbo_memory.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace tab5 {
namespace {
GumboVector *children(GumboNode *n){if(n->type==GUMBO_NODE_ELEMENT||n->type==GUMBO_NODE_TEMPLATE)return &n->v.element.children;if(n->type==GUMBO_NODE_DOCUMENT)return &n->v.document.children;return nullptr;}
std::string escape(const std::string &s){std::string out;for(char c:s){if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='>')out+="&gt;";else if(c=='\"')out+="&quot;";else out+=c;}return out;}
bool block(GumboTag t){return t==GUMBO_TAG_P||t==GUMBO_TAG_DIV||t==GUMBO_TAG_LI||t==GUMBO_TAG_ARTICLE||t==GUMBO_TAG_SECTION||t==GUMBO_TAG_H1||t==GUMBO_TAG_H2||t==GUMBO_TAG_H3||t==GUMBO_TAG_H4||t==GUMBO_TAG_H5||t==GUMBO_TAG_H6||t==GUMBO_TAG_TR||t==GUMBO_TAG_BR;}
bool hidden(GumboNode *n){auto &e=n->v.element;auto t=e.tag;if(t==GUMBO_TAG_SCRIPT||t==GUMBO_TAG_STYLE||t==GUMBO_TAG_HEAD||t==GUMBO_TAG_NAV||t==GUMBO_TAG_FOOTER||t==GUMBO_TAG_FORM||t==GUMBO_TAG_BUTTON||t==GUMBO_TAG_SVG||t==GUMBO_TAG_IFRAME||t==GUMBO_TAG_TEMPLATE)return true;if(gumbo_get_attribute(&e.attributes,"hidden"))return true;auto *aria=gumbo_get_attribute(&e.attributes,"aria-hidden");if(aria&&std::string(aria->value)=="true")return true;auto *style=gumbo_get_attribute(&e.attributes,"style");if(style){std::string v;for(const auto *p=style->value;*p;++p)if(!std::isspace(static_cast<unsigned char>(*p)))v+=char(std::tolower(static_cast<unsigned char>(*p)));if(v.find("display:none")!=v.npos||v.find("visibility:hidden")!=v.npos)return true;}return false;}
}
ReadableDocument htmlReadableDocument(const std::string &html){
 std::unique_ptr<GumboOutput,decltype(&tab5_gumbo_destroy)> tree(tab5_gumbo_parse(html.data(),html.size()),tab5_gumbo_destroy);if(!tree)throw std::bad_alloc();
 struct Visit {GumboNode *node;unsigned depth;};std::vector<Visit> pending{{tree->root,1}};GumboNode *main=nullptr,*body=nullptr;std::string base;size_t visited=0;
 // Inspect the whole raw tree before prioritizing main: depth/count guards
 // still apply to content omitted from the lightweight document.
 while(!pending.empty()){auto v=pending.back();pending.pop_back();if(++visited>32768||v.depth>128)throw std::runtime_error("Documento excede os limites de estrutura HTML");if(v.node->type==GUMBO_NODE_ELEMENT){if(v.node->v.element.tag==GUMBO_TAG_MAIN&&!main)main=v.node;if(v.node->v.element.tag==GUMBO_TAG_BODY)body=v.node;if(v.node->v.element.tag==GUMBO_TAG_BASE&&base.empty()){auto *href=gumbo_get_attribute(&v.node->v.element.attributes,"href");if(href&&strlen(href->value)<=1024)base=href->value;}}if(auto *c=children(v.node))for(size_t i=c->length;i>0;--i)pending.push_back({static_cast<GumboNode*>(c->data[i-1]),v.depth+1});}
 ReadableDocument out;out.html=base.empty()?"<html><body>":"<html><head><base href=\""+escape(base)+"\"></head><body>";size_t textBytes=0,links=0,breaks=0;bool whitespace=true,lineStart=true;
 auto newline=[&]{if(!lineStart&&breaks<192){out.html+="<br>";++breaks;lineStart=true;whitespace=true;}};
 auto text=[&](const char *s){for(size_t i=0;s[i];++i){unsigned char c=s[i];if((c&0xc0)!=0x80&&c>=128){size_t width=c<0xe0?2:c<0xf0?3:4;if(textBytes+width>32768){out.truncated=true;break;}}if(textBytes>=32768){out.truncated=true;break;}if(c<128&&std::isspace(c)){if(!whitespace){out.html+=' ';++textBytes;whitespace=true;}}else {if(c=='&')out.html+="&amp;";else if(c=='<')out.html+="&lt;";else if(c=='>')out.html+="&gt;";else out.html+=char(c);++textBytes;whitespace=false;lineStart=false;}}};
 struct Event {GumboNode *node;bool exit,anchor;};std::vector<Event> events{{main?main:body?body:tree->root,false,false}};
 while(!events.empty()&&!out.truncated){auto event=events.back();events.pop_back();auto *n=event.node;if(event.exit){if(event.anchor)out.html+="</a>";if(n->type==GUMBO_NODE_ELEMENT&&block(n->v.element.tag))newline();continue;}
  if(n->type==GUMBO_NODE_TEXT||n->type==GUMBO_NODE_WHITESPACE||n->type==GUMBO_NODE_CDATA){text(n->v.text.text);continue;}
  if(n->type!=GUMBO_NODE_ELEMENT&&n->type!=GUMBO_NODE_DOCUMENT)continue;
  if(n->type==GUMBO_NODE_ELEMENT&&hidden(n))continue;
  bool anchor=false;
  if(n->type==GUMBO_NODE_ELEMENT){auto tag=n->v.element.tag;if(block(tag))newline();
   if(tag==GUMBO_TAG_A&&links<128){auto *href=gumbo_get_attribute(&n->v.element.attributes,"href");if(href&&strlen(href->value)<=1024){std::string url=href->value;auto low=url;for(auto &c:low)c=char(std::tolower(static_cast<unsigned char>(c)));if(low.find(':')==low.npos||low.rfind("https://",0)==0||low.rfind("http://",0)==0){out.html+="<a href=\""+escape(url)+"\">";++links;anchor=true;}}}
   if(tag==GUMBO_TAG_IMG){auto *alt=gumbo_get_attribute(&n->v.element.attributes,"alt");if(alt&&alt->value[0])text(alt->value);}
  }
  events.push_back({n,true,anchor});if(auto *c=children(n))for(size_t i=c->length;i>0;--i)events.push_back({static_cast<GumboNode*>(c->data[i-1]),false,false});
 }
 // Close any active links if the text budget ended within an element.
 if(out.truncated){while(!events.empty()){if(events.back().exit&&events.back().anchor)out.html+="</a>";events.pop_back();}out.html+="<p>Trecho da página. Abra um link para continuar a leitura.</p>";}
 out.html+="</body></html>";return out;
}
}
