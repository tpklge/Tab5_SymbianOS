// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include <cstdarg>
#include <cstdio>
#include "esp_heap_caps.h"
#include "esp_hosted_api.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "http_fetch.hpp"
#include "http_body.hpp"
#include "browser_profile.hpp"
#include "esp_timer.h"
#include "runtime.hpp"
#include "wifi_lifecycle.hpp"
extern "C" {
#include "hosted_os_abstraction.h"
}
#if !defined(CONFIG_ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE) || !CONFIG_ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE
#error "Tab5 factory C6 requires ESP-Hosted streaming RX; packet RX stalls aggregated traffic."
#endif
#if !CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
#error "File transfer HTTPD requires PSRAM task stacks"
#endif
#if CONFIG_WIFI_RMT_NVS_ENABLED || CONFIG_ESP_WIFI_NVS_ENABLED
#error "Wi-Fi preferences must be kept in RAM/SD, never NVS"
#endif
#include <atomic>
#include <cstring>
#include <cerrno>
#include <cstdlib>
std::string tab5_wifi_transport_info();
namespace {
tab5::Runtime *systemRuntime = nullptr;
esp_netif_t *station = nullptr;
std::atomic<bool> linked{false}, associated{false}, scanDone{false};
std::atomic<bool> transportFailed{false};
std::mutex wifiEventMutex;
tab5::WifiLifecycle staEvents, apEvents;
using WifiEventPost = int (*)(int32_t, void *, size_t, uint32_t);
WifiEventPost originalWifiPost = nullptr;
int filteredWifiEventPost(int32_t id, void *data, size_t size, uint32_t ticks) {
  // ESP-Hosted/C6 may report START twice. Default ESP-IDF handlers add the
  // same lwIP netif on every START and abort. Filter before the event queue,
  // including before association (esp_netif_is_netif_up is insufficient).
  std::lock_guard<std::mutex> guard(wifiEventMutex);
  if (!originalWifiPost) return ESP_ERR_INVALID_STATE;
  auto deliver = [&] { return originalWifiPost(id, data, size, ticks); };
  using Event = tab5::WifiLifecycle::Event;
  if (id == WIFI_EVENT_STA_START) return staEvents.post(Event::Start, deliver);
  if (id == WIFI_EVENT_STA_STOP) return staEvents.post(Event::Stop, deliver);
  if (id == WIFI_EVENT_AP_START) return apEvents.post(Event::Start, deliver);
  if (id == WIFI_EVENT_AP_STOP) return apEvents.post(Event::Stop, deliver);
  return deliver();
}
void endStationEvents() {
  // Successful deinit also ends the interface lifetime if the remote STOP
  // event was lost. Queue removal before accepting another START.
  filteredWifiEventPost(WIFI_EVENT_STA_STOP, nullptr, 0, portMAX_DELAY);
}
// Fixed ring: log callbacks never allocate or access the SD. Keep only transport
// warning/error lines; email/authentication payloads are deliberately excluded.
portMUX_TYPE transportLogLock = portMUX_INITIALIZER_UNLOCKED;
char transportLines[12][256]{};
unsigned transportLineCount = 0;
vprintf_like_t previousLog = nullptr;
int transportLog(const char *format, va_list arguments) {
  char line[256]; va_list copy; va_copy(copy,arguments);
  vsnprintf(line,sizeof(line),format,copy); va_end(copy);
  const bool severity = strstr(line,"E (") || strstr(line,"W (");
  if(severity && (strstr(line,"H_SDIO_DRV:") || strstr(line,"sdmmc_req:") || strstr(line,"sdmmc_io:") || strstr(line,"transport:"))) {
    portENTER_CRITICAL(&transportLogLock);
    memcpy(transportLines[transportLineCount++ % 12],line,sizeof(line));
    portEXIT_CRITICAL(&transportLogLock);
  }
  return previousLog ? previousLog(format,arguments) : vprintf(format,arguments);
}
std::string transportHistory() {
  char snapshot[12][256]; unsigned count;
  portENTER_CRITICAL(&transportLogLock);
  count=transportLineCount; memcpy(snapshot,transportLines,sizeof(snapshot));
  portEXIT_CRITICAL(&transportLogLock);
  std::string result="runtime=1.0.0\nÚltimos avisos/erros SDIO (até 12):\n";
  for(unsigned i=count>12?count-12:0;i<count;++i) result+=snapshot[i%12];
  if(!count)result+="Nenhum aviso/erro SDIO capturado; isso não comprova transporte saudável.\n";
  return result;
}
std::atomic<unsigned> disconnectReason{0}, scanStatus{0};
std::mutex diagnosticLock;
std::string networkDiagnostic;
std::string wifiCompatibility="WiFi permitido: ainda nao consultado";
void failure(const std::string &message) {
  {
    std::lock_guard<std::mutex> lock(diagnosticLock);
    networkDiagnostic = message;
  }
  if (systemRuntime)
    systemRuntime->storage().write(
        "/logs/network.txt",
        "runtime=1.0.0\nhosted=1.4.0\nwifi_remote=0.8.5\n" + message + "\n");
}
bool checked(esp_err_t result, const char *stage) {
  if (result == ESP_OK)
    return true;
  failure(std::string(stage) + ": " + esp_err_to_name(result) + " (" +
          std::to_string(result) + ")");
  return false;
}
std::atomic<const std::atomic<bool> *> cancellation{nullptr};
bool initialized = false, started = false, wifiReady = false;
std::atomic<bool> stopAcknowledged{false};
httpd_handle_t server = nullptr;
std::string token, transferPage, shareError;
void event(void *, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
    scanStatus =
        data ? static_cast<wifi_event_sta_scan_done_t *>(data)->status : 1;
    scanDone = true;
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
    associated = true;
    disconnectReason = 0;
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_STOP) stopAcknowledged=true;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    associated = false;
    if (data)
      disconnectReason =
          static_cast<wifi_event_sta_disconnected_t *>(data)->reason;
  }
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    linked = true;
  if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) linked = false;
  if (base == WIFI_EVENT &&
      (id == WIFI_EVENT_STA_DISCONNECTED || id == WIFI_EVENT_STA_STOP))
    linked = false;
}
bool hasAddress() {
  esp_netif_ip_info_t info{};
  return station && esp_netif_is_netif_up(station) && esp_netif_get_ip_info(station,&info)==ESP_OK && info.ip.addr!=0;
}
std::string address() {
  esp_netif_ip_info_t info{};
  char ip[48]{};
  if (station && esp_netif_get_ip_info(station, &info) == ESP_OK)
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
  return ip;
}
bool init() {
  if(transportFailed)return false;
  if(!initialized) {
    // Launcher installations need not provide a partition named "nvs".
    // Profiles belong to /SymbianOS; the P4/C6 driver uses RAM only.
    if(!checked(esp_netif_init(),"Inicializar rede"))return false;
    auto loop=esp_event_loop_create_default();
    if(loop!=ESP_OK&&loop!=ESP_ERR_INVALID_STATE)return false;
    if(!station)station=esp_netif_create_default_wifi_sta();
    if(!station)return false;
    if(!checked(bsp_feature_enable(BSP_FEATURE_WIFI,true),"Alimentar C6") || !checked(esp_hosted_init(),"Inicializar transporte C6"))return false;
    if(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,nullptr)!=ESP_OK)return false;
    if(esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,event,nullptr)!=ESP_OK){esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,event);return false;}
    initialized=true;
  }
  if(!wifiReady) {
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable=0;
    if(!checked(esp_wifi_init(&cfg),"Inicializar driver Wi-Fi C6"))return false;
    if(!checked(esp_wifi_set_storage(WIFI_STORAGE_RAM),"Configurar Wi-Fi em RAM")){
      if(esp_wifi_deinit()==ESP_OK)endStationEvents();
      return false;
    }
    wifiReady=true;
  }
  return true;
}
bool recoverDriver(const std::atomic<bool> &cancel,std::string &error) {
  if(cancel){error="Recuperação cancelada";return false;}
  if(!initialized)return true;
  const bool wasStarted=started;
  linked=false;associated=false;stopAcknowledged=false;
  const auto disconnected=esp_wifi_disconnect();
  if(cancel){error="Recuperação cancelada";return false;}
  const auto stopped=esp_wifi_stop();
  if(stopped==ESP_OK && wasStarted) {
    for(unsigned i=0;i<40&&!cancel&&!stopAcknowledged;++i)tab5::workerWait(std::chrono::milliseconds(50));
  }
  if(stopped==ESP_OK||stopped==ESP_ERR_WIFI_NOT_STARTED||stopped==ESP_ERR_WIFI_NOT_INIT)started=false;
  if(cancel){error="Recuperação cancelada";return false;}
  // Even when disconnect/stop reports stale state, deinit may still clean it.
  // Only successful deinit (or NOT_INIT) permits a new esp_wifi_init().
  const auto deinitialized=esp_wifi_deinit();
  if(systemRuntime)systemRuntime->storage().write("/logs/wifi-driver-recovery.txt",
    std::string("runtime=1.0.0\nDisconnect=")+esp_err_to_name(disconnected)+"\nStop="+esp_err_to_name(stopped)+"\nSTA_STOP="+(stopAcknowledged?"received":"absent")+"\nDeinit="+esp_err_to_name(deinitialized)+"\n");
  if(systemRuntime)systemRuntime->storage().write("/logs/wifi-sdio.txt",transportHistory());
  if(deinitialized!=ESP_OK && deinitialized!=ESP_ERR_WIFI_NOT_INIT){
    // Three generic failures plus no STOP event mean the remote control path
    // cannot be used for another driver cycle. SDIO deinit is TODO in 1.4.0.
    transportFailed = disconnected==ESP_FAIL && stopped==ESP_FAIL && deinitialized==ESP_FAIL && !stopAcknowledged;
    error=std::string("Desinicializar driver C6: ")+esp_err_to_name(deinitialized)+" ("+std::to_string(deinitialized)+")"+
      "\nDisconnect="+esp_err_to_name(disconnected)+"; Stop="+esp_err_to_name(stopped)+"; STA_STOP="+(stopAcknowledged?"recebido":"ausente");
    if(transportFailed)error+="\nTransporte P4/C6 sem resposta. Salve e reinicie o Tab5; repetir reconexão não recupera este estado.";
    failure(error);return false;
  }
  endStationEvents();
  started=false;wifiReady=false;
  // Keep netif, handlers and esp-hosted alive; init() recreates only Wi-Fi.
  return true;
}

