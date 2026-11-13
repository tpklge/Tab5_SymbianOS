// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
std::size_t heap_caps_get_free_size(unsigned);
void *heap_caps_malloc(std::size_t,unsigned);
void *heap_caps_realloc(void *,std::size_t,unsigned);
