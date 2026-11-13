// SPDX-License-Identifier: MIT
// Original bounded UART/GATT byte transports. Protocol/UI live on the SD.
#include "runtime.hpp"
#include "radio_rx.hpp"
#include "esp_heap_caps.h"
#include "bsp/m5stack_tab5.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_hosted.h"
#include "esp_timer.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
#include "store/config/ble_store_config.h"
#include <mutex>
#include <deque>
#include <cstdlib>
#include <cstring>
extern "C" void ble_store_config_init(void);
#if defined(CONFIG_BT_NIMBLE_NVS_PERSIST) || defined(CONFIG_NIMBLE_NVS_PERSIST)
#error "BLE credentials must never be persisted in NVS"
#endif
using namespace tab5;
namespace {
std::recursive_mutex guard;
std::string state="Desconectado",mode,scanResults,pendingOp,pendingArg;
bool command(const std::string &,const std::string &,std::string &);
RadioInbox incoming;SerialFrames serialParser;
RadioFrame *framePool=nullptr;TaskHandle_t uartTask=nullptr;
uint64_t uartRx=0,uartTx=0,uartRequests=0;
bool grovePowered=false;
bool prepareInbox(std::string &out) {
 if(framePool) return true;
 framePool=static_cast<RadioFrame *>(heap_caps_calloc(256,sizeof(RadioFrame),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!framePool) {out="PSRAM indisponível para receber o rádio";return false;}
 incoming.attach(framePool,256);return true;
}
bool started=false,synced=false,reading=false;
uint8_t ownType=0;
uint16_t conn=BLE_HS_CONN_HANDLE_NONE,toRadio=0,fromRadio=0;
uint32_t passkey=0;
int64_t lastRead=0;
ble_uuid_any_t svcUuid,toUuid,fromUuid;
constexpr uart_port_t port=UART_NUM_2;
void receive(const std::string &frame) {if(!frame.empty())incoming.push(frame.data(),frame.size());}
int onRead(uint16_t,const ble_gatt_error *error,ble_gatt_attr *attr,void *) {
  std::lock_guard<std::recursive_mutex> lock(guard);reading=false;
  if(!error->status && attr && attr->om) {size_t n=OS_MBUF_PKTLEN(attr->om);if(n && n<=512) {std::string data(n,'\0');os_mbuf_copydata(attr->om,0,n,&data[0]);receive(std::move(data));}}
  else if(error->status) {state="Falha de leitura BLE "+std::to_string(error->status);if(conn!=BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);}
  return 0;
}
int characteristics(uint16_t,const ble_gatt_error *error,const ble_gatt_chr *chr,void *) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(!error->status && chr) {if(!ble_uuid_cmp(&chr->uuid.u,&toUuid.u)) toRadio=chr->val_handle;if(!ble_uuid_cmp(&chr->uuid.u,&fromUuid.u)) fromRadio=chr->val_handle;}
  if(error->status==BLE_HS_EDONE) state=toRadio && fromRadio?"Conectado BLE":"Serviço Meshtastic incompleto";
  else if(error->status) state="Falha ao descobrir características";
  return 0;
}
int service(uint16_t c,const ble_gatt_error *error,const ble_gatt_svc *svc,void *) {
  if(!error->status && svc) return ble_gattc_disc_all_chrs(c,svc->start_handle,svc->end_handle,characteristics,nullptr);
  return 0;
}
int mtuReady(uint16_t c,const ble_gatt_error *error,uint16_t,void *) {if(!error->status) ble_gattc_disc_svc_by_uuid(c,&svcUuid.u,service,nullptr);else {std::lock_guard<std::recursive_mutex> lock(guard);state="Falha de MTU BLE";}return 0;}
void discover() {int rc=ble_gattc_exchange_mtu(conn,mtuReady,nullptr);if(rc) state="Falha ao iniciar MTU BLE";}
int gap(ble_gap_event *event,void *) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  switch(event->type) {
    case BLE_GAP_EVENT_DISC: {
      ble_hs_adv_fields fields{};if(ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data)) break;
      bool match=false;for(int i=0;i<fields.num_uuids128;++i) if(!ble_uuid_cmp(&fields.uuids128[i].u,&svcUuid.u)) match=true;
      if(!match || scanResults.size()>3000) break;
      const auto &a=event->disc.addr;char address[32];snprintf(address,sizeof(address),"%02x:%02x:%02x:%02x:%02x:%02x,%u",a.val[5],a.val[4],a.val[3],a.val[2],a.val[1],a.val[0],a.type);
      if(scanResults.find(address)!=std::string::npos) break;
      std::string name=fields.name?std::string(reinterpret_cast<const char *>(fields.name),fields.name_len):"Meshtastic";
      for(char &ch:name) if(ch=='\n' || ch=='|' || ch=='\t' || ch=='\r') ch=' ';
      scanResults+=std::string(address)+"|"+name+"\n";break;
    }
    case BLE_GAP_EVENT_CONNECT:
      if(event->connect.status) state="Falha de conexão BLE "+std::to_string(event->connect.status);
      else {conn=event->connect.conn_handle;state="Pareando BLE";int rc=ble_gap_security_initiate(conn);if(rc==BLE_HS_EALREADY) discover();else if(rc) state="Falha de segurança BLE "+std::to_string(rc);}
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if(!event->enc_change.status) discover();else state="Pareamento recusado";break;
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
      ble_sm_io io{};io.action=event->passkey.params.action;
      if(io.action==BLE_SM_IOACT_INPUT) {io.passkey=passkey;ble_sm_inject_io(conn,&io);} else state="Pareamento requer entrada de PIN no Tab5";
      break;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING: return BLE_GAP_REPEAT_PAIRING_IGNORE;
    case BLE_GAP_EVENT_DISCONNECT: conn=BLE_HS_CONN_HANDLE_NONE;toRadio=fromRadio=0;reading=false;state="BLE desconectado";break;
    case BLE_GAP_EVENT_DISC_COMPLETE: state="Busca BLE concluída";break;
    default:break;
  }
  return 0;
}
void sync() {std::lock_guard<std::recursive_mutex> lock(guard);synced=ble_hs_id_infer_auto(0,&ownType)==0;state=synced?"BLE pronto":"BLE sem endereço";if(synced && !pendingOp.empty()) {auto op=pendingOp,arg=pendingArg;pendingOp.clear();pendingArg.clear();std::string ignored;command(op,arg,ignored);}}
void task(void *) {nimble_port_run();nimble_port_freertos_deinit();}
bool initBle(std::string &out) {
  if(started) return true;
  if(!prepareInbox(out)) return false;
  if(bsp_feature_enable(BSP_FEATURE_WIFI,true)!=ESP_OK || esp_hosted_init()!=ESP_OK || nimble_port_init()!=ESP_OK) {out="Falha ao iniciar BLE/C6";return false;}
  ble_uuid_from_str(&svcUuid,"6ba1b218-15a8-461f-9fa8-5dcae273eafd");ble_uuid_from_str(&toUuid,"f75c76d2-129e-4dad-a1dd-7866124401e7");ble_uuid_from_str(&fromUuid,"2c55e69e-4993-11ed-b878-0242ac120002");
  ble_hs_cfg.sync_cb=sync;ble_hs_cfg.sm_io_cap=BLE_HS_IO_KEYBOARD_ONLY;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;
  ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;ble_hs_cfg.sm_their_key_dist=ble_hs_cfg.sm_our_key_dist;
  // Bonds are session-only RAM state. Re-enter the radio PIN after a restart;
  // no pairing credential is written outside /SymbianOS.
  ble_store_config_init();ble_att_set_preferred_mtu(512);nimble_port_freertos_init(task);started=true;return true;
}
void disconnect() {
  if(conn!=BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);
  if(started) ble_gap_disc_cancel();
  if(mode=="uart") uart_driver_delete(port);
  mode.clear();serialParser.reset();incoming.clear();pendingOp.clear();pendingArg.clear();toRadio=fromRadio=0;state="Desconectado";
}
void pollSerial() {
 char bytes[512];
 for(unsigned attempt=0;attempt<8;++attempt) {
  int n=uart_read_bytes(port,bytes,sizeof(bytes),0);if(n<=0)break;uartRx+=unsigned(n);
  serialParser.feed(bytes,size_t(n),[](const char *data,size_t length){incoming.push(data,length);});
 }
}
void pumpUart(void *) {
 for(;;) {
  {std::lock_guard<std::recursive_mutex> lock(guard);if(mode=="uart")pollSerial();}
  vTaskDelay(pdMS_TO_TICKS(10)>0?pdMS_TO_TICKS(10):1);
 }
}
bool command(const std::string &op,const std::string &arg,std::string &out) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(op=="disconnect") {disconnect();return true;}
  if(op=="state") {out=state;return true;}
  if(op=="diagnostic") {out="runtime=1.0.0\nModo="+(mode.empty()?std::string("desconectado"):mode)+"\nPorta=UART2 TX53 RX54 115200 8N1; loopback interno desligado; GPIOs reinicializados, RX somente entrada\nAlimentação Grove="+std::string(grovePowered?"EXT5V habilitada":"não habilitada por este driver")+"\nPedidos UART="+std::to_string(uartRequests)+"\nEstado="+state+"\nUART TX="+std::to_string(uartTx)+" bytes\nUART RX="+std::to_string(uartRx)+" bytes\nQuadros="+std::to_string(serialParser.frames)+"\nInválidos="+std::to_string(serialParser.invalid)+"\nFila="+std::to_string(incoming.size())+" / 256\nPerdidos="+std::to_string(incoming.dropped);return true;}
  if(op=="link") {out=mode=="uart"?"uart":mode=="ble" && conn!=BLE_HS_CONN_HANDLE_NONE && toRadio && fromRadio?"ble":"";return true;}
  if(op=="connect_uart") {
    ++uartRequests;disconnect();if(!prepareInbox(out)){state=out;return false;}
    // The working board setup drives EXT5V_EN (0x43/P2) high. The generic
    // BSP resets this expander without enabling that rail. Use its shared
    // handle and change only P2, preserving display/touch/audio controls.
    auto groveIo=bsp_io_expander_init();
    esp_err_t powerRc=groveIo?esp_io_expander_set_level(groveIo,IO_EXPANDER_PIN_NUM_2,1):ESP_ERR_INVALID_STATE;
    if(powerRc==ESP_OK)powerRc=esp_io_expander_set_dir(groveIo,IO_EXPANDER_PIN_NUM_2,IO_EXPANDER_OUTPUT);
    if(powerRc==ESP_OK)powerRc=esp_io_expander_set_output_mode(groveIo,IO_EXPANDER_PIN_NUM_2,IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
    grovePowered=powerRc==ESP_OK;
    if(!grovePowered){out=state=std::string("Habilitar alimentação Grove: ")+esp_err_to_name(powerRc);return false;}
    uartRx=uartTx=0;uart_config_t cfg{};cfg.baud_rate=115200;cfg.data_bits=UART_DATA_8_BITS;cfg.parity=UART_PARITY_DISABLE;cfg.stop_bits=UART_STOP_BITS_1;cfg.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;cfg.source_clk=UART_SCLK_DEFAULT;
    // uart_set_pin() enables the RX input but does not disable an inherited
    // output route on that GPIO. Release both pads before assigning the UART.
    auto rc=gpio_reset_pin(GPIO_NUM_53);
    if(rc==ESP_OK)rc=gpio_reset_pin(GPIO_NUM_54);
    if(rc==ESP_OK)rc=gpio_set_direction(GPIO_NUM_54,GPIO_MODE_INPUT);
    if(rc!=ESP_OK){out=state=std::string("Reinicializar GPIOs Grove: ")+esp_err_to_name(rc);return false;}
    rc=uart_param_config(port,&cfg);if(rc!=ESP_OK){out=state=std::string("Configurar UART2: ")+esp_err_to_name(rc);return false;}
    rc=uart_set_pin(port,53,54,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);if(rc!=ESP_OK){out=state=std::string("Pinos UART2: ")+esp_err_to_name(rc);return false;}
    rc=uart_driver_install(port,4096,1024,0,nullptr,0);if(rc!=ESP_OK){out=state=std::string("Instalar UART2: ")+esp_err_to_name(rc);return false;}
    rc=uart_set_mode(port,UART_MODE_UART);if(rc==ESP_OK)rc=uart_set_loop_back(port,false);
    if(rc!=ESP_OK){uart_driver_delete(port);out=state=std::string("Modo UART2 sem loopback: ")+esp_err_to_name(rc);return false;}
    vTaskDelay(pdMS_TO_TICKS(200));uart_flush_input(port);
    uart_set_rx_timeout(port,2);uart_set_rx_full_threshold(port,64);
    mode="uart";
    if(!uartTask && xTaskCreate(pumpUart,"mesh-uart-rx",4096,nullptr,4,&uartTask)!=pdPASS) {mode.clear();uart_driver_delete(port);out=state="Falha ao iniciar recepção UART";return false;}
    state="UART aberta; aguardando rádio";return true;
  }
  if(op=="scan" || op=="connect_ble") {
    if(!initBle(out)) return false;
    if(!synced) {pendingOp=op;pendingArg=arg;out=state="BLE iniciando";return true;}
    if(op=="scan") {ble_gap_disc_cancel();scanResults.clear();ble_gap_disc_params p{};p.passive=0;p.filter_duplicates=1;int rc=ble_gap_disc(ownType,10000,&p,gap,nullptr);out=rc?"Falha de busca BLE":"Buscando BLE";state=out;return rc==0;}
    unsigned a[6],type=0,pin=0;
    if(sscanf(arg.c_str(),"%x:%x:%x:%x:%x:%x,%u,%u",&a[5],&a[4],&a[3],&a[2],&a[1],&a[0],&type,&pin)!=8 || type>1 || pin>999999) {out="Endereço ou PIN BLE inválido";return false;}
    ble_addr_t peer{};peer.type=type;for(int i=0;i<6;++i) {if(a[i]>255)return false;peer.val[i]=a[i];}
    disconnect();passkey=pin;mode="ble";state="Conectando BLE";int rc=ble_gap_connect(ownType,&peer,10000,nullptr,gap,nullptr);if(rc) out=state="Falha de conexão BLE "+std::to_string(rc);return rc==0;
  }
  if(op=="devices") {out=scanResults;return true;}
  if(op=="read") {
    if(mode=="uart") pollSerial();
    if(mode=="ble" && reading && esp_timer_get_time()-lastRead>10000000) {reading=false;ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);}
    if(mode=="ble" && fromRadio && !reading && esp_timer_get_time()-lastRead>500000) {reading=true;lastRead=esp_timer_get_time();if(ble_gattc_read(conn,fromRadio,onRead,nullptr)) reading=false;}
    incoming.pop(out);return true;
  }
  if(op=="send" && !arg.empty() && arg.size()<=512) {
    if(mode=="uart") {char frame[516]={char(0x94),char(0xc3),char(arg.size()>>8),char(arg.size())};memcpy(frame+4,arg.data(),arg.size());
      int written=uart_write_bytes(port,frame,arg.size()+4);
      if(written>0)uartTx+=unsigned(written);
      bool ok=written==int(arg.size()+4);if(!ok)out="Falha de escrita UART";return ok;}
    if(mode=="ble" && toRadio) {if(arg.size()>size_t(ble_att_mtu(conn)-3)) {out="MTU BLE insuficiente";return false;}int rc=ble_gattc_write_flat(conn,toRadio,arg.data(),arg.size(),nullptr,nullptr);if(rc)out="Falha de escrita BLE "+std::to_string(rc);return rc==0;}
  }
  out="Rádio não conectado";return false;
}
}
void tab5_setup_radio(Runtime &runtime) {runtime.radioCommand=command;}
