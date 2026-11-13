# SPDX-License-Identifier: MIT
"""Exercise the native event filter and installed IDF Wi-Fi START/STOP handlers.

Radio RPC, event queue and netif insertion are simulated. The initial unfiltered
case reproduces a repeated insertion, then the filter must preserve queue order,
failures and concurrency without invoking the START handler twice per lifetime.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IDF = Path(__import__('os').environ.get('IDF_PATH', str(Path.home()/'esp/esp-idf')))

def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prelude = r'''
#include "wifi_lifecycle.hpp"
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
using esp_event_base_t=int;
using esp_err_t=int;
using wifi_netif_driver_t=void*;
constexpr int ESP_OK=0,ESP_ERR_INVALID_STATE=259,ESP_ERR_TIMEOUT=263;
constexpr int WIFI_EVENT=1,WIFI_IF_STA=0;
constexpr int WIFI_EVENT_STA_START=1,WIFI_EVENT_STA_STOP=2,WIFI_EVENT_AP_START=3,WIFI_EVENT_AP_STOP=4;
constexpr uint32_t portMAX_DELAY=0xffffffff;
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
struct Interface {bool added=false;unsigned starts=0,stops=0;}sta,ap;
void *s_wifi_netifs[]={&sta,&ap};
wifi_netif_driver_t esp_netif_get_io_driver(void*p){return p;}
int esp_wifi_get_if_mac(void*,uint8_t*){return ESP_OK;}
bool esp_wifi_is_if_ready_when_started(void*){return true;}
void esp_netif_receive(){}
int esp_wifi_register_if_rxcb(void*,decltype(&esp_netif_receive),void*){return ESP_OK;}
void esp_netif_netstack_buf_ref(){}void esp_netif_netstack_buf_free(){}
int esp_wifi_internal_reg_netstack_buf_cb(decltype(&esp_netif_netstack_buf_ref),decltype(&esp_netif_netstack_buf_free)){return ESP_OK;}
int esp_netif_set_mac(void*,uint8_t*){return ESP_OK;}
void esp_netif_action_start(void *p,int,int,void*){
 auto &net=*static_cast<Interface*>(p);
 if(net.added)throw std::runtime_error("netif already added");
 net.added=true;++net.starts;
}
void esp_netif_action_stop(void *p,int,int,void*){
 auto &net=*static_cast<Interface*>(p);net.added=false;++net.stops;
}
struct Queued {int32_t id;void*data;size_t size;uint32_t ticks;};
std::deque<Queued> queue;
bool failNext=false;
int postOriginal(int32_t id,void *data,size_t size,uint32_t ticks){
 if(failNext){failNext=false;return ESP_ERR_TIMEOUT;}
 queue.push_back({id,data,size,ticks});return ESP_OK;
}
std::mutex wifiEventMutex;
tab5::WifiLifecycle staEvents,apEvents;
using WifiEventPost=int(*)(int32_t,void*,size_t,uint32_t);
WifiEventPost originalWifiPost=postOriginal;
'''
checks = r'''
void drain(){
 while(!queue.empty()){
  auto next=queue.front();queue.pop_front();
  if(next.id==WIFI_EVENT_STA_START)wifi_default_action_sta_start(nullptr,WIFI_EVENT,next.id,next.data);
  else if(next.id==WIFI_EVENT_STA_STOP)wifi_default_action_sta_stop(nullptr,WIFI_EVENT,next.id,next.data);
  else if(next.id==WIFI_EVENT_AP_START)esp_netif_action_start(&ap,WIFI_EVENT,next.id,next.data);
  else if(next.id==WIFI_EVENT_AP_STOP)esp_netif_action_stop(&ap,WIFI_EVENT,next.id,next.data);
 }
}
void reset(){sta={};ap={};staEvents={};apEvents={};queue.clear();failNext=false;}
int post(int event){return filteredWifiEventPost(event,nullptr,0,100);}
int main(){
 // Baseline: the installed handler unconditionally calls netif start twice.
 postOriginal(WIFI_EVENT_STA_START,nullptr,0,100);
 postOriginal(WIFI_EVENT_STA_START,nullptr,0,100);
 bool reproduced=false;try{drain();}catch(const std::runtime_error&){reproduced=true;}
 assert(reproduced);reset();
 assert(post(WIFI_EVENT_STA_START)==ESP_OK && post(WIFI_EVENT_STA_START)==ESP_OK);
 assert(queue.size()==1);drain();assert(sta.starts==1 && sta.added);
 assert(staEvents.duplicateStarts()==1);
 // Association/DHCP is not required for duplicate suppression.
 for(unsigned cycle=0;cycle<1000;++cycle){
  assert(post(WIFI_EVENT_STA_STOP)==ESP_OK && post(WIFI_EVENT_STA_STOP)==ESP_OK);
  assert(post(WIFI_EVENT_STA_START)==ESP_OK && post(WIFI_EVENT_STA_START)==ESP_OK);
  assert(queue.size()==2);drain();
 }
 assert(sta.starts==1001 && sta.stops==1000 && sta.added);
 // Failed queue writes must not consume the transition.
 reset();failNext=true;
 assert(post(WIFI_EVENT_STA_START)==ESP_ERR_TIMEOUT && queue.empty());
 assert(post(WIFI_EVENT_STA_START)==ESP_OK);drain();
 failNext=true;assert(post(WIFI_EVENT_STA_STOP)==ESP_ERR_TIMEOUT);
 assert(post(WIFI_EVENT_STA_START)==ESP_OK && queue.empty());
 assert(post(WIFI_EVENT_STA_STOP)==ESP_OK && post(WIFI_EVENT_STA_START)==ESP_OK);
 drain();assert(sta.starts==2 && sta.stops==1);
 // Deinit supplies a missing STOP before another START can reach lwIP.
 endStationEvents();assert(post(WIFI_EVENT_STA_START)==ESP_OK);
 drain();assert(sta.starts==3 && sta.stops==2);
 // AP and station have independent lifetimes. Other payloads pass unchanged.
 assert(post(WIFI_EVENT_AP_START)==ESP_OK && post(WIFI_EVENT_AP_START)==ESP_OK);
 char payload[]="event";
 assert(filteredWifiEventPost(42,payload,sizeof(payload),73)==ESP_OK);
 assert(queue.back().id==42 && queue.back().data==payload && queue.back().size==sizeof(payload) && queue.back().ticks==73);
 drain();assert(ap.starts==1 && sta.starts==3);
 // Concurrent duplicate producers enqueue exactly one start.
 reset();std::vector<std::thread> producers;
 for(unsigned i=0;i<16;++i)producers.emplace_back([]{for(unsigned n=0;n<100;++n)assert(post(WIFI_EVENT_STA_START)==ESP_OK);});
 for(auto &thread:producers)thread.join();
 assert(queue.size()==1 && staEvents.duplicateStarts()==1599);drain();assert(sta.starts==1);
 std::cout<<"Installed IDF handlers: repeated netif insertion reproduced; native filter passes 1000 restarts, queue failures, missing STOP, AP isolation and 16 producers (host simulation)\n";
}
'''
network = (ROOT/'main/network.cpp').read_text()
wifi = (IDF/'components/esp_wifi/src/wifi_default.c').read_text()
code = prelude
for signature in ('static void wifi_start(', 'static void wifi_default_action_sta_start(', 'static void wifi_default_action_sta_stop('):
    code += function(wifi,signature)+'\n'
for signature in ('int filteredWifiEventPost(', 'void endStationEvents()'):
    code += function(network,signature)+'\n'
code += checks
with tempfile.TemporaryDirectory(prefix='tab5-wifi-lifecycle-') as temp:
    source = Path(temp)/'lifecycle.cpp'
    binary = source.with_suffix('')
    source.write_text(code)
    command = [sys.argv[1] if len(sys.argv)>1 else 'c++', '-std=c++17', '-pthread', '-I'+str(ROOT/'components/platform/include'), str(source), '-o', str(binary)]
    sdk = Path('/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk')
    if sys.platform=='darwin' and sdk.exists():
        command += ['-isysroot',str(sdk),'-mmacosx-version-min=26.0']
    subprocess.run(command,check=True)
    subprocess.run([str(binary)],check=True)
