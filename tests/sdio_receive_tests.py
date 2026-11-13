#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise installed ESP-Hosted receive functions, without SDIO hardware.
Upstream function text is compiled only in the temporary build fixture. Its
copyright/license stays in the installed component; no vendor implementation is
vendored into this test. Registers/queue delivery are simulated, not measured.
"""
import pathlib, re, subprocess, sys, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'managed_components/espressif__esp_hosted'
source = (VENDOR / 'host/drivers/transport/sdio/sdio_drv.c').read_text()
def function(signature, last=False):
    start = source.rfind(signature) if last else source.index(signature)
    begin = source.index('{', start); depth = 1; end = begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}'); end += 1
    return source[start:end]
reg = (VENDOR / 'host/drivers/transport/sdio/sdio_reg.h').read_text()
constants = '\n'.join(re.search(r'^#define '+name+r'\s+[^\n]+',reg,re.M)[0]
                      for name in ('ESP_SLAVE_LEN_MASK','ESP_RX_BYTE_MAX','ESP_RX_BUFFER_SIZE'))
length = function('static int sdio_get_len_from_slave(uint32_t *rx_size, uint32_t reg_val')
valid = function('static int is_valid_sdio_rx_packet(')
stream = function('static esp_err_t sdio_push_data_to_queue(',last=True)
prelude = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdio>
#include "esp_hosted_header.h"
#define H_SDIO_HOST_STREAMING_MODE 1
#define H_SDIO_CHECKSUM 0
#define ESP_PKT_STATS 0
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define MEMSET_REQUIRED 1
#define le16toh(x) (x)
#define MAX_PAYLOAD_SIZE 1524
using esp_err_t = int;
static uint32_t sdio_rx_byte_count;
static unsigned frames;
static uint8_t* sdio_buffer_alloc(int) {return (uint8_t*)calloc(1,1536);}
static int sdio_push_pkt_to_queue(uint8_t* b,uint16_t len,uint16_t offset) {
 assert(len && offset==sizeof(esp_payload_header)); ++frames; free(b);return 0;
}
'''
main = r'''
static void frame(std::vector<uint8_t>& b,unsigned size,unsigned type) {
 const auto start=b.size();b.resize(start+size,0);
 esp_payload_header h{};h.if_type=type;h.len=size-sizeof(h);h.offset=sizeof(h);
 memcpy(b.data()+start,&h,sizeof(h));
}
int main() {
 uint32_t received=0;
 // A lone first frame is accepted in both modes, as in the old 1363-byte RSS symptom.
 sdio_rx_byte_count=0;
 assert(sdio_get_len_from_slave(&received,1363,false)==0 && received==1363);
 sdio_rx_byte_count=1363;
 std::vector<uint8_t> burst;frame(burst,1514,0);frame(burst,1514,0);frame(burst,39,2);
 const auto reg=(sdio_rx_byte_count+burst.size())%ESP_RX_BYTE_MAX;
#if H_SDIO_HOST_RX_MODE != H_SDIO_HOST_STREAMING_MODE
 // Old config rejects an aggregated burst and never advances the consumed
 // counter. The next control response cannot fix the rejected unread backlog.
 assert(sdio_get_len_from_slave(&received,reg,false)==ESP_FAIL && received==0);
 assert(sdio_get_len_from_slave(&received,(reg+39)%ESP_RX_BYTE_MAX,false)==ESP_FAIL);
 puts("Legacy packet RX: reproduced rejection/stalled byte counter after first frame.");
#else
 assert(sdio_get_len_from_slave(&received,reg,false)==0 && received==burst.size());
 assert(sdio_push_data_to_queue(burst.data(),received)==0 && frames==3);
 sdio_rx_byte_count=ESP_RX_BYTE_MAX-100;
 assert(sdio_get_len_from_slave(&received,(sdio_rx_byte_count+burst.size())%ESP_RX_BYTE_MAX,false)==0);
 assert(received==burst.size());
 for(unsigned round=0;round<10000;++round) {
   assert(sdio_push_data_to_queue(burst.data(),burst.size())==0);
   sdio_rx_byte_count=(sdio_rx_byte_count+burst.size())%ESP_RX_BYTE_MAX;
 }
 assert(frames==30003);
 // Packet-mode co-processors' individual frames are also valid streams of one.
 std::vector<uint8_t> single;frame(single,39,2);
 assert(sdio_push_data_to_queue(single.data(),single.size())==0 && frames==30004);
 puts("Streaming RX: data + control frames, 10000 bursts, counter wrap and single frames passed.");
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='tab5-sdio-') as tmp:
    for mode in (3,1):
        p=pathlib.Path(tmp)/f'rx{mode}.cpp'
        p.write_text(prelude+constants+f'\n#define H_SDIO_HOST_RX_MODE {mode}\n'+length+'\n'+valid+'\n'+stream+'\n'+main)
        out=p.with_suffix('')
        cmd=[sys.argv[1] if len(sys.argv)>1 else 'c++','-std=c++17','-I'+str(VENDOR/'common'),str(p),'-o',str(out)]
        if sys.platform=='darwin' and pathlib.Path('/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk').exists():
            cmd+=['-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk','-mmacosx-version-min=26.0']
        if '--sanitize' in sys.argv:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(out)],check=True)
# A passing algorithm test is insufficient if the native image still selects packet RX.
assert 'CONFIG_ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE=y' in (ROOT/'sdkconfig.defaults').read_text(), 'native defaults still select incompatible packet RX'
print('Project default selects streaming RX.')
