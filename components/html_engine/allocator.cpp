// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <cstring>
#include <new>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
namespace {
thread_local bool htmlAlloc=false;
bool taskStorageReady() {
#ifdef ESP_PLATFORM
 // ESP-IDF runs global C++ constructors before starting FreeRTOS. RISC-V
 // tp is still zero there: even reading a trivial thread_local would fault.
 return xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED;
#else
 return true;
#endif
}
}
extern "C" bool tab5_html_alloc_scope(bool value){
 if(!taskStorageReady())return false;
 bool old=htmlAlloc;htmlAlloc=value;return old;
}
#ifdef ESP_PLATFORM
void *operator new(std::size_t n){const bool scoped=taskStorageReady() && htmlAlloc;if(scoped){auto available=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);if(n>available || available-n<2*1024*1024)throw std::bad_alloc();}void *p=scoped?heap_caps_malloc(n?n:1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void *operator new[](std::size_t n){return ::operator new(n);}
#endif
extern "C" void *tab5_html_malloc(size_t n){
#ifdef ESP_PLATFORM
 const auto available=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
 if(n>available||available-n<2*1024*1024)return nullptr;
 return heap_caps_malloc(n?n:1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 return std::malloc(n?n:1);
#endif
}
extern "C" void *tab5_html_calloc(size_t n,size_t size){if(size && n>size_t(-1)/size)return nullptr;auto *p=tab5_html_malloc(n*size);if(p)std::memset(p,0,n*size);return p;}
extern "C" void *tab5_html_realloc(void *p,size_t n){
#ifdef ESP_PLATFORM
 return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 return std::realloc(p,n);
#endif
}
extern "C" void tab5_html_free(void *p){std::free(p);}
