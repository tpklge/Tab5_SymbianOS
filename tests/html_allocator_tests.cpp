// SPDX-License-Identifier: MIT
// Compile the ESP allocator implementation with a simulated heap, not its
// host fallback. This tests task isolation and failure without a physical P4.
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
#include "freertos/task.h"
static std::atomic<int> scheduler{taskSCHEDULER_NOT_STARTED};
int xTaskGetSchedulerState(){return scheduler.load();}
static thread_local unsigned psramCalls=0;
static thread_local size_t available=8*1024*1024;
size_t heap_caps_get_free_size(unsigned){return available;}
void *heap_caps_malloc(size_t n,unsigned){++psramCalls;return std::malloc(n);}
void *heap_caps_realloc(void *p,size_t n,unsigned){++psramCalls;return std::realloc(p,n);}
extern "C" bool tab5_html_alloc_scope(bool);
extern "C" void *tab5_html_malloc(size_t);
// The actual regression happened in a litehtml global constructor before
// app_main. Exercise global new at that phase, not just from main/threads.
struct BeforeScheduler {
  BeforeScheduler(){
    assert(!tab5_html_alloc_scope(true));
    const auto calls=psramCalls;
    auto *p=::operator new(210);::operator delete(p);
    assert(psramCalls==calls);
  }
};
static BeforeScheduler earlyAllocation;
int main(){
  scheduler=taskSCHEDULER_RUNNING;
  std::atomic<bool> go{false},done{false};
  std::thread other([&]{
    while(!go)std::this_thread::yield();
    assert(!tab5_html_alloc_scope(false));
    const auto calls=psramCalls;
    auto *p=::operator new(128);::operator delete(p);
    assert(calls==psramCalls);
    assert(!tab5_html_alloc_scope(true));
    p=::operator new(128);::operator delete(p);
    assert(psramCalls==calls+1 && tab5_html_alloc_scope(false));
    done=true;
  });
  assert(!tab5_html_alloc_scope(true));
  go=true;
  while(!done)std::this_thread::yield();
  assert(tab5_html_alloc_scope(true)); // Other thread did not disable this scope.
  const auto calls=psramCalls;
  auto *p=::operator new(128);::operator delete(p);
  assert(psramCalls==calls+1);
  available=2*1024*1024;
  assert(tab5_html_malloc(128)==nullptr); // C parser sees a recoverable allocation failure.
  bool rejected=false;try{p=::operator new(128);::operator delete(p);}catch(const std::bad_alloc&){rejected=true;}
  assert(rejected && tab5_html_alloc_scope(false));
  p=::operator new(128);::operator delete(p); // Unrelated allocations still work.
  other.join();
  std::cout<<"ESP allocator: pre-scheduler global constructor, concurrent scopes, PSRAM routing and reserve failure isolated (simulated heap)\n";
}
