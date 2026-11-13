// SPDX-License-Identifier: MIT
#include "mail_tls.hpp"
#include "mbedtls/ssl_ciphersuites.h"
#include "mbedtls/x509_crt.h"
#include <iostream>
#include <thread>
using namespace tab5;
static void check(bool ok, const std::string &why) {
  if (!ok) {
    std::cerr << why << '\n';
    std::exit(1);
  }
}
int main(int argc, char **argv) {
  if(argc==2&&std::string(argv[1])=="dns") {
    std::atomic<bool> release{false},started{false},cancel{false},finished{false};
    auto slow=[&](const char *,const char *,const addrinfo *,addrinfo **){started=true;while(!release)std::this_thread::sleep_for(std::chrono::milliseconds(2));finished=true;return EAI_NONAME;};
    std::string error;
    {
      MailTlsSocket wire({}, {}, {}, 2000,slow);wire.cancellation=&cancel;
      std::thread timer([&]{while(!started)std::this_thread::sleep_for(std::chrono::milliseconds(1));cancel=true;});
      auto begin=std::chrono::steady_clock::now();bool ok=wire.open("stalled.test",443,false,error);timer.join();
      check(!ok&&error.find("cancelada")!=error.npos&&std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(500),"DNS cancellation bounded");
    }
    {MailTlsSocket next({}, {}, {}, 50,slow);check(!next.open("stalled.test",443,false,error)&&error.find("anterior")!=error.npos,"one outstanding DNS job");}
    release=true;while(!finished)std::this_thread::sleep_for(std::chrono::milliseconds(1));std::this_thread::sleep_for(std::chrono::milliseconds(20));
    release=false;started=false;finished=false;
    {MailTlsSocket wire({}, {}, {}, 60,slow);auto begin=std::chrono::steady_clock::now();check(!wire.open("stalled.test",443,false,error)&&error.find("Prazo")!=error.npos&&std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(500),"DNS deadline bounded");}
    release=true;while(!finished)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::cout<<"DNS cancellation, deadline, bounded worker and late completion passed\n";return 0;
  }
  check(argc == 5, "host port CA mode required");
  std::string mode = argv[4], error, progress, history;
  mbedtls_x509_crt ca;
  mbedtls_x509_crt_init(&ca);
  check(mbedtls_x509_crt_parse_file(&ca, argv[3]) == 0, "load CA");
  {
    MailTlsSocket wire(
        [&](mbedtls_ssl_config *conf) {
          mbedtls_ssl_conf_ca_chain(conf, &ca, nullptr);
          return 0;
        },
        {}, {}, mode == "timeout" ? 100 : 30000);
    wire.progress = [&](const std::string &s) {
      progress = s;
      history += s + "\n";
    };
    std::atomic<bool> cancel{false};
    wire.cancellation = &cancel;
    std::thread timer;
    if (mode == "cancel")
      timer = std::thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        cancel = true;
      });
    auto begin = std::chrono::steady_clock::now();
    bool integration = mode == "imap" || mode == "imap-stall" ||
                       mode == "smtp" || mode == "smtp-drop";
    bool ok = integration || wire.open(argv[1], unsigned(std::stoul(argv[2])),
                                       mode != "starttls", error);
    if (timer.joinable())
      timer.join();
    if (mode == "smtp" || mode == "smtp-drop") {
      MailAccount account;
      account.address = "local@example.test";
      account.outHost = argv[1];
      account.outPort = unsigned(std::stoul(argv[2]));
      account.outUser = "sender";
      account.outPassword = "outgoing-only-secret";
      MailMessage message;
      message.to = "alice@example.test";
      message.subject = "TLS integration";
      message.body = "Hello from a real TLS SMTP session";
      message.messageId = "<integration@tab5.local>";
      auto result = mail::send(wire, account, message, cancel, error);
      check(result == (mode == "smtp" ? mail::SendResult::accepted
                                      : mail::SendResult::uncertain),
            "real TLS SMTP result incorrect: " + error);
      check(history.find("outgoing-only-secret") == history.npos &&
                error.find("outgoing-only-secret") == error.npos,
            "SMTP secret leaked");
    } else if (integration) {
      MailAccount account;
      account.protocol = "imap";
      account.inHost = argv[1];
      account.inPort = unsigned(std::stoul(argv[2]));
      account.inUser = "reader";
      account.inPassword = "incoming-only-secret";
      std::vector<std::string> folders;
      std::string message;
      if (mode == "imap-stall")
        wire.protocolTimeoutMs = 100;
      else
        wire.protocolTimeoutMs = 200;
      bool received = mail::receive(
          wire, account, "INBOX", [](const std::string &) { return false; },
          [&](const std::string &, const std::string &raw) {
            message = raw;
            return true;
          },
          folders, cancel, error);
      if (mode == "imap")
        check(received && message.size() > 50000 &&
                  message.find("Subject: Integration") != message.npos,
              "real TLS IMAP receive failed: " + error);
      else
        check(!received && error.find("IMAP LOGIN T2") != error.npos &&
                  error.find(" RX=") != error.npos,
              "protocol timeout lost its stage/transport: " + error);
      check(history.find("incoming-only-secret") == history.npos &&
                error.find("incoming-only-secret") == error.npos,
            "credential leaked into diagnostic");
    } else if (mode == "cancel") {
      check(!ok && error == "Operação cancelada",
            "cancel stalled TLS: " + error);
      check(std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
            "cancel too slow");
    } else if (mode == "timeout") {
      check(!ok && error.find("Prazo de conexão esgotado") != error.npos &&
                error.find("TLS TX=") != error.npos,
            "stalled handshake must time out with counters: " + error);
      check(std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
            "timeout deadline ignored");
    } else if (mode == "reject") {
      check(!wire.retryableConnectFailure(),
            "certificate failure must not request automatic retry");
      check(!ok && error.find("Falha de negociação TLS") != error.npos,
            "invalid certificate accepted: " + error);
    } else {
      check(ok, error);
      auto writeAll = [&](const std::string &s) {
        size_t at = 0;
        while (at < s.size()) {
          int n = wire.write(s.data() + at, s.size() - at);
          if (n == -2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
          }
          check(n > 0, "write failed");
          at += size_t(n);
        }
      };
      auto readCount = [&](size_t count) {
        std::string out;
        auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (out.size() < count) {
          check(std::chrono::steady_clock::now() < deadline, "read timed out");
          char b[2048];
          int n = wire.read(b, std::min(sizeof(b), count - out.size()));
          if (n == -2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
          }
          check(n > 0, "read failed");
          out.append(b, n);
        }
        return out;
      };
      if (mode == "starttls") {
        writeAll("STARTTLS\r\n");
        check(readCount(11) == "220 ready\r\n", "STARTTLS greeting");
        check(wire.upgrade(argv[1], error), error);
      }
      if (mode == "greeting") {
        auto prefix = readCount(4);
        check(prefix == "* OK", "IMAP greeting invalid");
        std::cout << progress << '\n';
      } else {
        std::string payload(32768, 'x');
        payload[0] = 'P';
        writeAll(payload);
        check(readCount(payload.size()) == payload,
              "TLS fragmented payload mismatch");
      }
    }
  }
  mbedtls_x509_crt_free(&ca);
  std::cout << "real mbedTLS transport passed: " << mode << '\n';
}
