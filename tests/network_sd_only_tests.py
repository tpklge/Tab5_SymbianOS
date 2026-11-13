# SPDX-License-Identifier: MIT
"""Compile the real native initialization functions against a missing-NVS fixture.

No radio/flash is accessed. API failures are simulated; this does not validate
association or the installed C6 firmware on the physical Tab5.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def function(path, signature):
    source = path.read_text()
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

common = r'''
#include <cassert>
#include <string>
#include <iostream>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_ERR_NOT_FOUND=261, ESP_ERR_INVALID_STATE=259;
constexpr int BSP_FEATURE_WIFI=1;
static unsigned nvsCalls=0;
int nvs_flash_init(){++nvsCalls;return ESP_ERR_NOT_FOUND;}
int bsp_feature_enable(int,bool){return ESP_OK;}
int esp_hosted_init(){return ESP_OK;}
'''
wifi = r'''
bool transportFailed=false,initialized=false,wifiReady=false;
void *station=nullptr;
int loopResult=ESP_OK,netResult=ESP_OK,wifiResult=ESP_OK,storageResult=ESP_OK;
unsigned initCalls=0,deinitCalls=0,ramCalls=0;
std::string diagnostic;
constexpr int WIFI_EVENT=1,IP_EVENT=2,ESP_EVENT_ANY_ID=-1,WIFI_STORAGE_RAM=0;
bool checked(int code,const char *stage){if(code==ESP_OK)return true;diagnostic=stage;return false;}
int esp_netif_init(){return netResult;}
int esp_event_loop_create_default(){return loopResult;}
void *esp_netif_create_default_wifi_sta(){return reinterpret_cast<void *>(1);}
void event(void*,int,int,void*){}
int esp_event_handler_register(int,int,decltype(&event),void*){return ESP_OK;}
int esp_event_handler_unregister(int,int,decltype(&event)){return ESP_OK;}
struct wifi_init_config_t {int nvs_enable=1;};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
int esp_wifi_init(wifi_init_config_t *cfg){++initCalls;assert(cfg->nvs_enable==0);return wifiResult;}
int esp_wifi_set_storage(int storage){++ramCalls;assert(storage==WIFI_STORAGE_RAM);return storageResult;}
int esp_wifi_deinit(){++deinitCalls;return ESP_OK;}
void endStationEvents(){}
'''
wifi_checks = r'''
int main(){
 assert(nvs_flash_init()==ESP_ERR_NOT_FOUND);nvsCalls=0;
 assert(init() && wifiReady && initialized && nvsCalls==0);
 assert(initCalls==1 && ramCalls==1 && init());assert(initCalls==1);
 wifiReady=false;wifiResult=ESP_ERR_NOT_FOUND;
 assert(!init() && !wifiReady && diagnostic=="Inicializar driver Wi-Fi C6");
 wifiResult=ESP_OK;storageResult=ESP_ERR_NOT_FOUND;
 assert(!init() && !wifiReady && deinitCalls==1 && diagnostic=="Configurar Wi-Fi em RAM");
 storageResult=ESP_OK;assert(init() && wifiReady);
 assert(nvsCalls==0);
 std::cout<<"Native Wi-Fi init: missing NVS accepted, remote config/storage use RAM; genuine driver errors preserved\n";
}
'''
ble = r'''
bool started=false;
int svcUuid=0,toUuid=0,fromUuid=0;
unsigned storeCalls=0,hostCalls=0;
bool prepareInbox(std::string&){return true;}
int nimble_port_init(){return ESP_OK;}
void sync(){}void task(void*){}
int ble_uuid_from_str(int*,const char*){return 0;}
struct Config {decltype(&sync) sync_cb=nullptr;int sm_io_cap=0,sm_bonding=0,sm_mitm=0,sm_sc=0,sm_our_key_dist=0,sm_their_key_dist=0;}ble_hs_cfg;
constexpr int BLE_HS_IO_KEYBOARD_ONLY=1,BLE_SM_PAIR_KEY_DIST_ENC=1,BLE_SM_PAIR_KEY_DIST_ID=2;
void ble_store_config_init(){++storeCalls;}
int ble_att_set_preferred_mtu(int mtu){assert(mtu==512);return 0;}
void nimble_port_freertos_init(decltype(&task)){++hostCalls;}
'''
ble_checks = r'''
int main(){std::string out;
 assert(initBle(out) && started && nvsCalls==0 && storeCalls==1 && hostCalls==1);
 assert(initBle(out) && nvsCalls==0 && storeCalls==1);
 std::cout<<"Native BLE init: no NVS dependency, RAM store remains session-only\n";
}
'''
with tempfile.TemporaryDirectory(prefix='tab5-network-sd-only-') as directory:
    for name, prelude, path, signature, checks in (
        ('wifi', wifi, ROOT/'main/network.cpp', 'bool init()', wifi_checks),
        ('ble', ble, ROOT/'main/radio.cpp', 'bool initBle(std::string &out)', ble_checks),
    ):
        source = Path(directory)/(name+'.cpp')
        source.write_text(common+prelude+function(path,signature)+checks)
        binary = source.with_suffix('')
        command = [sys.argv[1] if len(sys.argv)>1 else 'c++', '-std=c++17', str(source), '-o', str(binary)]
        sdk = Path('/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk')
        if sys.platform == 'darwin' and sdk.exists():
            command += ['-isysroot', str(sdk), '-mmacosx-version-min=26.0']
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)

# These are build/packaging invariants, independent of the API simulation.
for name in ('sdkconfig.defaults', 'sdkconfig.release'):
    config = (ROOT/name).read_text()
    for key in ('CONFIG_BT_NIMBLE_NVS_PERSIST', 'CONFIG_NIMBLE_NVS_PERSIST',
                'CONFIG_WIFI_RMT_NVS_ENABLED', 'CONFIG_ESP_WIFI_NVS_ENABLED'):
        assert key+'=y' not in config, (name, key)
print('Application defaults/release disable NVS credential persistence')
