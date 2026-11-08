// SPDX-License-Identifier: MIT
#include "bsp/m5stack_tab5.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_hosted_api.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "runtime.hpp"
#include <atomic>
#include <cstring>
namespace {
tab5::Runtime *systemRuntime = nullptr;
esp_netif_t *station = nullptr;
std::atomic<bool> linked{false}, associated{false}, scanDone{false};
std::atomic<unsigned> disconnectReason{0}, scanStatus{0};
std::mutex diagnosticLock;
std::string networkDiagnostic;
void failure(const std::string &message) {
  {
    std::lock_guard<std::mutex> lock(diagnosticLock);
    networkDiagnostic = message;
  }
  if (systemRuntime)
    systemRuntime->storage().write(
        "/logs/network.txt",
        "runtime=0.8.0\nhosted=1.4.0\nwifi_remote=0.8.5\n" + message + "\n");
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
httpd_handle_t server = nullptr;
std::string token, transferPage;
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
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    associated = false;
    if (data)
      disconnectReason =
          static_cast<wifi_event_sta_disconnected_t *>(data)->reason;
  }
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    linked = true;
  if (base == WIFI_EVENT &&
      (id == WIFI_EVENT_STA_DISCONNECTED || id == WIFI_EVENT_STA_STOP))
    linked = false;
}
std::string address() {
  esp_netif_ip_info_t info{};
  char ip[48]{};
  if (station && esp_netif_get_ip_info(station, &info) == ESP_OK)
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
  return ip;
}
bool init() {
  if (initialized)
    return true;
  // A radio failure is optional: never erase NVS or abort the OS on an error.
  if (!checked(nvs_flash_init(), "Inicializar NVS"))
    return false;
  if (!checked(esp_netif_init(), "Inicializar rede"))
    return false;
  auto rc = esp_event_loop_create_default();
  if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE)
    return false;
  if (!station)
    station = esp_netif_create_default_wifi_sta();
  if (!station)
    return false;
  if (!checked(bsp_feature_enable(BSP_FEATURE_WIFI, true), "Alimentar C6") ||
      !checked(esp_hosted_init(), "Inicializar transporte C6"))
    return false;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (!wifiReady) {
    if (!checked(esp_wifi_init(&cfg), "Inicializar Wi-Fi no C6"))
      return false;
    wifiReady = true;
  }
  if (!checked(esp_wifi_set_storage(WIFI_STORAGE_RAM),
               "Configurar Wi-Fi em RAM"))
    return false;
  if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event,
                                 nullptr) != ESP_OK ||
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event,
                                 nullptr) != ESP_OK)
    return false;
  initialized = true;
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
  if (req->method == HTTP_PUT) {
    int socketResult = 0;
    bool ok = net.receive(
        path,
        [req, &socketResult](char *p, size_t n) {
          // A short socket timeout is recoverable; a closed socket/cancel is
          // not.
          for (unsigned attempt = 0; attempt < 5; ++attempt) {
            int result = httpd_req_recv(req, p, n);
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
              std::string("runtime=0.8.0\nstage=") + stage +
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
  if (!on) {
    if (server)
      httpd_stop(server);
    server = nullptr;
    token.clear();
    return true;
  }
  if (!linked)
    return false;
  if (!server) {
    std::array<uint8_t, 8> random{};
    esp_fill_random(random.data(), random.size());
    token = tab5::Network::transferToken(random);
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 32768;
    cfg.max_open_sockets = 2;
    cfg.recv_wait_timeout = 2;
    cfg.send_wait_timeout = 2;
    if (httpd_start(&server, &cfg) != ESP_OK) {
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
    if (httpd_register_uri_handler(server, &page) != ESP_OK) {
      httpd_stop(server);
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
    if (httpd_register_uri_handler(server, &list) != ESP_OK ||
        httpd_register_uri_handler(server, &get) != ESP_OK ||
        httpd_register_uri_handler(server, &put) != ESP_OK) {
      httpd_stop(server);
      server = nullptr;
      token.clear();
      return false;
    }
  }
  ip = address();
  key = token;
  return true;
}
} // namespace
void tab5_setup_network(tab5::Runtime &runtime) {
  systemRuntime = &runtime;
  auto &b = runtime.network().backend;
  b.linked = [] { return linked.load(); };
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
          "runtime=0.8.0\nhosted=1.4.0\nwifi_remote=0.8.5\nC6=" +
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
    linked = false;
    if (!checked(esp_wifi_connect(), "Solicitar conexão"))
      return false;
    for (unsigned i = 0; i < 250 && !cancel && !linked; ++i)
      tab5::workerWait(std::chrono::milliseconds(100));
    if (cancel || !linked) {
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
    return true;
  };
  b.disconnect = [] {
    if (initialized) {
      esp_wifi_disconnect();
      if (started)
        esp_wifi_stop();
      started = false;
    }
    linked = false;
  };
  b.share = share;
  b.fetch = [](const std::string &url, const tab5::Network::Sink &sink,
               const std::atomic<bool> &cancel, std::string &error) {
    if (!linked) {
      error = "Sem conexão Wi-Fi";
      return false;
    }
    esp_http_client_config_t cfg{};
    cfg.url = url.c_str();
    cfg.timeout_ms = 2000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.disable_auto_redirect = true;
    cfg.buffer_size = 4096;
    auto h = esp_http_client_init(&cfg);
    if (!h) {
      error = "Sem memória para HTTP";
      return false;
    }
    bool ok = esp_http_client_open(h, 0) == ESP_OK;
    if (ok) {
      esp_http_client_fetch_headers(h);
      ok = esp_http_client_get_status_code(h) == 200;
    }
    char buffer[4096];
    while (ok && !cancel) {
      int n = esp_http_client_read(h, buffer, sizeof(buffer));
      if (n < 0) {
        ok = false;
        break;
      }
      if (n == 0) {
        ok = esp_http_client_is_complete_data_received(h);
        break;
      }
      ok = sink(buffer, n);
    }
    ok = ok && !cancel;
    if (!ok)
      error = "HTTP/TLS interrompido; redirects exigem URL final";
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    return ok;
  };
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
