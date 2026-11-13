// SPDX-License-Identifier: MIT
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "mail_tls.hpp"
#include "runtime.hpp"
#include <cstdlib>
#include <sys/time.h>
std::string tab5_wifi_transport_info();
void tab5_setup_mail(tab5::Runtime &runtime) {
  runtime.mail().recoverTransport = [&runtime] {
    std::string profile;
    runtime.storage().read("/config/public/profile", profile);
    return profile != "offline" && runtime.network().recover(true);
  };
  runtime.mail().wire = [&runtime] {
    return std::make_unique<tab5::MailTlsSocket>(
        [](mbedtls_ssl_config *conf) { return esp_crt_bundle_attach(conf); },
        [&runtime](std::string &error) {
          const int64_t epoch = runtime.now();
          if (epoch < 1700000000) {
            error = "Ajuste data e hora antes de conectar e-mail";
            return false;
          }
          timeval date{static_cast<time_t>(epoch), 0};
          if (settimeofday(&date, nullptr) != 0 ||
              std::llabs(int64_t(time(nullptr)) - epoch) > 2) {
            error = "Falha ao aplicar relógio POSIX";
            return false;
          }
          return true;
        },
        [] {
          return tab5_wifi_transport_info() + "\nHeap livre=" +
                 std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) +
                 " maior=" +
                 std::to_string(
                     heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)) +
                 " PSRAM=" +
                 std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) +
                 "\nRelogio Unix=" + std::to_string(time(nullptr));
        });
  };
}