bool authorized(httpd_req_t *req) {
  char key[80]{};
  if (token.empty() ||
      httpd_req_get_hdr_value_str(req, "X-Tab5-Token", key, sizeof(key)) !=
          ESP_OK ||
      token != key) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_sendstr(req, "Token necessário no header X-Tab5-Token");
    return false;
  }
  return true;
}
esp_err_t transfer(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  char query[300]{}, name[200]{};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "name", name, sizeof(name)) != ESP_OK)
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                               "Use ?name=arquivo.ext");
  std::string path = "/home/Transfer/" + std::string(name);
  if (!tab5::Network::sharedPath(path))
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Nome inválido");
  auto &net = systemRuntime->network();
  if(req->method==HTTP_DELETE) {
    char session[24]{};
    httpd_query_key_value(query,"session",session,sizeof(session));
    std::string error;
    bool ok=net.removeShared(path,session,error);
    httpd_resp_set_type(req,"text/plain; charset=utf-8");
    if(!ok)httpd_resp_set_status(req,"409 Conflict");
    return httpd_resp_sendstr(req,ok?"Concluído":error.c_str());
  }
  if (req->method == HTTP_PUT) {
    char session[24]{}, off[24]{}, total[24]{};
    if (httpd_query_key_value(query, "session", session, sizeof(session)) == ESP_OK) {
      auto number = [&](const char *key, char *buf, size_t &value) {
        if (httpd_query_key_value(query, key, buf, 24) != ESP_OK || !buf[0] ||
            strspn(buf, "0123456789") != strlen(buf)) return false;
        errno = 0; char *end = nullptr;
        auto n = strtoull(buf, &end, 10);
        if (errno || *end || n > tab5::Network::maxUploadBytes) return false;
        value = size_t(n); return true;
      };
      size_t offset = 0, length = 0;
      if (!number("offset", off, offset) || !number("total", total, length) ||
          req->content_len > 65536 || !req->content_len) {
        httpd_resp_set_status(req, "413 Content Too Large");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_sendstr(req, "Máximo 64 MiB por arquivo e 64 KiB por bloco.");
        return ESP_FAIL;
      }
      std::string why;
      bool ok = net.receivePart(session, path, offset, length, req->content_len,
          [req](char *p, size_t n) { return httpd_req_recv(req, p, std::min(n, size_t(4096))); }, why);
      httpd_resp_set_type(req, "text/plain; charset=utf-8");
      httpd_resp_set_hdr(req, "Connection", "close");
      if (!ok) httpd_resp_set_status(req, why.rfind("Bloco interrompido",0)==0 ? "408 Request Timeout" : "409 Conflict");
      httpd_resp_sendstr(req, ok ? "Bloco confirmado" : why.c_str());
      systemRuntime->storage().write("/logs/last-upload.txt",
          "runtime=1.0.0\nprotocol=blocks\noffset=" + std::to_string(offset) +
          "\ntotal=" + std::to_string(length) + "\nresult=" + (ok ? "ok" : why) + "\n");
      // Close acknowledged requests; idle keep-alive sockets cannot starve the server.
      return ESP_FAIL;
    }
    if (req->content_len > tab5::Network::maxUploadBytes ||
        req->content_len == 0) {
      httpd_resp_set_status(req, req->content_len == 0
                                     ? "400 Bad Request"
                                     : "413 Content Too Large");
      httpd_resp_set_type(req, "text/plain; charset=utf-8");
      httpd_resp_set_hdr(req, "Connection", "close");
      httpd_resp_sendstr(
          req, req->content_len == 0
                   ? "Arquivo vazio: selecione um arquivo com conteúdo."
                   : "Arquivo maior que o limite de 64 MiB (67.108.864 bytes). "
                     "O envio foi recusado.");
      // Close this request without purging a potentially huge unread body.
      return ESP_FAIL;
    }
    int socketResult = 0;
    bool ok = net.receive(
        path,
        [req, &socketResult](char *p, size_t n) {
          // A short socket timeout is recoverable; a closed socket/cancel is
          // not.
          for (unsigned attempt = 0; attempt < 5; ++attempt) {
            int result = httpd_req_recv(req, p, std::min(n, size_t(4096)));
            socketResult = result;
            if (result != HTTPD_SOCK_ERR_TIMEOUT)
              return result;
          }
          return HTTPD_SOCK_ERR_TIMEOUT;
        },
        req->content_len,
        [req, &socketResult](const char *stage, size_t bytes) {
          systemRuntime->storage().write(
              "/logs/last-upload.txt",
              std::string("runtime=1.0.0\nstage=") + stage +
                  "\nreceived=" + std::to_string(bytes) +
                  "\nexpected=" + std::to_string(req->content_len) +
                  "\nlast_socket_result=" + std::to_string(socketResult) +
                  "\nhttp_stack_free_bytes=" +
                  std::to_string(uxTaskGetStackHighWaterMark(nullptr)) +
                  "\ninternal_free_bytes=" +
                  std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) +
                  "\n");
        });
    if (!ok)
      return httpd_resp_send_err(
          req, HTTPD_400_BAD_REQUEST,
          "Upload recusado/incompleto; confira nome e espaço");
    return httpd_resp_sendstr(req, "Salvo no SD");
  }
  httpd_resp_set_type(req, "application/octet-stream");
  if (!net.send(path, [req](const char *p, size_t n) {
        return httpd_resp_send_chunk(req, p, n) == ESP_OK;
      }))
    return ESP_FAIL;
  return httpd_resp_send_chunk(req, nullptr, 0);
}
esp_err_t listing(httpd_req_t *req) {
  if (!authorized(req))
    return ESP_OK;
  std::vector<tab5::File> files;
  if (!systemRuntime->storage().listDir("/home/Transfer", files))
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "SD indisponível");
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  for (auto &f : files)
    if (!f.directory && tab5::Network::sharedPath(f.path)) {
      auto line = f.path.substr(15) + "\t" + std::to_string(f.size) + "\n";
      if (httpd_resp_send_chunk(req, line.data(), line.size()) != ESP_OK)
        return ESP_FAIL;
    }
  return httpd_resp_send_chunk(req, nullptr, 0);
}
bool share(bool on, std::string &ip, std::string &key) {
  shareError.clear();
  if (!on) {
    if (server)
      httpd_stop(server);
    server = nullptr;
    token.clear();
    return true;
  }
  if (!linked) {shareError="Wi-Fi sem endereço ativo para o envio web";return false;}
  if (!server) {
    std::array<uint8_t, 8> random{};
    esp_fill_random(random.data(), random.size());
    token = tab5::Network::transferToken(random);
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 8080;
    cfg.ctrl_port = 32769;
    // Keep the proven handler stack size without requiring a contiguous
    // 32 KiB block of scarce internal RAM. WithCaps is used by IDF HTTPD.
    cfg.stack_size = 32768;
    cfg.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    cfg.max_open_sockets = 4;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 15;
    cfg.send_wait_timeout = 15;
    httpd_handle_t candidate=nullptr;
    errno=0;auto result=httpd_start(&candidate, &cfg);
    if (result != ESP_OK) {
      shareError=std::string("Iniciar envio web na porta 8080: ")+esp_err_to_name(result)+
        " • errno="+std::to_string(errno)+
        " • heap interno="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL))+
        " • bloco interno="+std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))+
        " • PSRAM="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))+
        " • bloco PSRAM="+std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
      token.clear();
      return false;
    }
    httpd_uri_t page{};
    page.uri = "/";
    page.method = HTTP_GET;
    page.handler = [](httpd_req_t *req) {
      httpd_resp_set_type(req, "text/html; charset=utf-8");
      httpd_resp_set_hdr(req, "Cache-Control", "no-store");
      return httpd_resp_send(req, transferPage.data(), transferPage.size());
    };
    if ((result=httpd_register_uri_handler(candidate, &page)) != ESP_OK) {
      shareError=std::string("Registrar página de envio web: ")+esp_err_to_name(result);
      httpd_stop(candidate);
      server = nullptr;
      token.clear();
      return false;
    }
    httpd_uri_t list{};
    list.uri = "/files";
    list.method = HTTP_GET;
    list.handler = listing;
    httpd_uri_t get{};
    get.uri = "/file";
    get.method = HTTP_GET;
    get.handler = transfer;
    httpd_uri_t put = get;
    put.method = HTTP_PUT;
    httpd_uri_t del=get;del.method=HTTP_DELETE;
    if ((result=httpd_register_uri_handler(candidate, &list)) != ESP_OK ||
        (result=httpd_register_uri_handler(candidate, &get)) != ESP_OK ||
        (result=httpd_register_uri_handler(candidate, &put)) != ESP_OK ||
        (result=httpd_register_uri_handler(candidate, &del)) != ESP_OK) {
      shareError=std::string("Registrar transferência web: ")+esp_err_to_name(result);
      httpd_stop(candidate);
      server = nullptr;
      token.clear();
      return false;
    }
    server=candidate;
  }
  ip = address()+":8080";
  key = token;
  return true;
}
} // namespace
void tab5_setup_network(tab5::Runtime &runtime) {
  systemRuntime = &runtime;
  // Install once, before either Wi-Fi or BLE can start ESP-Hosted. The pinned
  // OS abstraction table is writable; preserve the upstream implementation.
  if (!originalWifiPost) {
    originalWifiPost = g_h.funcs->_h_event_wifi_post;
    g_h.funcs->_h_event_wifi_post = filteredWifiEventPost;
  }
  previousLog=esp_log_set_vprintf(transportLog);
  auto &b = runtime.network().backend;
  b.restartRequired = [] {return transportFailed.load();};
  b.linked = [] { return !transportFailed && linked.load() && hasAddress(); };
  b.signal=[] {wifi_ap_record_t ap{};return !transportFailed && linked && esp_wifi_sta_get_ap_info(&ap)==ESP_OK?int(ap.rssi):-127;};
  b.diagnostic = [] {
    std::lock_guard<std::mutex> lock(diagnosticLock);
    return networkDiagnostic.empty()
               ? std::string("Falha de Wi-Fi; consulte logs/network.txt")
               : networkDiagnostic;
  };
  b.scan = [](std::vector<tab5::Network::AccessPoint> &found,
              const std::atomic<bool> &cancel) {
    cancellation = &cancel;
    if (cancel || !init())
      return false;
    if (!checked(esp_wifi_set_mode(WIFI_MODE_STA), "Modo de busca STA"))
      return false;
    if (!started) {
      if (!checked(esp_wifi_start(), "Iniciar rádio para busca"))
        return false;
      started = true;
    }
    wifi_scan_config_t scan{};
    scan.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan.scan_time.active.min = 40;
    scan.scan_time.active.max = 120;
    scanDone = false;
    scanStatus = 0;
    if (!checked(esp_wifi_scan_start(&scan, false), "Buscar SSIDs"))
      return false;
    for (unsigned i = 0; i < 200 && !cancel && !scanDone; ++i)
      tab5::workerWait(std::chrono::milliseconds(50));
    if (cancel || !scanDone) {
      esp_wifi_scan_stop();
      failure(cancel ? "Busca cancelada" : "Busca de SSIDs excedeu o tempo");
      return false;
    }
    if (scanStatus != 0) {
      failure("Busca recusada pelo C6 • status=" +
              std::to_string(scanStatus.load()));
      return false;
    }
    uint16_t count = 32;
    wifi_ap_record_t records[32]{};
    if (!checked(esp_wifi_scan_get_ap_records(&count, records), "Ler SSIDs"))
      return false;
    for (unsigned i = 0; i < count; ++i) {
      auto &ap = records[i];
      std::string security;
      switch (ap.authmode) {
      case WIFI_AUTH_OPEN:
        security = "Aberta";
        break;
      case WIFI_AUTH_WPA2_PSK:
        security = "WPA2";
        break;
      case WIFI_AUTH_WPA3_PSK:
        security = "WPA3";
        break;
      case WIFI_AUTH_WPA2_WPA3_PSK:
        security = "WPA2/WPA3";
        break;
      default:
        security = "Protegida (" + std::to_string(ap.authmode) + ")";
        break;
      }
      found.push_back(
          {std::string(reinterpret_cast<const char *>(ap.ssid),
                       strnlen(reinterpret_cast<const char *>(ap.ssid), 32)),
           security, ap.rssi, ap.primary});
    }
    return true;
  };
  b.connect = [](const std::string &ssid, const std::string &password,
                 std::string &ip, const std::atomic<bool> &cancel) {
    cancellation = &cancel;
    if (cancel || !init()) {
      return false;
    }
    esp_hosted_coprocessor_fwver_t firmware{};
    if (esp_hosted_get_coprocessor_fwversion(&firmware) == ESP_OK)
      systemRuntime->storage().write(
          "/logs/network.txt",
          "runtime=1.0.0\nhosted=1.4.0\nwifi_remote=0.8.5\nC6=" +
              std::to_string(firmware.major1) + "." +
              std::to_string(firmware.minor1) + "." +
              std::to_string(firmware.patch1) + "\n");
    if (started) {
      esp_wifi_disconnect();
      esp_wifi_stop();
      started = false;
    }
    linked = false;
    associated = false;
    disconnectReason = 0;
    wifi_config_t cfg{};
    memcpy(cfg.sta.ssid, ssid.data(), ssid.size());
    memcpy(cfg.sta.password, password.data(), password.size());
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;
    cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    cfg.sta.threshold.authmode =
        password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    if (!checked(esp_wifi_set_mode(WIFI_MODE_STA), "Modo STA") ||
        !checked(esp_wifi_set_config(WIFI_IF_STA, &cfg), "Aplicar WPA2/WPA3"))
      return false;
    if (!started) {
      if (!checked(esp_wifi_start(), "Iniciar estação"))
        return false;
      started = true;
    }
    // Restore normal capabilities after the legacy-only mitigation did not solve RX.
    uint8_t previousProtocol = 0;
    auto previousResult = esp_wifi_get_protocol(WIFI_IF_STA, &previousProtocol);
    const uint8_t requested = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX;
    if (!checked(esp_wifi_set_protocol(WIFI_IF_STA, requested), "Aplicar protocolos Wi-Fi normais")) return false;
    uint8_t applied = 0;
    if (!checked(esp_wifi_get_protocol(WIFI_IF_STA, &applied), "Conferir protocolo Wi-Fi no C6")) return false;
    {
      std::lock_guard<std::mutex> guard(diagnosticLock);
      wifiCompatibility = "WiFi anterior=" + (previousResult == ESP_OK ? std::to_string(previousProtocol) : "indisponivel") +
          "; solicitado=b/g/n/ax (71); C6 confirmou=" + std::to_string(applied) +
          "\nC6=" + std::to_string(firmware.major1) + "." + std::to_string(firmware.minor1) + "." + std::to_string(firmware.patch1);
    }
    if (applied != requested) {
      failure("C6 nao confirmou protocolos Wi-Fi solicitados; bitmap=" + std::to_string(applied)); return false;
    }
    systemRuntime->storage().write("/logs/wifi-transport.txt", "runtime=1.0.0\nstage=before-connect\n" + tab5_wifi_transport_info() + "\n");
    linked = false;
    if (!checked(esp_wifi_connect(), "Solicitar conexão"))
      return false;
    for (unsigned i = 0; i < 250 && !cancel && (!linked || !hasAddress()); ++i)
      tab5::workerWait(std::chrono::milliseconds(100));
    if (cancel || !linked || !hasAddress()) {
      unsigned reason = disconnectReason.load();
      std::string detail = cancel       ? "Conexão cancelada"
                           : associated ? "Associado, mas não recebeu IP (DHCP)"
                           : reason == 201
                               ? "SSID não encontrado na banda de 2,4 GHz"
                           : (reason == 202 || reason == 15 || reason == 204)
                               ? "Falha de autenticação WPA2/WPA3 ou handshake"
                               : "Falha de associação Wi-Fi";
      failure(detail + " • motivo=" + std::to_string(reason));
      esp_wifi_disconnect();
      return false;
    }
    ip = address();
    {std::lock_guard<std::mutex> guard(diagnosticLock);networkDiagnostic.clear();}
    systemRuntime->storage().write("/logs/wifi-transport.txt", "runtime=1.0.0\nstage=connected\n" + tab5_wifi_transport_info() + "\n");
    return true;
  };
  b.recover = recoverDriver;
  b.disconnect = [] {
    if (initialized && !transportFailed) {
      esp_wifi_disconnect();
      if (started)
        esp_wifi_stop();
      started = false;
    }
    linked = false;
  };
  b.share = share;
  b.shareDiagnostic=[] {return shareError;};
  auto fetchDocument = [](const std::string &url, const tab5::Network::Sink &sink,
               const std::atomic<bool> &cancel, std::string &error, std::string &resolved,bool mobile,const tab5::Network::BrowserRequest *request=nullptr) {
    if (!linked) {
      error = "Sem conexão Wi-Fi";
      return false;
    }
    class Stream final : public tab5::HttpStream {
      bool mobile_;
      const tab5::Network::BrowserRequest *request_;
      const std::atomic<bool> *cancelled_;
      std::string currentUrl_,initialUrl_;bool post_=false;int lastStatus_=0;
      std::vector<std::string> setCookies_;
      esp_http_client_handle_t client_ = nullptr;
      std::string location_;
      bool invalidLocation_ = false;
      bool encoded_ = false, invalidLength_ = false;
      tab5::HttpBody body_;
      std::string readError_;
      static uint64_t now() {return esp_timer_get_time()/1000;}

    public:
      explicit Stream(bool mobile,const tab5::Network::BrowserRequest *request,const std::atomic<bool> *cancelled):mobile_(mobile),request_(request),cancelled_(cancelled),post_(request&&request->method=="POST"){}
      ~Stream() { close(); }
      bool open(const std::string &url, int &status, std::string &location,
                std::string &error) override {
        close();
        if(initialUrl_.empty())initialUrl_=url;
        else if(post_ && lastStatus_){
          if(lastStatus_==301||lastStatus_==302||lastStatus_==303)post_=false;
          else if(url!=initialUrl_){error="Redirecionamento POST 307/308 recusado para evitar reenvio de dados";return false;}
        }
        currentUrl_=url;setCookies_.clear();
        location_.clear();location_.reserve(1024);
        invalidLocation_ = false;encoded_=false;invalidLength_=false;
        body_={};readError_.clear();
        esp_http_client_config_t config{};
        config.url = url.c_str();
        config.timeout_ms = 20000;
        config.addr_type = HTTP_ADDR_TYPE_INET;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        if(request_&&request_->legacyTls)config.tls_version=ESP_HTTP_CLIENT_TLS_VER_TLS_1_2;
        config.disable_auto_redirect = true;
        config.buffer_size = 4096;
        config.buffer_size_tx = 1024;
        config.user_agent = mobile_?tab5::browserProfile::userAgent:tab5::browserProfile::genericAgent;
        config.user_data = this;
        config.event_handler = [](esp_http_client_event_t *event) {
          auto *self = static_cast<Stream *>(event->user_data);
          if(event->event_id==HTTP_EVENT_ON_HEADER && event->header_key && event->header_value) {
            if(self->request_ && strcasecmp(event->header_key,"Set-Cookie")==0 && self->setCookies_.size()<64 && strnlen(event->header_value,2049)<=2048)self->setCookies_.emplace_back(event->header_value);
            if(strcasecmp(event->header_key,"Content-Encoding")==0 && strcasecmp(event->header_value,"identity")!=0)self->encoded_=true;
            if(strcasecmp(event->header_key,"Content-Length")==0) {
              char *end=nullptr;errno=0;auto value=strtoull(event->header_value,&end,10);
              if(errno || end==event->header_value || !end || *end || value>INT64_MAX || strspn(event->header_value,"0123456789")!=strlen(event->header_value)) self->invalidLength_=true;
              else self->body_.length=int64_t(value);
            }
            if(strcasecmp(event->header_key,"Transfer-Encoding")==0 && strcasestr(event->header_value,"chunked")) self->body_.chunked=true;
          }
          if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key &&
              event->header_value &&
              strcasecmp(event->header_key, "Location") == 0) {
            size_t n = strnlen(event->header_value, 1025);
            if (n > 1024)
              self->invalidLocation_ = true;
            else
              self->location_.assign(event->header_value, n);
          }
          return ESP_OK;
        };
        client_ = esp_http_client_init(&config);
        if (!client_) {
          error = "Sem memória para HTTP";
          return false;
        }
        esp_http_client_set_header(client_,"Accept","text/html, application/xhtml+xml, application/rss+xml, application/atom+xml, text/css, */*");
        if(mobile_){
          esp_http_client_set_header(client_,"Sec-CH-UA-Mobile","?1");
          esp_http_client_set_header(client_,"Save-Data","on");
          esp_http_client_set_header(client_,"Viewport-Width",std::to_string(tab5::browserProfile::width).c_str());
        }
        esp_http_client_set_header(client_,"Accept-Encoding","identity");
        esp_http_client_set_header(client_,"Connection","close");
        if(request_){auto cookies=request_->cookies?request_->cookies(url,!post_):std::string();if(!cookies.empty())esp_http_client_set_header(client_,"Cookie",cookies.c_str());}
        if(post_){esp_http_client_set_method(client_,HTTP_METHOD_POST);esp_http_client_set_header(client_,"Content-Type","application/x-www-form-urlencoded");}
        auto result = esp_http_client_open(client_, post_?int(request_->body.size()):0);
        if (result != ESP_OK) {
          int tls = 0, flags = 0;
          esp_http_client_get_and_clear_last_tls_error(client_, &tls, &flags);
          error = std::string("Falha de conexão HTTP/TLS: ") +
                  esp_err_to_name(result) + " • TLS=" + std::to_string(tls) +
                  " • certificado=" + std::to_string(flags) +
                  " • errno="+std::to_string(esp_http_client_get_errno(client_))+
                  " • heap_livre="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL))+
                  " • maior_bloco=" + std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))+
                  " • PSRAM="+std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))+
                  " • stack_livre="+std::to_string(uxTaskGetStackHighWaterMark(nullptr));
          failure(error);
          return false;
        }
        if(post_){size_t at=0;while(at<request_->body.size()){if(cancelled_&&*cancelled_){error="Cancelado";return false;}int written=esp_http_client_write(client_,request_->body.data()+at,request_->body.size()-at);if(written<=0){error="Envio POST interrompido; não foi repetido";return false;}at+=size_t(written);}}
        auto headerResult=esp_http_client_fetch_headers(client_);
        if (headerResult < 0) {
          int tls=0,flags=0;esp_http_client_get_and_clear_last_tls_error(client_,&tls,&flags);
          error = "Falha ao receber cabeçalhos HTTP: resultado="+std::to_string(headerResult)+
              " errno="+std::to_string(esp_http_client_get_errno(client_))+" TLS="+std::to_string(tls)+" flags="+std::to_string(flags);
          failure(error);
          return false;
        }
        if(invalidLength_){error="Content-Length HTTP inválido";return false;}
        if (invalidLocation_) {
          error = "Location excede 1024 bytes";
          return false;
        }
        if(encoded_){error="Servidor enviou conteúdo comprimido apesar de Accept-Encoding: identity";return false;}
        body_.chunked=esp_http_client_is_chunked_response(client_);
        if(body_.chunked) body_.length=-1;
        body_.lastProgress=now();
        esp_http_client_set_timeout_ms(client_,3000);
        status = esp_http_client_get_status_code(client_);lastStatus_=status;
        if(request_ && request_->setCookie)for(const auto &cookie:setCookies_)request_->setCookie(currentUrl_,cookie);
        location = location_;
        return true;
      }
      int read(char *data, size_t n) override {
        int got = esp_http_client_read(client_, data, std::min(n,size_t(1024)));
        int code=esp_http_client_get_errno(client_);
        bool transient=got==-ESP_ERR_HTTP_EAGAIN || code==EAGAIN || code==EWOULDBLOCK || code==ETIMEDOUT;
        int result=body_.accept(got,esp_http_client_is_complete_data_received(client_),transient,got==0 && !transient,now());
        if(result==-1) {
          int tls=0,flags=0;esp_http_client_get_and_clear_last_tls_error(client_,&tls,&flags);
          readError_=std::string("tls_version=")+(request_&&request_->legacyTls?"1.2 ":"auto ")+"bytes="+std::to_string(body_.bytes)+" esperado="+std::to_string(body_.length)+" chunked="+std::to_string(body_.chunked)+" retorno="+std::to_string(got)+" errno="+std::to_string(code)+" TLS="+std::to_string(tls);
        }
        return result;
      }
      std::string diagnostic() const override {return readError_;}
      void close() override {
        if (client_) {
          esp_http_client_close(client_);
          esp_http_client_cleanup(client_);
          client_ = nullptr;
        }
      }
    } stream(mobile,request,&cancel);
    bool delivered=false;
    auto guarded=[&](const char *data,size_t size) {
      delivered=true; return sink(data,size);
    };
    for(unsigned attempt=0;attempt<2;++attempt) {
      if(tab5::fetchHttp(stream,url,guarded,cancel,error,&resolved,mobile?180000u:90000u)) return true;
      if((request&&request->method=="POST") || delivered || cancel || !linked || (error.find("conexão HTTP/TLS")==std::string::npos && error.find("cabeçalhos HTTP")==std::string::npos)) break;
      tab5::workerWait(std::chrono::milliseconds(300));
    }
    return false;
  };
  b.browserFetch=[fetchDocument](const auto &url,const auto &request,const auto &sink,const auto &cancel,std::string &error,std::string &resolved){return fetchDocument(url,sink,cancel,error,resolved,true,&request);};
  b.fetchDocument=[fetchDocument](const auto &url,const auto &sink,const auto &cancel,std::string &error,std::string &resolved){return fetchDocument(url,sink,cancel,error,resolved,true);};
  b.fetchBrowserAsset=[fetchDocument](const auto &url,const auto &sink,const auto &cancel,std::string &error){std::string resolved;return fetchDocument(url,sink,cancel,error,resolved,true);};
  b.fetch=[fetchDocument](const auto &url,const auto &sink,const auto &cancel,std::string &error){std::string resolved;return fetchDocument(url,sink,cancel,error,resolved,false);};
  runtime.setOfflineWriter([&runtime](bool off) {
    if (off)
      runtime.network().disconnect();
    return true;
  });
}

