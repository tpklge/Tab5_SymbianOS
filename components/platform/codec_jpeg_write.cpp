// SPDX-License-Identifier: MIT
#include "codec_memory.hpp"
#define STBIW_MALLOC(n) tab5::codecRealloc(nullptr, n)
#define STBIW_REALLOC(p, n) tab5::codecRealloc(p, n)
#define STBIW_FREE(p) tab5::codecFree(p)
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"
