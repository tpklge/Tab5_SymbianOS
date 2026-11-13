// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
namespace tab5 {
struct BrowserLink {std::string url,label;};
struct BrowserDocumentData {std::string title,base,text,markup,scriptError;std::vector<BrowserLink> links;bool truncated=false;};
// Bounded streaming text/link extraction. Remote HTML/CSS never reaches Gumbo.
class BrowserDocument {
 BrowserDocumentData data_;
 std::string token_,text_,raw_,rawPattern_,title_,skipTag_,href_,label_;
 size_t rawMatch_=0;unsigned mode_=0,skipDepth_=0,breaks_=0;
 char quote_=0;bool discard_=false,head_=false,main_=false,mainDone_=false,anchor_=false,titleStopped_=false;
 void tag();void flush(bool final);void text(const std::string &);void closeLink();void line();void resetContent();
public:
 void feed(const char *,size_t);
 BrowserDocumentData finish();
 static std::string record(const std::string &url,const BrowserDocumentData &);
 static bool readRecord(const std::string &,std::string &url,BrowserDocumentData &);
};
}
