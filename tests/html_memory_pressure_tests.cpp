// SPDX-License-Identifier: MIT
#include "html_engine.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
static std::unordered_map<void*,size_t> blocks;
static size_t calls=0,failAt=0;
extern "C" bool tab5_html_alloc_scope(bool){return false;}
extern "C" void *tab5_html_malloc(size_t n){if(++calls==failAt)return nullptr;void *p=std::malloc(n?n:1);if(p)blocks[p]=n;return p;}
extern "C" void tab5_html_free(void *p){if(!p)return;auto i=blocks.find(p);assert(i!=blocks.end());blocks.erase(i);std::free(p);}
extern "C" void *tab5_html_calloc(size_t n,size_t size){auto *p=tab5_html_malloc(n*size);if(p)std::memset(p,0,n*size);return p;}
extern "C" void *tab5_html_realloc(void *p,size_t n){if(!p)return tab5_html_malloc(n);auto i=blocks.find(p);assert(i!=blocks.end());auto *out=tab5_html_malloc(n);if(out){std::memcpy(out,p,std::min(n,i->second));tab5_html_free(p);}return out;}
std::string read(const std::string &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){assert(argc==2);tab5::HtmlSource source;source.font=read(std::string(argv[1])+"/theme.ttf");source.masterCss=read(std::string(argv[1])+"/html-default.css");source.html="<html><body><h1 style='font-size:96px'>MWÁ</h1><p>Memória</p></body></html>";source.key="pressure";source.width=1184;source.height=352;
 {tab5::HtmlEngine engine;assert(engine.render(source).picture);}assert(blocks.empty());auto total=calls;
 for(size_t failure=1;failure<=total;++failure){calls=0;failAt=failure;{tab5::HtmlEngine engine;auto result=engine.render(source);assert(!result.picture&&result.error.find("Memória")!=result.error.npos);}assert(blocks.empty());}
 failAt=0;{tab5::HtmlEngine engine;assert(engine.render(source).picture);}assert(blocks.empty());std::cout<<"Full HTML/font render OOM: "<<total<<" allocation failures recovered without leaks; subsequent render passed\n";
}
