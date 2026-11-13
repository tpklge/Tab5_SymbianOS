// SPDX-License-Identifier: MIT
#pragma once
#include "executor.hpp"
#include "mail_protocol.hpp"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <memory>
#include <mutex>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
namespace tab5 {
// Shared by the ESP firmware and real TLS host tests. No ESP-TLS state machine.
class MailTlsSocket final : public MailWire {
  int fd_ = -1, lastTls_ = 0, lastErrno_ = 0;
  bool secure_ = false, connectionTimeout_ = false, dnsFailure_ = false;
  size_t tx_ = 0, rx_ = 0;
  std::string stage_, host_;
  unsigned port_ = 0;
  mbedtls_ssl_context ssl_{};
  mbedtls_ssl_config conf_{};
  mbedtls_ctr_drbg_context random_{};
  mbedtls_entropy_context entropy_{};
  std::function<int(const char *,const char *,const addrinfo *,addrinfo **)> dns_;
  std::function<int(mbedtls_ssl_config *)> trust_;
  std::function<bool(std::string &)> before_;
  std::function<std::string()> details_;
  using Clock = std::chrono::steady_clock;
  Clock::time_point deadline_;
  std::chrono::milliseconds budget_;
  void phase(const std::string &s) {
    stage_ = s;
    if (progress)
      progress(report());
  }
  std::string report() const {
    return host_ + ":" + std::to_string(port_) +
           "\nTransporte=mbedTLS direto / send-recv\nPerfil=TLS1.2 ECDHE "
           "AES-GCM; MFL solicitado=1024\nEtapa=" +
           stage_ + "\nTLS=" + std::to_string(lastTls_) + " cert=" +
           std::to_string(
               (secure_ || lastTls_ == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
                   ? mbedtls_ssl_get_verify_result(&ssl_)
                   : 0) +
           " errno=" + std::to_string(lastErrno_) +
           "\nTLS TX=" + std::to_string(tx_) + " RX=" + std::to_string(rx_);
  }
  bool alive(std::string &error) {
    if (cancelled()) {
      error = "Operação cancelada";
      return false;
    }
    if (Clock::now() >= deadline_) {
      connectionTimeout_ = true;
      error = "Prazo de conexão esgotado\n" + report();
      if (details_)
        error += "\n" + details_();
      return false;
    }
    return true;
  }
  bool fail(const std::string &why, std::string &error) {
    error = why + "\n" + report();
    if (details_)
      error += "\n" + details_();
    return false;
  }
  static int sendBio(void *ctx, const unsigned char *data, size_t count) {
    auto &self = *static_cast<MailTlsSocket *>(ctx);
    auto n = ::send(self.fd_, data, std::min(count, size_t(1024)), 0);
    if (n > 0) {
      self.tx_ += size_t(n);
      return int(n);
    }
    self.lastErrno_ = errno;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_NET_SEND_FAILED;
  }
  static int recvBio(void *ctx, unsigned char *data, size_t count) {
    auto &self = *static_cast<MailTlsSocket *>(ctx);
    auto n = ::recv(self.fd_, data, std::min(count, size_t(1024)), 0);
    if (n >= 0) {
      self.rx_ += size_t(n);
      return int(n);
    }
    self.lastErrno_ = errno;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return MBEDTLS_ERR_SSL_WANT_READ;
    return MBEDTLS_ERR_NET_RECV_FAILED;
  }
  bool tls(const std::string &host, std::string &error) {
    deadline_ = Clock::now() + budget_;
    phase("Preparando TLS; TCP conectado");
    static const unsigned char label[] = "Tab5Symbian mail";
    int rc = mbedtls_ctr_drbg_seed(&random_, mbedtls_entropy_func, &entropy_,
                                   label, sizeof(label) - 1);
    if (!rc)
      rc = mbedtls_ssl_config_defaults(&conf_, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (!rc) {
      mbedtls_ssl_conf_authmode(&conf_, MBEDTLS_SSL_VERIFY_REQUIRED);
      mbedtls_ssl_conf_rng(&conf_, mbedtls_ctr_drbg_random, &random_);
      mbedtls_ssl_conf_min_tls_version(&conf_, MBEDTLS_SSL_VERSION_TLS1_2);
      mbedtls_ssl_conf_max_tls_version(&conf_, MBEDTLS_SSL_VERSION_TLS1_2);
      static const int suites[] = {
          MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
          MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
          MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
          MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, 0};
      static const uint16_t groups[] = {MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1,
                                        MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1,
                                        0};
      mbedtls_ssl_conf_ciphersuites(&conf_, suites);
      mbedtls_ssl_conf_groups(&conf_, groups);
      static const uint16_t signatures[] = {0x0401, 0x0501, 0x0403, 0x0503,
                                            0}; // RSA/ECDSA SHA-256/384
      mbedtls_ssl_conf_sig_algs(&conf_, signatures);
      rc = mbedtls_ssl_conf_max_frag_len(&conf_, MBEDTLS_SSL_MAX_FRAG_LEN_1024);
      if (!rc)
        rc = trust_(&conf_);
    }
    if (!rc)
      rc = mbedtls_ssl_setup(&ssl_, &conf_);
    if (!rc)
      rc = mbedtls_ssl_set_hostname(&ssl_, host.c_str());
    if (rc) {
      lastTls_ = rc;
      return fail("Falha ao configurar TLS", error);
    }
    mbedtls_ssl_set_bio(&ssl_, this, sendBio, recvBio, nullptr);
    phase("Negociando TLS; TCP conectado");
    size_t previousTx = 0, previousRx = 0;
    while (alive(error)) {
      rc = mbedtls_ssl_handshake(&ssl_);
      if (rc == 0) {
        secure_ = true;
        lastTls_ = 0;
        lastErrno_ = 0;
        phase(std::string("TLS estabelecido; certificado verificado; ") +
              mbedtls_ssl_get_version(&ssl_));
        return true;
      }
      lastTls_ = rc;
      if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
        return fail("Falha de negociação TLS", error);
      if (tx_ != previousTx || rx_ != previousRx) {
        previousTx = tx_;
        previousRx = rx_;
        phase(rx_ ? "Negociando TLS; dados recebidos"
                  : "ClientHello enviado; aguardando servidor");
      }
      workerWait(std::chrono::milliseconds(10));
    }
    return false;
  }

public:
  MailTlsSocket(std::function<int(mbedtls_ssl_config *)> trust,
                std::function<bool(std::string &)> before = {},
                std::function<std::string()> details = {},
                unsigned timeoutMs = 30000,
                std::function<int(const char *,const char *,const addrinfo *,addrinfo **)> dns = getaddrinfo)
      : dns_(std::move(dns)), trust_(std::move(trust)), before_(std::move(before)),
        details_(std::move(details)), budget_(timeoutMs) {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&conf_);
    mbedtls_ctr_drbg_init(&random_);
    mbedtls_entropy_init(&entropy_);
  }
  ~MailTlsSocket() override {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&conf_);
    mbedtls_ctr_drbg_free(&random_);
    mbedtls_entropy_free(&entropy_);
    if (fd_ >= 0)
      ::close(fd_);
  }
  bool retryableConnectFailure() const override {
    return (connectionTimeout_ || dnsFailure_) && !secure_;
  }
  std::string diagnostic() const override {
    return report() + (details_ ? "\n" + details_() : "");
  }
  bool open(const std::string &host, unsigned port, bool secure,
            std::string &error) override {
    error.clear();
    connectionTimeout_ = false;
    dnsFailure_ = false;
    if (fd_ >= 0)
      return fail("Conexão já aberta", error);
    host_ = host;
    port_ = port;
    deadline_ = Clock::now() + budget_;
    if (!alive(error) || (before_ && !before_(error)))
      return false;
    phase("Resolvendo DNS IPv4");
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *list = nullptr;
    // A single resolver worker bounds outstanding DNS work. Its owned result
    // survives cancellation of this socket, including late libc/lwIP replies.
    struct Resolution {
      std::atomic<bool> done{false};
      addrinfo *addresses=nullptr;int result=EAI_AGAIN;
      ~Resolution(){if(addresses)freeaddrinfo(addresses);}
    };
    struct Resolver {std::mutex mutex;Executor worker{8192,3};std::shared_ptr<Resolution> pending;};
    static Resolver resolver;
    auto lookup=std::make_shared<Resolution>();
    {
      std::lock_guard<std::mutex> lock(resolver.mutex);
      if(resolver.pending && !resolver.pending->done.load())
        return fail("DNS anterior ainda em andamento; tente novamente",error);
      resolver.worker.join();
      resolver.pending=lookup;
      if(!resolver.worker.submit([lookup,host,port,hints,resolve=dns_]{
        lookup->result=resolve(host.c_str(),std::to_string(port).c_str(),&hints,&lookup->addresses);
        lookup->done=true;
      }))return fail("Não foi possível iniciar DNS",error);
    }
    while(!lookup->done.load()) {
      if(!alive(error))return false;
      workerWait(std::chrono::milliseconds(10));
    }
    if(!alive(error))return false;
    int result=lookup->result;list=lookup->addresses;lookup->addresses=nullptr;
    if (result || !list) {
      dnsFailure_ = result == EAI_FAIL;
#ifdef EAI_AGAIN
      dnsFailure_ = dnsFailure_ || result == EAI_AGAIN;
#endif
      if (list)
        freeaddrinfo(list);
      return fail("Falha DNS (" + std::to_string(result) + ")", error);
    }
    bool connected = false;
    for (auto *entry = list; entry && alive(error); entry = entry->ai_next) {
      fd_ = ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
      if (fd_ < 0) {
        lastErrno_ = errno;
        continue;
      }
      int mode = fcntl(fd_, F_GETFL, 0);
      if (mode < 0 || fcntl(fd_, F_SETFL, mode | O_NONBLOCK) < 0) {
        lastErrno_ = errno;
        ::close(fd_);
        fd_ = -1;
        continue;
      }
      int noDelay = 1;
      if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &noDelay,
                     sizeof(noDelay)) != 0) {
        lastErrno_ = errno;
        ::close(fd_);
        fd_ = -1;
        continue;
      }
      phase("Conectando TCP");
      result = ::connect(fd_, entry->ai_addr, entry->ai_addrlen);
      if (result == 0)
        connected = true;
      else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
        while (alive(error)) {
          fd_set ready;
          FD_ZERO(&ready);
          FD_SET(fd_, &ready);
          timeval wait{0, 10000};
          result = select(fd_ + 1, nullptr, &ready, nullptr, &wait);
          if (result < 0) {
            if (errno == EINTR)
              continue;
            lastErrno_ = errno;
            break;
          }
          if (result > 0) {
            int fault = 0;
            socklen_t n = sizeof(fault);
            if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &fault, &n) == 0 &&
                !fault)
              connected = true;
            else
              lastErrno_ = fault ? fault : errno;
            break;
          }
        }
      } else
        lastErrno_ = errno;
      if (connected)
        break;
      ::close(fd_);
      fd_ = -1;
    }
    freeaddrinfo(list);
    if (!connected) {
      if (!error.empty())
        return false;
      return fail("Falha ao conectar TCP", error);
    }
    lastErrno_ = 0;
    phase("TCP conectado");
    return !secure || tls(host, error);
  }
  bool upgrade(const std::string &host, std::string &error) override {
    if (fd_ < 0 || secure_)
      return fail("Estado inválido para STARTTLS", error);
    return tls(host, error);
  }
  int read(char *data, size_t count) override {
    int n = secure_ ? mbedtls_ssl_read(
                          &ssl_, reinterpret_cast<unsigned char *>(data), count)
                    : int(::recv(fd_, data, count, 0));
    if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE ||
        (!secure_ && n < 0 &&
         (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
      return -2;
    return n;
  }
  int write(const char *data, size_t count) override {
    int n = secure_ ? mbedtls_ssl_write(
                          &ssl_, reinterpret_cast<const unsigned char *>(data),
                          count)
                    : int(::send(fd_, data, count, 0));
    if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE ||
        (!secure_ && n < 0 &&
         (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
      return -2;
    return n;
  }
};
} // namespace tab5
