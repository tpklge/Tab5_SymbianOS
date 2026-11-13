// SPDX-License-Identifier: MIT
#pragma once
#include "picture.hpp"
#include <functional>
#include <memory>
#include <vector>
namespace tab5 {
struct HtmlAsset {std::string url;bool css=false;};
struct HtmlSource {
 std::string html,base,masterCss,font,key;
 unsigned page=0,width=1184,height=400;
 std::function<std::string(const std::string &,const std::string &)> resolve;
 std::function<std::string(const std::string &,const std::string &,bool)> resource;
 std::function<std::shared_ptr<const Picture>(const std::string&,const std::string&)> image;
 std::function<bool()> cancelled;
 std::function<void(const char *)> phase;
};
struct HtmlLink {std::string url,label;};
struct HtmlHotspot {int x=0,y=0,width=0,height=0;std::string url;};
struct HtmlResult {std::vector<HtmlHotspot> hotspots;bool simplified=false,truncated=false;std::vector<HtmlLink> links;std::shared_ptr<Picture> picture;unsigned page=0,pages=1;std::vector<HtmlAsset> pending;std::string error;};
class HtmlEngine {
 struct Impl;std::unique_ptr<Impl> state_;
public:
 HtmlEngine();~HtmlEngine();void reset();
 HtmlResult render(const HtmlSource &);
};
}
