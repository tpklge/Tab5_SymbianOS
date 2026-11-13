// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <limits>
#include <new>
extern "C" void *tab5_html_malloc(size_t);
extern "C" void tab5_html_free(void *);
namespace tab5 {
// stb_truetype is compiled as C++ in the renderer. Throw on allocation failure
// and retain temporary blocks so unwinding cannot leak its plain C buffers.
class HtmlFontMemory {
 union Block {struct {Block *previous,*next;HtmlFontMemory *owner;} item;std::max_align_t alignment;};
 Block *blocks_=nullptr;
public:
 HtmlFontMemory()=default;HtmlFontMemory(const HtmlFontMemory&)=delete;
 HtmlFontMemory &operator=(const HtmlFontMemory&)=delete;
 ~HtmlFontMemory(){while(blocks_)release(blocks_+1);}
 void *allocate(size_t size){
  if(size>std::numeric_limits<size_t>::max()-sizeof(Block))throw std::bad_alloc();
  auto *block=static_cast<Block*>(tab5_html_malloc(sizeof(Block)+size));if(!block)throw std::bad_alloc();
  block->item.previous=nullptr;block->item.next=blocks_;block->item.owner=this;if(blocks_)blocks_->item.previous=block;blocks_=block;return block+1;
 }
 static void release(void *p){if(!p)return;auto *block=static_cast<Block*>(p)-1;auto *owner=block->item.owner;
  if(block->item.previous)block->item.previous->item.next=block->item.next;else owner->blocks_=block->item.next;
  if(block->item.next){block->item.next->item.previous=block->item.previous;}
  tab5_html_free(block);
 }
};
inline void *htmlFontAllocate(size_t size,void *owner){if(!owner)throw std::bad_alloc();return static_cast<HtmlFontMemory*>(owner)->allocate(size);}
}
