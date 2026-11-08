// SPDX-License-Identifier: MIT
// Separate translation unit makes the decoder's linker cost measurable.
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "vendor/minimp3.h"
