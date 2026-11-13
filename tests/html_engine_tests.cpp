// SPDX-License-Identifier: MIT
#include "html_engine.hpp"
#include "browser_profile.hpp"
#include "document_input.hpp"
#include "readable_document.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
using namespace tab5;
std::string load(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
unsigned color(const HtmlResult &r,unsigned x,unsigned y){size_t p=(y*r.picture->width+x)*2;return (unsigned char)r.picture->pixels[p]|unsigned((unsigned char)r.picture->pixels[p+1])<<8;}
int main(int argc,char **argv){assert(argc==2||argc==3);std::filesystem::path path=argv[1];HtmlSource source;source.font=load(path/"theme.ttf");source.masterCss=load(path/"html-default.css");source.width=400;source.height=300;source.key="cascade";
 source.html="<!doctype html><html><head><style>body{margin:0;background:white}.box{position:absolute;left:20px;top:20px;width:100px;height:50px;background:red}#target{background:lime!important}.hidden{display:none}.row{position:absolute;left:20px;top:90px;display:flex;width:200px;height:50px}.row div{width:100px;height:50px}.a{background:blue}.b{background:red}.bottom{position:absolute;top:650px;left:20px;width:100px;height:40px;background:blue}</style></head><body><div id='target' class='box'>ação</div><div class='box hidden'>hidden</div><div class='row'><div class='a'></div><div class='b'></div></div><div class='bottom'>última</div></body></html>";
 HtmlEngine engine;auto rendered=engine.render(source);if(!rendered.picture){std::cerr<<rendered.error<<'\n';return 1;}assert(color(rendered,25,25)==0x07e0);assert(color(rendered,25,100)==0x001f);assert(color(rendered,125,100)==0xf800);assert(rendered.pages>=3);
 source.page=2;auto last=engine.render(source);assert(last.picture&&last.page==2&&color(last,25,55)==0x001f);
 source.key="mobile-media";source.width=browserProfile::width;source.height=browserProfile::height;source.page=0;source.html="<style>body{margin:0}div{width:60px;height:60px;background:red}@media screen and (max-width:600px){div{background:lime}}</style><div></div>";auto mobile=engine.render(source);assert(mobile.picture&&color(mobile,5,5)==0x07e0);source.width=400;source.height=300;
 source.key="external";source.page=0;source.base="https://example.test/article/";source.html="<link rel='stylesheet' href='site.css'><div class='external'>Olá</div>";source.resolve=[](const std::string &u,const std::string&){return u.rfind("https://",0)==0?u:"https://example.test/article/"+u;};bool available=false;unsigned reads=0;
 source.resource=[&](const std::string &url,const std::string&,bool css){++reads;assert(css&&url=="https://example.test/article/site.css");return available?"body{margin:0}.external{background:lime;width:100px;height:100px}":"";};
 auto missing=engine.render(source);assert(missing.picture&&missing.pending.size()==1&&missing.pending[0].url=="https://example.test/article/site.css");available=true;source.key="external-loaded";auto loaded=engine.render(source);assert(loaded.picture&&loaded.pending.empty()&&color(loaded,5,50)==0x07e0&&reads>=2);
 source.key="cancel";source.cancelled=[] {return true;};auto cancelled=engine.render(source);assert(!cancelled.picture&&cancelled.error.find("cancelada")!=std::string::npos);source.cancelled={};
 source.key="huge";source.html=std::string(1024*1024+1,'x');assert(!engine.render(source).picture);
 source.key="many-leaves";source.resource={};source.html="<html><body>";for(unsigned i=0;i<5000;++i)source.html+="<span>x </span>";source.html+="</body></html>";auto broad=engine.render(source);assert(broad.picture&&broad.simplified);
 source.key="too-many-elements";source.html.clear();for(unsigned i=0;i<8193;++i)source.html+="<br>";auto elements=engine.render(source);assert(!elements.picture&&elements.error.find("8192")!=elements.error.npos);
 source.key="too-deep";source.html.clear();for(unsigned i=0;i<5000;++i)source.html+="<div>";source.html+="text";for(unsigned i=0;i<5000;++i)source.html+="</div>";auto deep=engine.render(source);assert(!deep.picture&&deep.error.find("estrutura")!=deep.error.npos);
 source.key="malformed-table";source.html="<table><b><p>before<tr><td>cell<div>nested</table><p>after</b>";assert(engine.render(source).picture);
 assert(htmlDocumentInput("a<!-- <div> -->b<SCRIPT data-x='>'>var x='<div>'; </SCRIPT ><p>OK</p>")=="ab<p>OK</p>");
 assert(htmlDocumentInput("<scripture>ok</scripture><script>drop</scripture>still drop</script><b>kept</b>")=="<scripture>ok</scripture><b>kept</b>");
 assert(htmlDocumentInput("<style>x::after{content:'<br>'}</style><textarea><script>literal</script></textarea><title><b>literal</b></title>")=="<style>x::after{content:'<br>'}</style><textarea><script>literal</script></textarea><title><b>literal</b></title>");
 assert(htmlDocumentInput("<b title='1 > 0'>ok</b><script>unclosed")=="<b title='1 > 0'>ok</b>");
 source.key="memory-error";source.html="<h1>Recovery</h1>";unsigned parses=0;bool failMemory=true;
 source.phase=[&](const char *phase){if(std::string(phase)=="parse"){++parses;if(failMemory)throw std::bad_alloc();}};
 auto failed=engine.render(source);assert(!failed.picture&&failed.error.find("Memória")!=failed.error.npos);
 failMemory=false;assert(!engine.render(source).picture&&parses==1);source.key="memory-retry";assert(engine.render(source).picture&&parses==2);source.phase={};
 auto reading=htmlReadableDocument("<html><head><style>bad{}</style></head><body><nav>MENU</nav><aside>outside</aside><main><h1>Notícia</h1><p>Ação &amp; texto <a href='/news?x=1&amp;y=2'>Abrir notícia</a></p><div hidden>SECRET</div></main></body></html>");assert(reading.html.find("MENU")==reading.html.npos&&reading.html.find("outside")==reading.html.npos&&reading.html.find("SECRET")==reading.html.npos&&reading.html.find("Notícia")!=reading.html.npos&&reading.html.find("/news?x=1&amp;y=2")!=reading.html.npos);
 auto based=htmlReadableDocument("<html><head><base href='https://example.test/base/'></head><main><a href='article'>Article</a></main></html>");assert(based.html.find("<base href=\"https://example.test/base/\">")!=based.html.npos&&based.html.find("href=\"article\"")!=based.html.npos);
 std::string longArticle="<main><a href='/news'>";for(unsigned i=0;i<10000;++i)longArticle+="ação ";longArticle+="</a></main>";reading=htmlReadableDocument(longArticle);assert(reading.truncated&&reading.html.find("</a>")!=reading.html.npos&&reading.html.find("Trecho da página")!=reading.html.npos);
 if(argc==3){source.key="local-document";source.html=load(argv[2]);source.width=1184;source.height=352;source.base="https://g1.globo.com/";source.resolve={};source.resource={};auto page=engine.render(source);std::cout<<"Local snapshot: input="<<source.html.size()<<" prepared="<<htmlDocumentInput(source.html).size()<<" lightweight="<<page.simplified<<" pages="<<page.pages<<" rendered="<<bool(page.picture)<<" error="<<page.error<<'\n';assert(page.picture);source.page=page.pages-1;assert(engine.render(source).picture);}
 std::cout<<"HTML5/CSS cascade/important, hidden elements, flex, external styles, paging and cancellation passed\n";
}
