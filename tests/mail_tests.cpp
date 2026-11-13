// SPDX-License-Identifier: MIT
#include "mail.hpp"
#include "runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace tab5;
namespace fs = std::filesystem;
void check(bool ok, const std::string &s) {
  if (!ok) {
    std::cerr << "FAIL: " << s << '\n';
    exit(1);
  }
}
struct Wire : MailWire {
  std::string input, output, host;
  size_t at = 0, upgradedAt = 0;
  unsigned port = 0;
  bool tls = false, upgradeOK = true;
  explicit Wire(std::string s) : input(std::move(s)) {}
  bool open(const std::string &h, unsigned p, bool secure,
            std::string &) override {
    host = h;
    port = p;
    tls = secure;
    return true;
  }
  bool upgrade(const std::string &, std::string &e) override {
    upgradedAt = output.size();
    if (!upgradeOK) {
      e = "TLS fixture refused";
      return false;
    }
    tls = true;
    return true;
  }
  int read(char *p, size_t) override {
    if (at == input.size())
      return 0;
    *p = input[at++];
    return 1;
  }
  int write(const char *p, size_t n) override {
    n = std::min(n, size_t(7));
    output.append(p, n);
    return int(n);
  }
};
MailAccount config() {
  MailAccount a;
  a.address = "me@example.test";
  a.name = "Pessoa";
  a.inHost = "in.example.test";
  a.inPort = 1993;
  a.inUser = "receive-user";
  a.inPassword = "receive-secret";
  a.outHost = "out.example.test";
  a.outPort = 2465;
  a.outUser = "send-user";
  a.outPassword = "send-secret";
  return a;
}
std::string smtp(const std::string &last = "250 queued\r\n") {
  return "220 ready\r\n250-AUTH PLAIN LOGIN\r\n250 SIZE 999999\r\n235 "
         "auth\r\n250 sender\r\n250 recipient\r\n354 data\r\n" +
         last;
}
void settle(Mail &m) {
  for (unsigned i = 0; i < 500 && m.status().busy; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  check(!m.status().busy, "asynchronous mailbox completes");
}
std::string readFile(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char **argv) {
  check(argc == 2, "SD path");
  std::atomic<bool> cancel{false};
  std::string error;
  auto a = config();
  check(mail::validAccount(a, error),
        "custom ports and separate credentials valid");
  auto bad = a;
  bad.inPort = 65536;
  check(!mail::validAccount(bad, error), "invalid port rejected");
  bad = a;
  bad.inPassword = "pw\r\nDELE 1";
  check(!mail::validAccount(bad, error),
        "credential command injection refused");
  const std::string raw =
      "From: Alice <alice@example.test>\r\nTo: me@example.test\r\nSubject: "
      "=?UTF-8?B?T2zDoQ==?=\r\nContent-Type: text/plain; "
      "charset=iso-8859-1\r\nContent-Transfer-Encoding: "
      "quoted-printable\r\n\r\nOl=E1, mundo!";
  auto parsed = mail::decodeMessage(raw);
  check(parsed.subject == "Olá" && parsed.body == "Olá, mundo!",
        "MIME encoded header and Latin-1 quoted-printable body");
  auto windows = mail::decodeMessage(
      "Content-Type: text/plain; charset=windows-1252\r\n\r\n" +
      std::string("\x80 \x93hi\x94"));
  check(windows.body == "€ “hi”", "Windows-1252 punctuation decoded correctly");
  auto clipped =
      mail::decodeMessage("Content-Type: text/plain; charset=utf-8\r\n\r\n" +
                          std::string(mail::maxBody + 100, 'x'));
  check(clipped.body.size() <= mail::maxBody &&
            clipped.body.find("Texto limitado") != clipped.body.npos,
        "text clipping is visible and stays inside the body budget");
  auto multipart = mail::decodeMessage(
      "Content-Type: multipart/alternative; "
      "boundary=x\r\n\r\n--x\r\nContent-Type: "
      "text/html\r\n\r\n<p>html<script>evil()</script></"
      "p>\r\n--x\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: "
      "base64\r\n\r\nb2ZmbGluZQ==\r\n--x--\r\n");
  check(multipart.body == "offline", "MIME prefers text alternative");
  auto html =
      mail::decodeMessage("Content-Type: text/html\r\n\r\n<p>safe &amp; "
                          "sound</p><script>evil</script><img src=remote>");
  check(html.body.find("evil") == html.body.npos &&
            html.body.find("safe & sound") != html.body.npos,
        "HTML becomes inert text without remote resources");
  std::vector<std::string> folders;
  std::string receivedUid, received;
  Wire imap("* OK ready\r\n* CAPABILITY IMAP4rev1\r\nT1 OK cap\r\nT2 OK "
            "login\r\n* LIST () \"/\" \"INBOX\"\r\n* LIST () \"/\" "
            "Archive\r\nT3 OK list\r\n* OK [UIDVALIDITY 77]\r\nT4 OK "
            "selected\r\n* SEARCH 1 2\r\nT5 OK search\r\n* 2 FETCH "
            "(RFC822.SIZE 200)\r\nT6 OK size\r\n* 2 FETCH (BODY[] {" +
            std::to_string(raw.size()) + "}\r\n" + raw +
            "\r\n)\r\nT7 OK fetched\r\n");
  check(mail::receive(
            imap, a, "INBOX",
            [](const std::string &s) { return s.back() == '1'; },
            [&](const std::string &uid, const std::string &body) {
              receivedUid = uid;
              received = body;
              return true;
            },
            folders, cancel, error),
        "IMAP literal transfer: " + error);
  check(imap.host == a.inHost && imap.port == 1993 &&
            receivedUid == "imap:INBOX:77:2" && received == raw &&
            folders.size() == 2,
        "IMAP identity, port, folders and literal bytes");
  check(imap.output.find("receive-secret") != imap.output.npos &&
            imap.output.find("send-secret") == imap.output.npos &&
            imap.output.find("BODY.PEEK[]<0.1048577>") != imap.output.npos,
        "IMAP incoming password and non-mutating bounded fetch");
  Wire denied("* OK ready\r\n* CAPABILITY IMAP4rev1\r\nT1 OK cap\r\nT2 NO do "
              "not log receive-secret\r\n");
  check(!mail::receive(
            denied, a, "INBOX", [](auto &) { return false; },
            [](auto &, auto &) { return true; }, folders, cancel, error) &&
            error.find("LOGIN T2") != error.npos &&
            error.find("(NO)") != error.npos &&
            error.find("receive-secret") == error.npos,
        "IMAP refusal preserves safe operation but not server-echoed secrets");
  auto bounded = imap.input;
  auto existsAt = bounded.find("* OK [UIDVALIDITY");
  bounded.insert(existsAt, "* 50000 EXISTS\r\n");
  Wire largeMailbox(bounded);
  check(mail::receive(
            largeMailbox, a, "INBOX",
            [](const std::string &s) { return s.back() == '1'; },
            [](auto &, auto &) { return true; }, folders, cancel, error) &&
            largeMailbox.output.find("UID SEARCH 49801:*") !=
                largeMailbox.output.npos,
        "large inbox searches only the latest 200 sequences");
  auto oversized = imap.input;
  auto literalAt = oversized.find("BODY[] {") + 8;
  auto literalEnd = oversized.find('}', literalAt);
  oversized.replace(literalAt, literalEnd - literalAt,
                    std::to_string(mail::maxRaw + 1));
  Wire tooLarge(oversized);
  bool cachedLarge = false;
  check(!mail::receive(
            tooLarge, a, "INBOX",
            [](const std::string &s) { return s.back() == '1'; },
            [&](auto &, auto &) {
              cachedLarge = true;
              return true;
            },
            folders, cancel, error) &&
            !cachedLarge && error.find("limite") != error.npos,
        "oversized message is rejected instead of silently cached truncated");
  auto headerScript = imap.input;
  auto sizeAt = headerScript.find("RFC822.SIZE 200");
  headerScript.replace(sizeAt, 15, "RFC822.SIZE 5000000");
  Wire headerOnly(headerScript);
  std::string largePreview;
  check(mail::receive(
            headerOnly, a, "INBOX",
            [](const std::string &s) { return s.back() == '1'; },
            [&](auto &, const std::string &preview) {
              largePreview = preview;
              return true;
            },
            folders, cancel, error) &&
            headerOnly.output.find("HEADER.FIELDS") != headerOnly.output.npos &&
            headerOnly.output.find("BODY.PEEK[]") == headerOnly.output.npos &&
            largePreview.find("permanece no servidor") != largePreview.npos,
        "known-large messages cache headers without requesting the body");
  auto popAccount = a;
  popAccount.protocol = "pop";
  popAccount.inPort = 2995;
  Wire pop(
      "+OK ready\r\n+OK user\r\n+OK pass\r\n+OK ids\r\n1 remote-1\r\n.\r\n+OK "
      "message\r\nFrom: a@example.test\r\nSubject: POP\r\n\r\n..dot\r\n.\r\n");
  check(mail::receive(
            pop, popAccount, "INBOX", [](auto &) { return false; },
            [&](auto &, const std::string &body) {
              received = body;
              return true;
            },
            folders, cancel, error) &&
            received.find(".dot") != received.npos,
        "POP UIDL and dot unstuffing");
  check(pop.output.find("DELE") == pop.output.npos && pop.port == 2995,
        "POP leaves server messages intact and uses configured port");
  MailMessage message;
  message.to = "alice@example.test";
  message.subject = "Olá";
  message.body = "Texto\n. ponto";
  message.messageId = "<stable@tab5.local>";
  Wire outgoing(smtp());
  check(mail::send(outgoing, a, message, cancel, error) ==
            mail::SendResult::accepted,
        "SMTP positive final DATA acknowledgement");
  check(outgoing.port == 2465 &&
            outgoing.output.find("AUTH PLAIN " +
                                 mail::base64(std::string(1, '\0') + a.outUser +
                                              std::string(1, '\0') +
                                              a.outPassword)) !=
                outgoing.output.npos &&
            outgoing.output.find("receive-secret") == outgoing.output.npos,
        "SMTP separate password and custom port");
  auto rejectedScript=smtp();auto authPos=rejectedScript.find("235 auth\r\n");rejectedScript.replace(authPos,10,"535 5.7.8 auth refused\r\n");Wire rejectedAuth(rejectedScript);
  check(mail::send(rejectedAuth,a,message,cancel,error)==mail::SendResult::rejected && error.find("SMTP 535:")!=error.npos && rejectedAuth.output.find("MAIL FROM")==rejectedAuth.output.npos,"535 is an authentication rejection, never attempts DATA");
  check(error.find(a.outPassword)==error.npos && error.find(a.inPassword)==error.npos,"535 diagnostic contains no secrets");
  auto missingPassword=a;missingPassword.outPassword="";Wire emptyPassword(smtp());check(mail::send(emptyPassword,missingPassword,message,cancel,error)==mail::SendResult::rejected && error.find("vazia")!=error.npos && emptyPassword.output.empty(),"empty outgoing password refused before authentication");
  auto challengeText = smtp();
  auto authAt = challengeText.find("235 auth\r\n");
  challengeText.replace(authAt, 10, "334 challenge\r\n235 auth\r\n");
  Wire challenge(challengeText);
  check(mail::send(challenge, a, message, cancel, error) ==
            mail::SendResult::accepted,
        "AUTH PLAIN supports a 334 challenge after initial response");
  for (auto reply : {"251 forwarded\r\n", "252 accepted\r\n"}) {
    auto redirected = smtp();
    auto recipientAt = redirected.find("250 recipient\r\n");
    redirected.replace(recipientAt, 15, reply);
    Wire recipient(redirected);
    check(mail::send(recipient, a, message, cancel, error) ==
              mail::SendResult::accepted,
          "SMTP accepts valid recipient forwarding replies");
  }
  Wire dropped(smtp(""));
  check(mail::send(dropped, a, message, cancel, error) ==
            mail::SendResult::uncertain,
        "lost final acknowledgement is uncertain, not sent");
  auto st = a;
  st.outSecurity = "starttls";
  st.outPort = 2587;
  Wire upgraded("220 ready\r\n250-STARTTLS\r\n250 AUTH PLAIN\r\n220 tls\r\n250 "
                "AUTH PLAIN\r\n235 auth\r\n250 sender\r\n250 rcpt\r\n354 "
                "data\r\n250 queued\r\n");
  check(mail::send(upgraded, st, message, cancel, error) ==
                mail::SendResult::accepted &&
            upgraded.tls &&
            upgraded.output.substr(0, upgraded.upgradedAt).find("AUTH ") ==
                std::string::npos,
        "SMTP STARTTLS precedes credentials and EHLO is repeated");
  Wire tlsFailure("220 ready\r\n250-STARTTLS\r\n250 AUTH PLAIN\r\n220 tls\r\n");
  tlsFailure.upgradeOK = false;
  check(mail::send(tlsFailure, st, message, cancel, error) ==
                mail::SendResult::rejected &&
            tlsFailure.output.find("AUTH PLAIN ") == std::string::npos,
        "failed STARTTLS never authenticates in cleartext");
  Wire noTLS("220 ready\r\n250 AUTH PLAIN\r\n");
  check(mail::send(noTLS, st, message, cancel, error) ==
                mail::SendResult::rejected &&
            noTLS.output.find("AUTH PLAIN ") == std::string::npos,
        "missing STARTTLS cannot downgrade");
  fs::path root =
      fs::temp_directory_path() / ("tab5-mail-" + std::to_string(getpid()));
  fs::copy(argv[1], root, fs::copy_options::recursive);
  Storage disk(root.string());
  check(disk.mount(), "mail SD mount");
  Mail mailbox(disk);
  check(mailbox.configure(a, error), "persist independent credentials");
  auto account = mailbox.account();
  check(account.inPassword != account.outPassword && account.inPort == 1993 &&
            account.outPort == 2465,
        "credentials and ports survive configuration");
  auto inboxScript =
      imap.input +
      "* 1 FETCH (RFC822.SIZE 200)\r\nT8 OK size\r\n* 1 FETCH (BODY[] {" +
      std::to_string(raw.size()) + "}\r\n" + raw + "\r\n)\r\nT9 OK fetched\r\n";
  struct FirstTimeout : Wire {
    FirstTimeout() : Wire("") {}
    bool open(const std::string &, unsigned, bool,
              std::string &error) override {
      error = "Prazo de conexão esgotado";
      return false;
    }
    bool retryableConnectFailure() const override { return true; }
  };
  auto recoveries = std::make_shared<std::atomic<unsigned>>(0);
  mailbox.recoverTransport = [recoveries] {
    ++*recoveries;
    return true;
  };
  auto connectionAttempts = std::make_shared<std::atomic<unsigned>>(0);
  mailbox.wire = [inboxScript,
                  connectionAttempts]() -> std::unique_ptr<MailWire> {
    if (connectionAttempts->fetch_add(1) == 0)
      return std::make_unique<FirstTimeout>();
    return std::make_unique<Wire>(inboxScript);
  };
  check(mailbox.synchronize("INBOX", error), "start two-message receive");
  settle(mailbox);
  auto inbox = mailbox.list("inbox");
  check(inbox.size() == 2 && inbox[0].uid.back() == '2' &&
            inbox[1].uid.back() == '1',
        "IMAP list remains newest-first despite fetch arrival order");
  check(mailbox.folders().size() == 2, "remote folder cache tagged to account");
  check(connectionAttempts->load() == 2,
        "only a pre-authentication connect timeout retries once");
  auto refusedAttempts = std::make_shared<std::atomic<unsigned>>(0);
  mailbox.wire = [script = denied.input, refusedAttempts] {
    ++*refusedAttempts;
    return std::make_unique<Wire>(script);
  };
  check(mailbox.synchronize("INBOX", error), "start refused login");
  settle(mailbox);
  check(refusedAttempts->load() == 1 &&
            mailbox.status().message.find("(NO)") != std::string::npos,
        "authentication refusal is not retried");
  check(recoveries->load() == 0,
        "successful retry and refused login cannot reset network");
  mailbox.wire = [] { return std::make_unique<FirstTimeout>(); };
  check(mailbox.synchronize("INBOX", error), "persistent transport timeout");
  settle(mailbox);
  check(recoveries->load() == 1 &&
            mailbox.status().message.find("Recuperação do Wi-Fi agendada") !=
                std::string::npos,
        "persistent pre-auth timeout schedules recovery once");
  MailMessage draft = message;
  check(mailbox.draft(draft, false, error) &&
            mailbox.list("drafts").size() == 1,
        "offline draft");
  check(mailbox.draft(draft, true, error) && mailbox.list("outbox").size() == 1,
        "offline outgoing queue");
  MailMessage metadata;
  metadata.to = std::string(600, 'x');
  metadata.subject = "x";
  for (unsigned i = 0; i < 99; ++i)
    metadata.subject += "é";
  metadata.body = "offline";
  check(mailbox.draft(metadata, false, error), "long metadata draft persists");
  auto compact = mailbox.list("drafts");
  MailMessage fullMetadata;
  check(compact.size() == 1 && compact[0].subject.size() <= 160 &&
            compact[0].to.size() <= 120 && compact[0].body.empty() &&
            compact[0].messageId.empty() &&
            mailbox.read(metadata.id, fullMetadata) &&
            fullMetadata.to == metadata.to &&
            fullMetadata.subject == metadata.subject,
        "compact memory summaries preserve complete data on SD");
  check(mailbox.move(metadata.id, "trash", error) &&
            mailbox.move(metadata.id, "delete", error),
        "remove metadata fixture");
  auto other = a;
  other.address = "other@example.test";
  check(mailbox.configure(other, error), "switch account");
  MailMessage hidden;
  check(mailbox.list("outbox").empty() && mailbox.status().queued == 0 &&
            !mailbox.read(draft.id, hidden) &&
            !mailbox.move(draft.id, "trash", error),
        "old account records and queued count stay isolated");
  check(mailbox.folders() == std::vector<std::string>{"INBOX"},
        "remote folders do not cross account boundaries");
  check(mailbox.configure(a, error) && mailbox.read(draft.id, hidden),
        "switching back preserves original message");
  auto submissions = std::make_shared<std::atomic<unsigned>>(0);
  mailbox.wire = [submissions] {
    ++*submissions;
    return std::make_unique<Wire>(smtp(""));
  };
  check(mailbox.flush(error), "async SMTP start");
  settle(mailbox);
  MailMessage stored;
  check(mailbox.read(draft.id, stored) && stored.status == "uncertain" &&
            stored.folder == "outbox",
        "ambiguous submission remains in outbox");
  auto automaticAccount = a;
  automaticAccount.automatic = true;
  check(mailbox.configure(automaticAccount, error),
        "enable automatic queue for uncertain-state test");
  mailbox.poll(100000, true);
  settle(mailbox);
  check(submissions->load() == 1,
        "automatic polling cannot duplicate an uncertain submission");
  check(mailbox.configure(a, error), "restore manual queue");
  check(mailbox.read(draft.id, stored) && stored.status == "uncertain",
        "uncertain message is not automatically resent");
  check(mailbox.move(draft.id, "outbox", error), "manual retry authorization");
  mailbox.wire = [] { return std::make_unique<Wire>(smtp()); };
  check(mailbox.flush(error), "retry start");
  settle(mailbox);
  check(mailbox.read(draft.id, stored) && stored.folder == "sent" &&
            stored.messageId == "<" + draft.id + "@tab5.local>",
        "stable message ID and accepted sent state");
  MailMessage authDraft;authDraft.to="receiver@example.test";authDraft.subject="Retry auth";authDraft.body="Local fixture";
  check(mailbox.draft(authDraft,true,error),"queue authentication retry fixture");
  mailbox.wire=[] {return std::make_unique<Wire>("220 ready\r\n250 AUTH PLAIN\r\n535 5.7.8 rejected\r\n");};
  check(mailbox.flush(error),"attempt rejected authentication");settle(mailbox);check(mailbox.read(authDraft.id,stored)&&stored.status=="failed","authentication rejection stays local");
  auto fixedCredentials=a;fixedCredentials.outPassword="corrected-outgoing-secret";check(mailbox.configure(fixedCredentials,error),"update outgoing password");
  mailbox.wire=[] {return std::make_unique<Wire>(smtp());};check(mailbox.flush(error),"automatic mode ignores rejected items");settle(mailbox);check(mailbox.read(authDraft.id,stored)&&stored.status=="failed","rejected message is not retried automatically");
  check(mailbox.flush(error,true),"manual queue includes rejected items after correcting password");settle(mailbox);check(mailbox.read(authDraft.id,stored)&&stored.status=="accepted"&&stored.folder=="sent","corrected password and explicit resend move message to Sent");
  check(mailbox.configure(a,error),"restore original fixture account");
  std::string diagnosticLog;
  check(disk.read("/logs/mail.txt", diagnosticLog) &&
            diagnosticLog.find("runtime=1.0.0") != diagnosticLog.npos &&
            diagnosticLog.find("receive-secret") == diagnosticLog.npos &&
            diagnosticLog.find("send-secret") == diagnosticLog.npos,
        "mail diagnostic persisted without credentials");
  mailbox.stop();
  Mail reopened(disk);
  check(reopened.list("sent").size() == 2,
        "mailbox cache survives service restart");
  check(reopened.move(draft.id, "trash", error) &&
            reopened.move(draft.id, "delete", error),
        "local trash and explicit local purge");
  auto entered = std::make_shared<std::atomic<bool>>(false);
  struct WaitingWire : Wire {
    std::shared_ptr<std::atomic<bool>> entered;
    explicit WaitingWire(std::shared_ptr<std::atomic<bool>> e)
        : Wire(""), entered(e) {}
    bool open(const std::string &, unsigned, bool,
              std::string &error) override {
      if (progress)
        progress("TCP conectado; aguardando TLS simulado");
      *entered = true;
      while (!cancelled())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      error = "Operação cancelada";
      return false;
    }
  };
  reopened.wire = [entered] { return std::make_unique<WaitingWire>(entered); };
  check(reopened.synchronize("INBOX", error), "start pending connection");
  for (unsigned i = 0; i < 1000 && !entered->load(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  check(entered->load() && reopened.status().busy &&
            reopened.status().message.find("TLS simulado") != std::string::npos,
        "live connection progress available while worker is pending");
  auto beforeCancel = std::chrono::steady_clock::now();
  reopened.cancel();
  check(std::chrono::steady_clock::now() - beforeCancel <
            std::chrono::milliseconds(50),
        "cancel returns immediately without joining network worker");
  settle(reopened);
  check(!reopened.status().busy &&
            reopened.status().message == "Operação cancelada",
        "pending connection cooperates with cancellation");
  Runtime runtime(disk);
  check(runtime.boot() && runtime.open("email"), "mail app boots on API14");
  check(runtime.dispatch("diagnostic") &&
            runtime.view().layout == "diagnostic" && runtime.view().readonly,
        "mail diagnostic is a complete readonly panel");
  runtime.network().backend.restartRequired=[] {return true;};
  check(runtime.dispatch("diagnostic") &&
        runtime.view().body.find("reinicie") != std::string::npos,
        "mail-only capability receives restart-required network snapshot");
  check(runtime.dispatch("restart_help") && runtime.current()=="energy",
        "restart action opens settings without rebooting");
  check(runtime.open("email"), "reopen mail after restart guidance");
  runtime.network().backend.restartRequired={};
  check(runtime.dispatch("settings") && runtime.dispatch("incoming"),
        "incoming configuration UI");
  check(runtime.view().controls.size() == 5 &&
            runtime.view().controls[4].type == "password" &&
            runtime.view().text.find("1993") != runtime.view().text.npos,
        "incoming editable port and masked independent password");
  check(runtime.dispatch("back") && runtime.dispatch("new"), "new mail screen");
  check(runtime.view().layout == "mail_compose" &&
            runtime.view().controls[2].type == "multiline",
        "mail composition uses a multiline body");
  check(runtime.dispatch("save_draft",
                         "alice@example.test\nAssunto\nlinha 1\nlinha 2"),
        "save multiline draft through Lua API");
  check(runtime.mail().list("drafts").size() == 1,
        "UI draft persisted offline");
  for(const auto &action:{"folders","settings"}) {
    check(runtime.open("email")&&runtime.dispatch("new"),"new draft before navigation");
    check(runtime.dispatch(action,std::string("saved@example.test\nLeaving ")+action+"\nPersistent body"),"leave compose with unsaved fields");
    bool saved=false;for(const auto &item:runtime.mail().list("drafts")){MailMessage message;if(runtime.mail().read(item.id,message)&&message.subject==std::string("Leaving ")+action)saved=message.body=="Persistent body";}
    check(saved,"navigation preserves draft fields");
  }
  check(runtime.open("email")&&runtime.dispatch("new"),"draft before failed autosave");
  runtime.storage().setFreeSpaceProvider([]{return uint64_t(0);});
  check(!runtime.dispatch("folders","keep@example.test\nMust stay\nUnsaved text")&&runtime.view().layout=="mail_compose","failed autosave blocks navigation");
  runtime.storage().setFreeSpaceProvider({});
  check(runtime.dispatch("folders"),"draft can save after storage recovers");
  runtime.shutdown();
  fs::remove_all(root);
  std::cout << "Mail protocols, TLS ordering, credentials, ports, MIME, "
               "offline queue, uncertain SMTP and UI passed\n";
}
