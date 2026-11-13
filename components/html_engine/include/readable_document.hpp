// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace tab5 {
struct ReadableDocument {std::string html;bool truncated=false;};
ReadableDocument htmlReadableDocument(const std::string &html);
}
