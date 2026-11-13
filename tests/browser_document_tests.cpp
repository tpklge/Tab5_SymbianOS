// SPDX-License-Identifier: MIT
#include "browser_document.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
using namespace tab5;
int main(int argc,char **argv){
 const std::string html="<html><head><title>Notícia &amp; teste</title><base href='https://example.test/base/'><script>let x='</form>'; </script><style>BAD</style></head><body><nav>MENU<nav>INNER</nav></nav>before<main><h1>Ação 📰</h1><p>Hello &amp; café <a href='next?x=1&amp;y=2'>Abrir notícia</a></p><!--comment--><script>DROP</script><form><script>var x='</form>';</script>FORM</form><p>FIM</p></main>FOOTER</body></html>";
 BrowserDocument one;one.feed(html.data(),html.size());auto baseline=one.finish();assert(baseline.text.find("Ação")!=baseline.text.npos&&baseline.text.find("Hello & café")!=baseline.text.npos&&baseline.text.find("FIM")!=baseline.text.npos&&baseline.title=="Notícia & teste"&&baseline.base=="https://example.test/base/");for(auto bad:{"MENU","INNER","before","BAD","DROP","FORM","FOOTER"})assert(baseline.text.find(bad)==baseline.text.npos);assert(baseline.links.size()==1&&baseline.links[0].url=="next?x=1&y=2");
 auto expected=BrowserDocument::record("https://example.test/page",baseline);
 for(size_t split=1;split<=html.size();++split){BrowserDocument d;for(size_t at=0;at<html.size();at+=split)d.feed(html.data()+at,std::min(split,html.size()-at));assert(BrowserDocument::record("https://example.test/page",d.finish())==expected);}
 BrowserDocumentData restored;std::string url;assert(BrowserDocument::readRecord(expected,url,restored)&&restored.text==baseline.text&&restored.links.size()==1);assert(!BrowserDocument::readRecord(expected+"P\tduplicate\n",url,restored));assert(!BrowserDocument::readRecord("bad\nT5B2\nX\t0\n",url,restored));
 BrowserDocument huge;std::string part(16384,'x');huge.feed("<script>",8);for(unsigned i=0;i<200;++i)huge.feed(part.data(),part.size());huge.feed("</script><main><a href='/item'>",31);for(unsigned i=0;i<10000;++i)huge.feed("ação ",7);huge.feed("</a></main>",11);auto bounded=huge.finish();assert(bounded.truncated&&bounded.text.size()<=32768&&bounded.links.size()==1);
 BrowserDocument invalid;std::string broken="<main>A";broken+=char(0xff);broken+="B";broken+=char(0xc3);broken+="</main>";for(char c:broken)invalid.feed(&c,1);auto clean=invalid.finish();assert(clean.text.find("\xef\xbf\xbd")!=clean.text.npos);auto cleanRecord=BrowserDocument::record("https://example.test",clean);assert(BrowserDocument::readRecord(cleanRecord,url,restored));
 BrowserDocument literal;std::string comparison="<main>1 < 3 e 5 > 2</main>";literal.feed(comparison.data(),comparison.size());assert(literal.finish().text=="1 < 3 e 5 > 2");
 std::cout<<"Streaming document: chunk boundaries, entities/UTF8, main selection, scripts/forms/menus, 3 MiB script, text/link bounds and cache validation passed\n";
 if(argc==2){std::ifstream f(std::string(argv[1])+"/system/releases/1.0.0/icons.bin",std::ios::binary);std::string atlas{std::istreambuf_iterator<char>(f),{}};assert(atlas.size()>8);unsigned size=(unsigned char)atlas[4]|((unsigned char)atlas[5]<<8),count=(unsigned char)atlas[6]|((unsigned char)atlas[7]<<8);size_t stride=16+size*size*3;std::string browser,feeds;for(unsigned i=0;i<count;++i){auto at=8+i*stride;auto name=atlas.substr(at,16);if(name.find("browser")==0)browser=atlas.substr(at+16,stride-16);if(name.find("feeds")==0)feeds=atlas.substr(at+16,stride-16);}assert(!browser.empty()&&browser==feeds);}
}
