// SPDX-License-Identifier: MIT
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <cstring>
#if !CONFIG_LV_USE_CUSTOM_MALLOC
#error "Tab5 graphics require the PSRAM allocator; apply sdkconfig.defaults"
#endif
// Referenced by startup, also keeps this allocator in the static link.
bool tab5_graphics_memory_available() {
  return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) >= 2 * 1024 * 1024;
}
extern "C" {
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *, size_t) { return nullptr; }
void lv_mem_remove_pool(lv_mem_pool_t) {}
void *lv_malloc_core(size_t size) {
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void *lv_realloc_core(void *p, size_t size) {
  return heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void lv_free_core(void *p) { heap_caps_free(p); }
void lv_mem_monitor_core(lv_mem_monitor_t *m) {
  memset(m, 0, sizeof(*m));
  m->total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  m->free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  m->free_biggest_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  m->used_pct = m->total_size ? 100 - 100 * m->free_size / m->total_size : 0;
}
lv_result_t lv_mem_test_core(void) {
  return heap_caps_check_integrity(MALLOC_CAP_SPIRAM, false)
             ? LV_RESULT_OK
             : LV_RESULT_INVALID;
}
}
