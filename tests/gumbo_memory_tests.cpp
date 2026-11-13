// SPDX-License-Identifier: MIT
#include "gumbo_memory.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>
static size_t calls=0,failAt=0,live=0,peak=0;
static std::unordered_map<void*,size_t> blocks;
extern "C" void *tab5_html_malloc(size_t n){++calls;if(failAt&&calls==failAt)return nullptr;void *p=std::malloc(n?n:1);if(p){blocks[p]=n;live+=n;if(live>peak)peak=live;}return p;}
extern "C" void tab5_html_free(void *p){if(!p)return;auto i=blocks.find(p);assert(i!=blocks.end());live-=i->second;blocks.erase(i);std::free(p);}
extern "C" void *tab5_html_calloc(size_t n,size_t size){auto *p=tab5_html_malloc(n*size);if(p)std::memset(p,0,n*size);return p;}
extern "C" void *tab5_html_realloc(void *p,size_t size){if(!p)return tab5_html_malloc(size);auto i=blocks.find(p);assert(i!=blocks.end());void *q=tab5_html_malloc(size);if(!q)return nullptr;std::memcpy(q,p,std::min(size,i->second));tab5_html_free(p);return q;}
int main(){
 std::string html="<!doctype html><html><head><meta charset='utf-8'></head><body><table><b>misnested<tr><td>x</table>";
 for(unsigned i=0;i<20;++i)html+="<p class='entry'>Olá &amp; content <a href='/news'>link</a></p>";html+="</body></html>";
 auto parse=[&](size_t budget){return tab5_gumbo_parse_limited(html.data(),html.size(),budget);};
 auto *tree=parse(8*1024*1024);assert(tree);tab5_gumbo_destroy(tree);assert(blocks.empty()&&live==0);const auto allocations=calls;
 for(size_t failure=1;failure<=allocations;++failure){calls=0;failAt=failure;tree=parse(8*1024*1024);assert(!tree);assert(live==0&&blocks.empty());}
 failAt=0;
 for(size_t budget:{size_t(0),size_t(128),size_t(4096)}){tree=parse(budget);assert(!tree&&blocks.empty());}
 tree=parse(8*1024*1024);assert(tree);tab5_gumbo_destroy(tree);assert(blocks.empty());
 std::cout<<"Gumbo allocation failures: "<<allocations<<" points, budget rejection, malformed HTML and recovery; peak="<<peak<<" bytes\n";
}
