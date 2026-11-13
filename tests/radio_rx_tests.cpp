// SPDX-License-Identifier: MIT
#include "radio_rx.hpp"
#include "network_signal.hpp"
#include <vector>
#include <iostream>
#include <cstdlib>
using namespace tab5;
void check(bool ok,const char *message) {if(!ok){std::cerr<<"FAIL: "<<message<<'\n';exit(1);}}
std::string frame(const std::string &body) {return std::string("\x94\xc3",2)+char(body.size()>>8)+char(body.size())+body;}
int main() {
 std::vector<RadioFrame> storage(256);RadioInbox inbox;inbox.attach(storage.data(),storage.size());SerialFrames parser;
 auto receive=[&](const char *p,size_t n){inbox.push(p,n);};
 auto encoded=frame(std::string(512,'x'));
 for(char b:encoded)parser.feed(&b,1,receive);
 std::string out;check(inbox.pop(out) && out==std::string(512,'x'),"byte-fragmented maximum frame");
 std::string noise="boot debug\r\n";noise+=char(0x94);noise+=frame("good");parser.feed(noise.data(),noise.size(),receive);check(inbox.pop(out)&&out=="good","noise and repeated magic resynchronize");
 std::string invalid="\x94\xc3\xff\xff";invalid+=frame("after invalid length");parser.feed(invalid.data(),invalid.size(),receive);check(parser.invalid==1 && inbox.pop(out)&&out=="after invalid length","invalid length does not poison next frame");
 std::string burst;for(unsigned i=0;i<200;++i)burst+=frame(std::to_string(i));
 // Entire initial radio database arrives before the Lua/UI consumes anything.
 for(size_t p=0;p<burst.size();p+=37)parser.feed(burst.data()+p,std::min(size_t(37),burst.size()-p),receive);
 check(inbox.size()==200 && inbox.dropped==0,"initial burst retained independently of UI tick");
 for(unsigned i=0;i<200;++i)check(inbox.pop(out)&&out==std::to_string(i),"FIFO burst ordering");
 for(unsigned i=0;i<256;++i)check(inbox.push("a",1),"bounded inbox capacity");check(!inbox.push("overflow",8)&&inbox.dropped==1,"overflow counted without overwriting queued packets");
 check(!inbox.push("",0) && !inbox.push(std::string(513,'x').data(),513),"oversized/empty frames rejected");
 check(wifiLevel(false,true,-40)==-1 && wifiLevel(true,false,-40)==0,"disconnected/unknown signal");
 check(wifiLevel(true,true,-55)==4 && wifiLevel(true,true,-56)==3 && wifiLevel(true,true,-65)==3 && wifiLevel(true,true,-66)==2 && wifiLevel(true,true,-75)==2 && wifiLevel(true,true,-76)==1 && wifiLevel(true,true,-85)==1 && wifiLevel(true,true,-86)==0,"Wi-Fi signal level thresholds");
 std::cout<<"UART fragmented/burst/noise/overflow and Wi-Fi levels passed\n";
}
