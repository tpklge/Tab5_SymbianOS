// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
#include <functional>
namespace tab5 {
struct BrowserField {std::string name,value,type="text",label;};
struct BrowserForm {std::string action,method="GET",label;std::vector<BrowserField> fields;};
// Streaming, bounded admission layer before native HTML/CSS parsing.
class BrowserWeb {
 std::string token_,raw_,rawName_,css_,script_,html_,skip_;unsigned skipDepth_=0;bool main_=false,mainDone_=false;char quote_=0;
 bool tag_=false,comment_=false,clipped_=false,rawOverflow_=false,rawScript_=false;unsigned nodes_=0;
 std::vector<std::string> stack_;
 std::vector<BrowserForm> forms_;int form_=-1;
 void consume();void append(const std::string&);
public:
 void feed(const char*,size_t);
 std::string finish();
 const std::string &script()const{return script_;}
 const std::vector<BrowserForm> &forms()const{return forms_;}
 bool clipped()const{return clipped_;}
 static std::string attribute(const std::string&,const std::string&);
 static std::string escape(const std::string&);
 static std::string formData(const BrowserForm&,const std::vector<std::string>&);
};
struct BrowserScriptResult {std::string html,cookies,error;bool changed=false;};
BrowserScriptResult browserScript(const std::string &html,const std::string &script,const std::string &url,const std::string &cookies,const std::function<bool()> &cancelled={});
}
