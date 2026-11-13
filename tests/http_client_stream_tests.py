#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the installed SDK's actual streaming read function with fake transport."""
import importlib.util, os, subprocess, sys, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sdk=Path(os.environ.get('IDF_PATH',Path.home()/'esp/esp-idf'))
source=(sdk/'components/esp_http_client/esp_http_client.c').read_text()
spec=importlib.util.spec_from_file_location('patch_http_client',root/'tools/patch_http_client.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def function(text):
 a=text.index('int esp_http_client_read(');return text[a:text.index('\nesp_err_t esp_http_client_perform(',a)].replace('"PRId64','" PRId64')
prelude=r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <cerrno>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOG_LEVEL(...) ((void)0)
#define TAG "test"
using esp_log_level_t=int;using esp_err_t=int;
constexpr int ESP_LOG_WARN=1,ESP_LOG_DEBUG=2,ESP_FAIL=-1,ESP_ERR_HTTP_EAGAIN=0x7007,HTTP_EVENT_ERROR=1;
constexpr int ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN=-2,ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT=0;
struct Buffer {int raw_len=0;char *raw_data=nullptr,*output_ptr=nullptr;char data[4096]{};};
using esp_http_buffer_t=Buffer;
struct Response {Buffer *buffer;bool is_chunked=false;int64_t data_process=0,content_length=-1;};
struct Client {Response *response;bool is_chunk_complete=false,terminal=true,fin=true,timeout=false;size_t at=0;std::string wire;void *transport,*parser,*parser_settings=nullptr;int buffer_size_rx=4096,timeout_ms=3000;};
using esp_http_client_handle_t=Client*;
int reads=0;
int esp_transport_read(void *ctx,char *data,int n,int){auto &c=*static_cast<Client*>(ctx);++reads;if(c.timeout){c.timeout=false;return 0;}if(c.at==c.wire.size())return c.fin?-2:-3;n=std::min(size_t(n),c.wire.size()-c.at);memcpy(data,c.wire.data()+c.at,n);c.at+=n;return n;}
size_t http_parser_execute(void *parser,void*,const char *data,size_t n){auto &c=*static_cast<Client*>(parser);if(!n){if(c.terminal)c.is_chunk_complete=true;return 0;}auto &b=*c.response->buffer;memcpy(b.output_ptr,data,n);b.raw_len=int(n);c.response->data_process+=n;if(c.response->is_chunked&&c.at==c.wire.size()&&c.terminal)c.is_chunk_complete=true;return n;}
bool esp_http_client_is_complete_data_received(Client*c){return c->response->is_chunked?c->is_chunk_complete:c->response->data_process==c->response->content_length;}
void esp_http_client_cached_buf_cleanup(Buffer*){}
esp_err_t esp_transport_translate_error(int){return -1;}
const char *esp_err_to_name(int){return "error";}
void *esp_transport_get_error_handle(void*){return nullptr;}
void http_dispatch_event(Client*,int,void*,int){}
void http_dispatch_event_to_event_loop(int,void*,size_t){}
'''
main=r'''
std::string consume(Client &c,int &end){std::string out;char b[1024];for(int i=0;i<100;i++){int n=esp_http_client_read(&c,b,sizeof(b));if(n==-ESP_ERR_HTTP_EAGAIN)continue;if(n<=0){end=n;return out;}out.append(b,n);}assert(false);return {};}
int main(){
 Buffer b;Response r{&b};Client c{&r};c.transport=c.parser=&c;c.wire=std::string(5000,'x');int end=1;reads=0;
 auto text=consume(c,end);
#ifdef BASELINE
 assert(text.empty()&&end==0&&reads==0);std::cout<<"Baseline reproduced: unknown-length body skipped\n";return 0;
#else
 assert(text==c.wire&&end==0);
 for(int kind=0;kind<5;kind++){
  b={};r={&b};c={&r};c.transport=c.parser=&c;c.wire=std::string(5000,'x');
  if(kind==0)r.content_length=c.wire.size();
  if(kind==1)c.timeout=true;
  if(kind==2){r.content_length=c.wire.size()+100;c.terminal=false;}
  if(kind>=3){r.is_chunked=true;c.terminal=kind==3;}
  text=consume(c,end);assert(text==c.wire);assert(end==((kind==2||kind==4)?ESP_FAIL:0));
 }
 b={};r={&b};c={&r};c.transport=c.parser=&c;c.wire="data";c.fin=false;text=consume(c,end);assert(text=="data"&&end==ESP_FAIL);
 std::cout<<"Actual SDK read: close framing, known length, transient timeout, strict truncation/chunks/reset passed\n";
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='tab5-http-read-') as directory:
 for label,text,flags in [('baseline',source,['-DBASELINE']),('patched',module.patch(source),[])]:
  path=Path(directory)/f'{label}.cpp';exe=path.with_suffix('');path.write_text(prelude+function(text)+main)
  sdk_flags=['-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk'] if sys.platform=='darwin' else []
  subprocess.run([sys.argv[1],'-std=c++17',*sdk_flags,*flags,str(path),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
try:module.patch('int esp_http_client_read(\nesp_err_t esp_http_client_perform(')
except ValueError:pass
else:raise AssertionError('changed SDK must fail closed')
