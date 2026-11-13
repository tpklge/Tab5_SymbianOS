// SPDX-License-Identifier: MIT
#include "gumbo_memory.h"
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
extern void *tab5_html_malloc(size_t);
extern void tab5_html_free(void *);
typedef struct Context Context;
typedef union Block Block;
union Block {
 struct {Block *previous,*next;Context *owner;size_t bytes;} item;
 max_align_t alignment;
};
struct Context {
 GumboOptions options;
 jmp_buf failure;
 Block *blocks;
 size_t used,budget;
};
static void release_all(Context *context) {
 Block *block=context->blocks;
 while(block){Block *next=block->item.next;tab5_html_free(block);block=next;}
 tab5_html_free(context);
}
static void *allocate(void *userdata,size_t size) {
 Context *context=userdata;
 if(size>SIZE_MAX-sizeof(Block)||context->used>context->budget||size+sizeof(Block)>context->budget-context->used)longjmp(context->failure,1);
 Block *block=tab5_html_malloc(sizeof(Block)+size);
 /* Gumbo assumes allocation succeeds. Never return NULL into those C paths.
  * The jump crosses only C parser frames; C++ ownership remains outside. */
 if(!block)longjmp(context->failure,1);
 block->item.previous=NULL;block->item.next=context->blocks;block->item.owner=context;block->item.bytes=size+sizeof(Block);
 if(context->blocks)context->blocks->item.previous=block;
 context->blocks=block;context->used+=block->item.bytes;
 return block+1;
}
static void deallocate(void *userdata,void *pointer) {
 if(!pointer)return;
 Context *context=userdata;Block *block=((Block*)pointer)-1;
 if(block->item.previous)block->item.previous->item.next=block->item.next;else context->blocks=block->item.next;
 if(block->item.next)block->item.next->item.previous=block->item.previous;
 context->used-=block->item.bytes;tab5_html_free(block);
}
GumboOutput *tab5_gumbo_parse_limited(const char *html,size_t length,size_t budget) {
 Context *context=tab5_html_malloc(sizeof(Context));if(!context)return NULL;
 memset(context,0,sizeof(*context));context->budget=budget;context->options=kGumboDefaultOptions;
 context->options.allocator=allocate;context->options.deallocator=deallocate;context->options.userdata=context;
 if(setjmp(context->failure)){release_all(context);return NULL;}
 GumboOutput *output=gumbo_parse_with_options(&context->options,html,length);
 if(!output)release_all(context);
 return output;
}
GumboOutput *tab5_gumbo_parse(const char *html,size_t length){return tab5_gumbo_parse_limited(html,length,8u*1024u*1024u);}
void tab5_gumbo_destroy(GumboOutput *output) {
 if(!output)return;
 Context *context=(((Block*)output)-1)->item.owner;
 gumbo_destroy_output(&context->options,output);
 /* Also collect any partially detached allocations from malformed markup. */
 release_all(context);
}
