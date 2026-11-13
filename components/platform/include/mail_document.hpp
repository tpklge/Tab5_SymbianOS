// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <string>
#include <vector>
namespace tab5 {
struct Picture;
struct MailRun {std::string text,href;bool strong=false,emphasis=false,underline=false;};
struct MailBlock {
  std::vector<MailRun> runs;
  std::string source,alt,align;
  unsigned heading=0;bool quote=false;
  std::shared_ptr<const Picture> picture;
};
namespace mail {
std::vector<MailBlock> document(const std::string &html);
std::string htmlEntityText(const std::string &text);
}
}
