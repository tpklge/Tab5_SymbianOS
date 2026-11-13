#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile actual IDF and native startup paths with real POSIX socket failures.

No board/radio/serial access. Task and heap exhaustion are injected; IDF opens
real local sockets. An unpatched SDK must reproduce TASK then FAIL; patched
startup must release all three sockets and permit immediate retry.
"""
from pathlib import Path
import importlib.util, os, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[1]
IDF=Path(os.environ.get('IDF_PATH',str(Path.home()/'esp/esp-idf')))
spec=importlib.util.spec_from_file_location('patch_httpd',ROOT/'tools/patch_httpd.py');patcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(patcher)
def function(source,signature):
 start=source.index(signature);brace=source.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
source=(IDF/'components/esp_http_server/src/httpd_main.c').read_text()
# Verify the dependency's destructor is indeed memory-only in this SDK.
assert 'close(' not in function(source,'static void httpd_delete(')
prelude=r'''
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_ARG=0x102,ESP_ERR_HTTPD_ALLOC_MEM=0x6001,ESP_ERR_HTTPD_TASK=0x6002;
constexpr int HTTPD_MAX_SOCKETS=10,HTTP_SERVER_EVENT_START=1,HTTP_GET=0,HTTP_PUT=1,HTTP_DELETE=2;
constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_SPIRAM=4;
#ifndef CONFIG_LWIP_IPV6
#define CONFIG_LWIP_IPV6 0
#endif
#ifndef CONFIG_HTTPD_QUEUE_WORK_BLOCKING
#define CONFIG_HTTPD_QUEUE_WORK_BLOCKING 0
#endif
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define LOG_FMT(...) ""
#define TAG "test"
struct httpd_config_t {int max_open_sockets=4,backlog_conn=5;int task_priority=5,core_id=-1;uint16_t server_port=0,ctrl_port=0;unsigned stack_size=32768,task_caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;int lru_purge_enable=0,recv_wait_timeout=0,send_wait_timeout=0;};
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
struct httpd_data {httpd_config_t config;int listen_fd=-1,ctrl_fd=-1,msg_fd=-1;struct {void *handle=nullptr;}hd_td;void*ctrl_sock_semaphore=nullptr;};
using httpd_handle_t=httpd_data*;
std::set<int> sockets;unsigned liveInstances=0,liveSemaphores=0;bool failTask=false,failAllocation=false;uint16_t listenerPort=0,controlPort=0;httpd_data *lastInstance=nullptr;
int trackedSocket(int a,int b,int c){int fd=::socket(a,b,c);if(fd>=0)assert(sockets.insert(fd).second);return fd;}
int trackedClose(int fd){assert(sockets.erase(fd)==1);return ::close(fd);}
int cs_create_ctrl_sock(int port){int fd=trackedSocket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return -1;sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(::bind(fd,(sockaddr*)&address,sizeof(address))<0){trackedClose(fd);return -1;}return fd;}
void cs_free_ctrl_sock(int fd){trackedClose(fd);}
httpd_data *httpd_create(const httpd_config_t *config){if(failAllocation)return nullptr;++liveInstances;auto *hd=new httpd_data;hd->config=*config;lastInstance=hd;return hd;}
void httpd_delete(httpd_data *hd){--liveInstances;delete hd;}
void *xSemaphoreCreateCounting(int,int){++liveSemaphores;return reinterpret_cast<void*>(1);}
void vSemaphoreDelete(void*){assert(liveSemaphores>0);--liveSemaphores;}
#define CONFIG_LWIP_UDP_RECVMBOX_SIZE 6
void httpd_sess_init(httpd_data*){}
void httpd_thread(void*){}
int httpd_os_thread_create(void **handle,const char*,unsigned stack,int,void(*)(void*),void *arg,int,unsigned caps){
 auto *hd=static_cast<httpd_data*>(arg);auto port=[](int fd){sockaddr_storage address{};socklen_t size=sizeof(address);assert(!getsockname(fd,(sockaddr*)&address,&size));return ntohs(address.ss_family==AF_INET6?reinterpret_cast<sockaddr_in6*>(&address)->sin6_port:reinterpret_cast<sockaddr_in*>(&address)->sin_port);};listenerPort=port(hd->listen_fd);controlPort=port(hd->ctrl_fd);
 if(failTask||((caps&MALLOC_CAP_INTERNAL)&&stack>8192))return ESP_FAIL;*handle=reinterpret_cast<void*>(1);return ESP_OK;
}
void esp_http_server_dispatch_event(int,void*,int){}
#define socket trackedSocket
#define close trackedClose
'''
server_init=function(source,'static esp_err_t httpd_server_init(')
start=function(source,'esp_err_t httpd_start(')
patched_start=function(patcher.patch(source),'esp_err_t httpd_start(')
suffix=r'''
#undef socket
#undef close
void stop(httpd_handle_t hd){trackedClose(hd->msg_fd);cs_free_ctrl_sock(hd->ctrl_fd);trackedClose(hd->listen_fd);
#if CONFIG_HTTPD_QUEUE_WORK_BLOCKING
 vSemaphoreDelete(hd->ctrl_sock_semaphore);
#endif
 httpd_delete(hd);}
int main(){httpd_handle_t handle=nullptr;httpd_config_t cfg;
#ifdef BASELINE
 failTask=true;assert(httpd_start(&handle,&cfg)==ESP_ERR_HTTPD_TASK);assert(handle==nullptr&&sockets.size()==3);cfg.server_port=listenerPort;cfg.ctrl_port=controlPort;failTask=false;assert(httpd_start(&handle,&cfg)==ESP_FAIL);for(int fd:std::set<int>(sockets))trackedClose(fd);
#else
 cfg.task_caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
 for(unsigned i=0;i<100;++i){failTask=true;assert(httpd_start(&handle,&cfg)==ESP_ERR_HTTPD_TASK);assert(!handle&&sockets.empty()&&liveInstances==0&&liveSemaphores==0);cfg.server_port=listenerPort;cfg.ctrl_port=controlPort;failTask=false;assert(httpd_start(&handle,&cfg)==ESP_OK&&handle);assert(sockets.size()==3);stop(handle);handle=nullptr;assert(sockets.empty()&&liveInstances==0&&liveSemaphores==0);}
 failAllocation=true;assert(httpd_start(&handle,&cfg)==ESP_ERR_HTTPD_ALLOC_MEM);assert(!handle&&sockets.empty());
#endif
 std::cout<<"HTTPD startup socket lifecycle passed\n";
}
'''
# Exercise the actual native share configuration and publish/registration logic.
share=function((ROOT/'main/network.cpp').read_text(),'bool share(bool on,')
native_stubs=r'''
#undef socket
#undef close
int httpd_start(httpd_handle_t *handle,const httpd_config_t *config){assert(config->server_port==8080&&config->ctrl_port==32769);auto cfg=*config;cfg.server_port=cfg.ctrl_port=0;return httpd_start_impl(handle,&cfg);}
std::string token,transferPage="page",shareError;bool linked=true;httpd_handle_t server=nullptr;unsigned registrations=0,failRegistration=0;
struct httpd_req_t{};
struct httpd_uri_t{const char *uri=nullptr;int method=0;int(*handler)(httpd_req_t*)=nullptr;};
std::string address(){return "192.0.2.5";}
void esp_fill_random(void *data,size_t n){memset(data,1,n);}
namespace tab5 {struct Network {static std::string transferToken(const std::array<uint8_t,8>&){return "TEST-TOKEN";}};}
const char *esp_err_to_name(int value){return value==ESP_ERR_HTTPD_TASK?"ESP_ERR_HTTPD_TASK":value==ESP_ERR_HTTPD_ALLOC_MEM?"ESP_ERR_HTTPD_ALLOC_MEM":"ESP_FAIL";}
size_t heap_caps_get_free_size(unsigned caps){return caps&MALLOC_CAP_SPIRAM?8000000:20000;}
size_t heap_caps_get_largest_free_block(unsigned caps){return caps&MALLOC_CAP_SPIRAM?4000000:8192;}
int httpd_resp_set_type(httpd_req_t*,const char*){return 0;}int httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){return 0;}int httpd_resp_send(httpd_req_t*,const char*,size_t){return 0;}
int listing(httpd_req_t*){return 0;}int transfer(httpd_req_t*){return 0;}
int httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*){return ++registrations==failRegistration?ESP_FAIL:ESP_OK;}
int httpd_stop(httpd_handle_t hd){trackedClose(hd->msg_fd);cs_free_ctrl_sock(hd->ctrl_fd);trackedClose(hd->listen_fd);httpd_delete(hd);return ESP_OK;}
'''
native_main=r'''
int main(){std::string ip,key;failTask=true;assert(!share(true,ip,key));assert(!server&&token.empty()&&sockets.empty());assert(shareError.find("ESP_ERR_HTTPD_TASK")!=shareError.npos);assert(shareError.find("PSRAM")!=shareError.npos);failTask=false;
 for(unsigned i=0;i<50;++i){registrations=0;failRegistration=0;assert(share(true,ip,key));assert(server&&ip=="192.0.2.5:8080"&&key=="TEST-TOKEN");assert(lastInstance->config.task_caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));assert(share(true,ip,key)&&sockets.size()==3);assert(share(false,ip,key)&&!server&&token.empty()&&sockets.empty());}
 for(unsigned fail=1;fail<=5;++fail){registrations=0;failRegistration=fail;assert(!share(true,ip,key));assert(!server&&token.empty()&&sockets.empty());}
 linked=false;assert(!share(true,ip,key)&&!server&&sockets.empty());std::cout<<"Native PSRAM stack, retries, registrations and stop passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='tab5-httpd-startup-') as folder:
 folder=Path(folder)
 for name,text,flags in [
  ('baseline',prelude+server_init+start+suffix,['-DBASELINE']),
  ('patched',prelude+server_init+patched_start+suffix,[]),
  ('patched-ipv6',prelude+server_init+patched_start+suffix,['-DCONFIG_LWIP_IPV6=1']),
  ('patched-semaphore',prelude+server_init+patched_start+suffix,['-DCONFIG_HTTPD_QUEUE_WORK_BLOCKING=1']),
  ('native',prelude+server_init+patched_start.replace('esp_err_t httpd_start(', 'esp_err_t httpd_start_impl(')+native_stubs+share+native_main,[])]:
  cpp=folder/(name+'.cpp');cpp.write_text(text);exe=folder/name
  sdk=['-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk','-Wno-reorder-init-list'] if sys.platform=='darwin' and Path('/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk').is_dir() else []
  subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'c++','-std=c++17',*sdk,*flags,str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('Unpatched TASK→FAIL reproduced; patched sockets/heap and native sharing verified.')
