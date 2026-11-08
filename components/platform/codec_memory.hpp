// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdlib>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
namespace tab5 {
// Pixel limits alone do not bound a malformed PNG's inflate output.
inline thread_local size_t codecBytes = 0;
inline constexpr size_t codecBudget = 8 * 1024 * 1024;
struct alignas(std::max_align_t) CodecBlock {
  size_t bytes;
};
inline void *codecRealloc(void *pointer, size_t size) {
  auto *old = pointer ? static_cast<CodecBlock *>(pointer) - 1 : nullptr;
  size_t before = old ? old->bytes : 0;
  if (size > codecBudget || codecBytes - before > codecBudget - size)
    return nullptr;
#ifdef ESP_PLATFORM
  auto *next = static_cast<CodecBlock *>(heap_caps_realloc(
      old, sizeof(CodecBlock) + size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  auto *next =
      static_cast<CodecBlock *>(std::realloc(old, sizeof(CodecBlock) + size));
#endif
  if (!next)
    return nullptr;
  codecBytes = codecBytes - before + size;
  next->bytes = size;
  return next + 1;
}
inline void codecFree(void *pointer) {
  if (!pointer)
    return;
  auto *block = static_cast<CodecBlock *>(pointer) - 1;
  codecBytes -= block->bytes;
#ifdef ESP_PLATFORM
  heap_caps_free(block);
#else
  std::free(block);
#endif
}
} // namespace tab5