void tab5_load_transfer_page(tab5::Runtime &runtime) {
  runtime.asset("transfer.html", transferPage, 32768);
}

extern "C" int tab5_network_cancelled(void) {
  auto *flag = cancellation.load();
  return flag && flag->load();
}

std::string tab5_wifi_transport_info() {
  std::string out;
  {std::lock_guard<std::mutex> guard(diagnosticLock);out=wifiCompatibility;}
  out+="\nSDIO RX=streaming (compatível com C6 1.4.1)";
  {
    std::lock_guard<std::mutex> guard(wifiEventMutex);
    out+="\nSTA eventos repetidos: START="+std::to_string(staEvents.duplicateStarts())+
         " STOP="+std::to_string(staEvents.duplicateStops());
  }
  auto text=[](const esp_ip4_addr_t &ip){char value[20]{};snprintf(value,sizeof(value),IPSTR,IP2STR(&ip));return std::string(value);};
  esp_netif_ip_info_t info{};
  if(station && esp_netif_get_ip_info(station,&info)==ESP_OK)out+="\nIP="+text(info.ip)+" gateway="+text(info.gw);
  for(unsigned i=0;i<3 && station;++i){
    esp_netif_dns_info_t dns{};
    auto rc=esp_netif_get_dns_info(station,static_cast<esp_netif_dns_type_t>(i),&dns);
    out+="\nDNS"+std::to_string(i)+"="+(rc!=ESP_OK?std::string("indisponivel"):dns.ip.type==ESP_IPADDR_TYPE_V4?text(dns.ip.u_addr.ip4):std::string("IPv6"));
  }
  return out;
}
