// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include "gumbo.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Result must be released by tab5_gumbo_destroy, including on C++ exceptions. */
GumboOutput *tab5_gumbo_parse(const char *html,size_t length);
GumboOutput *tab5_gumbo_parse_limited(const char *html,size_t length,size_t budget);
void tab5_gumbo_destroy(GumboOutput *output);
#ifdef __cplusplus
}
#endif
