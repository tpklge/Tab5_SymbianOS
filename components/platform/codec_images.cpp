// SPDX-License-Identifier: MIT
#include "codec_memory.hpp"
#define STBI_MALLOC(n) tab5::codecRealloc(nullptr, n)
#define STBI_REALLOC(p, n) tab5::codecRealloc(p, n)
#define STBI_FREE(p) tab5::codecFree(p)
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
