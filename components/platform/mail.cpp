// SPDX-License-Identifier: MIT
#include "mail.hpp"
#include "text_lines.hpp"
#include <algorithm>
#include <chrono>
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif
namespace tab5 {
namespace {
const std::string root = "/data/mail/messages/",
                  config = "/config/mail/account";
std::string record(const std::vector<std::string> &v) {
  std::string out = "T5MAIL1\n";
  for (auto &s : v)
    out += mail::hex(s) + "\n";
  return out;
}
std::vector<std::string> fields(const std::string &s) {
  TextLines in(s);
  std::string line;
  std::vector<std::string> v;
  if (!nextLine(in, line) || line != "T5MAIL1")
    return {};
  while (nextLine(in, line)) {
    if (line.size() % 2 ||
        line.find_first_not_of("0123456789abcdef") != line.npos)
      return {};
    v.push_back(mail::unhex(line));
    if (v.size() > 96)
      return {};
  }
  return v;
}
std::string pack(const MailMessage &m) {
  std::vector<std::string> values={m.id, m.from, m.to, m.subject, m.body, m.date, m.uid,
                 m.messageId, m.folder, m.status, m.seen ? "1" : "0", m.account,
                 std::to_string(m.order),m.html};
  values.push_back("@T5ATT2");values.push_back(std::to_string(m.images.size()));
  for(const auto &image:m.images){values.push_back(image.cid);values.push_back(image.type);values.push_back(image.data);}
  values.push_back(std::to_string(m.attachments.size()));
  for(const auto &a:m.attachments){values.push_back(a.name);values.push_back(a.type);values.push_back(a.path);values.push_back(std::to_string(a.size));}
  return record(values);
}
bool unpack(const std::string &raw, MailMessage &m) {
  auto v = fields(raw);
  if (v.size()!=13 && v.size()<14)
    return false;
  m = {};
  m.id = v[0];
  m.from = v[1];
  m.to = v[2];
  m.subject = v[3];
  m.body = v[4];
  m.date = v[5];
  m.uid = v[6];
  m.messageId = v[7];
  m.folder = v[8];
  m.status = v[9];
  m.seen = v[10] == "1";
  m.account = v[11];
  m.order = strtoull(v[12].c_str(), nullptr, 10);
  if(v.size()>13){m.html=v[13];size_t p=14;
    if(v.size()>16 && v[14]=="@T5ATT2"){
      unsigned count=strtoul(v[15].c_str(),nullptr,10);p=16;if(count>6||p+count*3>=v.size())return false;
      for(unsigned i=0;i<count;++i,p+=3)m.images.push_back({v[p],v[p+1],v[p+2]});
      count=strtoul(v[p++].c_str(),nullptr,10);if(count>8||p+count*4!=v.size())return false;
      for(unsigned i=0;i<count;++i,p+=4)m.attachments.push_back({v[p],v[p+1],v[p+2],strtoull(v[p+3].c_str(),nullptr,10)});
    }else {if((v.size()-14)%3 || v.size()>32)return false;for(;p+2<v.size();p+=3)m.images.push_back({v[p],v[p+1],v[p+2]});}
  }
  return m.id.size() == 24 &&
         m.id.find_first_not_of("0123456789abcdef") == m.id.npos;
}
MailMessage summary(MailMessage m) {
  std::string().swap(m.body);
  std::string().swap(m.html);std::vector<MailImage>().swap(m.images);std::vector<MailAttachment>().swap(m.attachments);
  std::string().swap(m.messageId);
  auto compact = [](std::string &value, size_t cap) {
    if (value.size() <= cap)
      return;
    while (cap && (static_cast<unsigned char>(value[cap]) & 0xc0) == 0x80)
      --cap;
    std::string(value.data(), cap).swap(value);
  };
  compact(m.subject, 160);
  compact(m.from, 160);
  compact(m.to, 120);
  compact(m.date, 80);
  return m;
}
bool idValid(const std::string &id) {
  return id.size() == 24 && id.find_first_not_of("0123456789abcdef") == id.npos;
}
} // namespace
bool Mail::load() {
  if (loaded_)
    return true;
  disk_.mkdir("/data/mail");
  disk_.mkdir("/data/mail/messages");
  disk_.mkdir("/data/mail/attachments");
  disk_.mkdir("/config/mail");
  std::vector<File> files;
  if (!disk_.listDir("/data/mail/messages", files))
    return false;
  index_.clear();
  for (auto &f : files) {
    auto name = f.path.substr(f.path.find_last_of('/') + 1);
    if (!idValid(name))
      continue;
    std::string raw;
    MailMessage m;
    if (index_.size() >= mail::maxMessages)
      break;
    if (disk_.read(root + name, raw, 2*1024*1024) && unpack(raw, m)) {
      if (m.status == "sending") {
        m.status = "uncertain";
        disk_.write(root + m.id, pack(m));
      }
      index_[m.id] = summary(m);
    }
  }
  std::string raw;
  if (disk_.read("/data/mail/folders", raw, 16000)) {
    auto v = fields(raw);
    if (!v.empty() && v[0].rfind("account:", 0) == 0) {
      foldersAccount_ = v[0].substr(8);
      folders_.assign(v.begin() + 1, v.end());
    }
  }
  loaded_ = true;
  return true;
}
bool Mail::put(const MailMessage &message) {
  auto m = message;
  if (!m.order) {
    auto old = index_.find(m.id);
    if (old != index_.end())
      m.order = old->second.order;
    else {
      for (const auto &entry : index_)
        m.order = std::max(m.order, entry.second.order);
      ++m.order;
    }
  }
  if (!idValid(m.id) || !disk_.write(root + m.id, pack(m)))
    return false;
  index_[m.id] = summary(m);
  return true;
}
MailAccount Mail::account() {
  std::lock_guard<std::mutex> g(mutex_);
  if (accountLoaded_)
    return accountCache_;
  MailAccount a;
  std::string raw;
  if (!disk_.read(config, raw, 12000)) {
    accountCache_ = a;
    accountLoaded_ = true;
    return a;
  }
  auto v = fields(raw);
  if (v.size() != 14)
    return a;
  a.address = v[0];
  a.name = v[1];
  a.protocol = v[2];
  a.inHost = v[3];
  a.inPort = unsigned(strtoul(v[4].c_str(), nullptr, 10));
  a.inSecurity = v[5];
  a.inUser = v[6];
  a.inPassword = v[7];
  a.outHost = v[8];
  a.outPort = unsigned(strtoul(v[9].c_str(), nullptr, 10));
  a.outSecurity = v[10];
  a.outUser = v[11];
  a.outPassword = v[12];
  a.automatic = v[13] == "1";
  accountCache_ = a;
  accountLoaded_ = true;
  return a;
}
static std::string identity(const MailAccount &a) {
  return sha256(a.address + "\n" + a.protocol + "\n" + a.inHost + "\n" +
                a.inUser)
      .substr(0, 24);
}
bool Mail::configure(const MailAccount &a, std::string &error) {
  if (!mail::validAccount(a, error))
    return false;
  std::lock_guard<std::mutex> g(mutex_);
  if (busy_) {
    error = "Aguarde a operação de e-mail";
    return false;
  }
  disk_.mkdir("/config/mail");
  if (!disk_.write(config,
                   record({a.address, a.name, a.protocol, a.inHost,
                           std::to_string(a.inPort), a.inSecurity, a.inUser,
                           a.inPassword, a.outHost, std::to_string(a.outPort),
                           a.outSecurity, a.outUser, a.outPassword,
                           a.automatic ? "1" : "0"}))) {
    error = "Não foi possível salvar a conta no SD";
    return false;
  }
  accountCache_ = a;
  accountLoaded_ = true;
  if (foldersAccount_ != identity(a)) {
    folders_ = {"INBOX"};
    foldersAccount_ = identity(a);
  }
  message_ = "Conta salva";
  return true;
}
void Mail::stop() {
  cancel_ = true;
  if (worker_ && worker_->joinable())
    worker_->join();
  worker_.reset();
  std::lock_guard<std::mutex> g(mutex_);
  busy_ = false;
  completed_ = false;
  loaded_ = false;
  accountLoaded_ = false;
  accountCache_ = {};
  index_.clear();
  deletedKeys_.clear();
  deletionsLoaded_ = false;
  folders_ = {"INBOX"};
  foldersAccount_.clear();
}
void Mail::report(const std::string &s) {
  {
    std::lock_guard<std::mutex> g(mutex_);
    message_ = s;
  }
  disk_.write("/logs/mail.txt", "runtime=1.0.0\n" + s + "\n");
}
void Mail::finish(const std::string &s) {
  const auto previousError = disk_.lastError();
  disk_.mkdir("/logs");
  disk_.write("/logs/mail.txt", "runtime=1.0.0\n" + s + "\n");
  disk_.restoreDiagnosticError(previousError);
  std::lock_guard<std::mutex> g(mutex_);
  message_ = s;
  busy_ = false;
  completed_ = true;
}
bool Mail::start(std::function<void()> fn) {
  if (worker_ && worker_->joinable()) {
    if (!completed_)
      return false;
    worker_->join();
  }
  {
    std::lock_guard<std::mutex> g(mutex_);
    if (busy_)
      return false;
    busy_ = true;
    message_ = "Conectando ao servidor...";
    cancel_ = false;
    completed_ = false;
  }
  if (!worker_)
    worker_ = std::make_unique<Executor>(32768, 3,true);
  if (!worker_->submit(std::move(fn))) {
    finish("Memória insuficiente para e-mail");
    return false;
  }
  return true;
}
Mail::Status Mail::status() {
  auto a = account();
  std::lock_guard<std::mutex> g(mutex_);
  load();
  Status s;
  s.busy = busy_;
  s.message = message_;
  for (auto &v : index_)
    if (v.second.account == identity(a) && v.second.folder == "outbox" &&
        v.second.status == "queued")
      ++s.queued;
  return s;
}
std::vector<MailMessage> Mail::list(const std::string &folder) {
  auto a = account();
  std::lock_guard<std::mutex> g(mutex_);
  load();
  std::vector<MailMessage> out;
  for (auto &p : index_)
    if (p.second.folder == folder && p.second.account == identity(a))
      out.push_back(p.second);
  auto uidKey = [](const MailMessage &m) -> uint64_t {
    if (m.uid.rfind("imap:", 0) != 0)
      return 0;
    auto last = m.uid.rfind(':');
    if (last == m.uid.npos || !last)
      return 0;
    auto previous = m.uid.rfind(':', last - 1);
    if (previous == m.uid.npos)
      return 0;
    auto validity = m.uid.substr(previous + 1, last - previous - 1),
         uid = m.uid.substr(last + 1);
    if (validity.empty() || uid.empty() ||
        validity.find_first_not_of("0123456789") != validity.npos ||
        uid.find_first_not_of("0123456789") != uid.npos)
      return 0;
    auto v = strtoull(validity.c_str(), nullptr, 10),
         u = strtoull(uid.c_str(), nullptr, 10);
    return v <= UINT32_MAX && u <= UINT32_MAX ? (uint64_t(v) << 32) | u : 0;
  };
  std::sort(out.begin(), out.end(),
            [&](const MailMessage &a, const MailMessage &b) {
              if (folder == "inbox" || folder.rfind("remote:", 0) == 0) {
                auto ak = uidKey(a), bk = uidKey(b);
                if (ak && bk && ak != bk)
                  return ak > bk;
              }
              return a.order > b.order;
            });
  return out;
}
std::vector<std::string> Mail::folders() {
  auto a = account();
  std::lock_guard<std::mutex> g(mutex_);
  load();
  return foldersAccount_ == identity(a) ? folders_
                                        : std::vector<std::string>{"INBOX"};
}
bool Mail::readStored(const std::string &id, MailMessage &m) {
  if (!idValid(id))
    return false;
  std::string raw;
  return disk_.read(root + id, raw, 2*1024*1024) && unpack(raw, m) && m.id == id;
}
bool Mail::read(const std::string &id, MailMessage &m) {
  auto a = account();
  std::lock_guard<std::mutex> g(mutex_);
  if (!readStored(id, m))
    return false;
  if (m.account != identity(a)) {
    m = {};
    return false;
  }
  if (!m.seen && (m.folder == "inbox" || m.folder.rfind("remote:", 0) == 0)) {
    m.seen = true;
    // Offline reads remain usable when the SD cannot persist a seen flag.
    if (!put(m))m.seen=false;
  }
  return true;
}
bool Mail::draft(MailMessage &m, bool queue, std::string &error) {
  auto a = account();
  if (!mail::address(a.address)) {
    error = "Configure primeiro o endereço da conta";
    return false;
  }
  if (m.body.size() > 16 * 1024 || m.subject.size() > 200 ||
      m.subject.find_first_of("\r\n") != m.subject.npos || m.to.size() > 1024 ||
      m.to.find_first_of("\r\n") != m.to.npos) {
    error = "Assunto até 200 bytes e corpo até 16 KiB; cabeçalhos sem quebras "
            "de linha";
    return false;
  }
  if (queue) {
    TextLines in(m.to);
    std::string to;
    unsigned count = 0;
    while (nextLine(in, to, ',')) {
      auto first = to.find_first_not_of(' '), last = to.find_last_not_of(' ');
      if (first == to.npos ||
          !mail::address(to.substr(first, last - first + 1)) || ++count > 10) {
        error = "Confira os destinatários (até dez separados por vírgula)";
        return false;
      }
    }
    if (!count || m.body.empty()) {
      error = "Informe destinatário e mensagem";
      return false;
    }
  }
  std::lock_guard<std::mutex> g(mutex_);
  if (queue && busy_) {
    error = "Aguarde a operação de e-mail antes de enviar o rascunho";
    return false;
  }
  if (!load()) {
    error = "Cartão indisponível";
    return false;
  }
  if (m.id.empty()) {
    if (index_.size() >= mail::maxMessages && !index_.count(m.id)) {
      error = "Cache cheio (100 mensagens). Remova mensagens locais.";
      return false;
    }
    std::string counter;
    disk_.read("/data/mail/counter", counter, 32);
    auto next = strtoull(counter.c_str(), nullptr, 10) + 1;
    if (!disk_.write("/data/mail/counter", std::to_string(next))) {
      error = "Falha ao reservar mensagem";
      return false;
    }
    m.id = sha256(identity(a) + std::to_string(next)).substr(0, 24);
    m.messageId = "<" + m.id + "@tab5.local>";
  } else {
    auto found = index_.find(m.id);
    if (found == index_.end() || found->second.folder != "drafts" ||
        found->second.account != identity(a)) {
      error = "Rascunho inválido";
      return false;
    }
    MailMessage old;
    std::string raw;
    if (disk_.read(root + m.id, raw, 2*1024*1024) && unpack(raw, old))
      {m.messageId = old.messageId;m.attachments=old.attachments;}
  }
  m.account = identity(a);
  m.seen = true;
  m.from = a.address;
  m.folder = queue ? "outbox" : "drafts";
  m.status = queue ? "queued" : "draft";
  m.date = std::to_string(time(nullptr));
  if (!put(m)) {
    error = "Não foi possível salvar a mensagem no SD";
    return false;
  }
  message_ = queue ? "Mensagem na caixa de saída" : "Rascunho salvo";
  return true;
}
bool Mail::move(const std::string &id, const std::string &folder,
                std::string &error) {
  MailMessage m;
  if (!read(id, m)) {
    error = "Mensagem não encontrada";
    return false;
  }
  if (folder != "trash" && folder != "outbox" && folder != "delete" &&
      folder != "drafts" && folder!="inbox") {
    error = "Destino inválido";
    return false;
  }
  std::lock_guard<std::mutex> g(mutex_);
  if (busy_) {
    error = "Aguarde a operação de e-mail";
    return false;
  }
  if((folder=="trash"||folder=="delete") && !m.uid.empty() && !deleted(m.account,m.uid,true)){error="Falha ao registrar exclusão local";return false;}
  if (folder == "delete") {
    if (m.folder != "trash") {
      error = "Remova primeiro para a lixeira local";
      return false;
    }
    if (!disk_.remove(root + id))
      return false;
    index_.erase(id);disk_.removeTree("/data/mail/attachments/"+id);
    return true;
  }
  if (folder == "outbox" && m.status != "failed" && m.status != "uncertain") {
    error = "Mensagem não está em falha";
    return false;
  }
  if(folder=="inbox" && !m.uid.empty() && !deleted(m.account,m.uid,false)){error="Falha ao restaurar exclusão local";return false;}
  std::string restored=folder;
  if(folder=="inbox" && m.uid.rfind("imap:",0)==0){auto last=m.uid.rfind(':');auto previous=last==m.uid.npos?m.uid.npos:m.uid.rfind(':',last-1);if(previous!=m.uid.npos&&previous>5){auto original=m.uid.substr(5,previous-5);restored=original=="INBOX"?"inbox":"remote:"+original;}}
  m.folder = restored;
  m.status = folder == "inbox" ? "cached" : folder == "outbox"   ? "queued"
             : folder == "drafts" ? "draft"
                                  : "local";
  return put(m);
}
void Mail::sendQueue(const MailAccount &a,bool retryRejected) {
  std::vector<std::string> ids;
  {
    std::lock_guard<std::mutex> g(mutex_);
    load();
    for (auto &v : index_)
      if (v.second.folder == "outbox" && (v.second.status == "queued" || (retryRejected && v.second.status=="failed")) &&
          v.second.account == identity(a))
        ids.push_back(v.first);
  }
#ifdef ESP_PLATFORM
  ESP_LOGW("mail", "manual_retry=%u selected=%u", unsigned(retryRejected), unsigned(ids.size()));
#endif
  unsigned sent = 0;
  for (auto &id : ids) {
    if (cancel_)
      break;
    MailMessage m;
    if (!read(id, m))
      continue;
    {
      std::lock_guard<std::mutex> g(mutex_);
      m.status = "sending";
      if (!put(m)) {
        message_ = "Falha ao registrar início do envio";
        busy_ = false;
        completed_ = true;
        return;
      }
    }
    auto socket = wire ? wire() : nullptr;
    if (socket)
      socket->progress = [this](const std::string &s) { report(s); };
    std::string error;
    auto result = socket ? mail::send(*socket, a, m, cancel_, error,[this,id=m.id](const std::string &path,uint64_t offset,size_t n,std::string &out){return path.rfind("/data/mail/attachments/"+id+"/",0)==0 && disk_.readRange(path,offset,n,out);})
                         : mail::SendResult::rejected;
    {
      std::lock_guard<std::mutex> g(mutex_);
      if (result == mail::SendResult::accepted) {
        m.status = "accepted";
        m.folder = "sent";
        ++sent;
      } else
        m.status =
            result == mail::SendResult::uncertain ? "uncertain" : "failed";
      if (!put(m)) {
        message_ = "Servidor pode ter aceitado; falha ao gravar estado no SD";
        busy_ = false;
        completed_ = true;
        return;
      }
    }
#ifdef ESP_PLATFORM
    int smtpCode=0;
    if(error.rfind("SMTP 535:",0)==0)smtpCode=535;
    else if(error.rfind("SMTP recusou",0)==0){auto at=error.rfind('(');if(at!=error.npos)smtpCode=atoi(error.c_str()+at+1);}
    ESP_LOGW("mail", "queue_result=%s smtp_code=%d", result==mail::SendResult::accepted?"accepted":result==mail::SendResult::uncertain?"uncertain":"rejected", smtpCode);
#endif
    if (result != mail::SendResult::accepted) {
      finish(error.empty() ? "Transporte de e-mail indisponível" : error);
      return;
    }
  }
  finish(cancel_ ? "Operação cancelada"
                 : std::to_string(sent) +
                       " mensagem(ns) aceita(s) pelo servidor SMTP");
}
bool Mail::flush(std::string &error,bool retryRejected) {
  auto a = account();
  if (!mail::validAccount(a, error))
    return false;
  if (!wire) {
    error = "Transporte TLS de e-mail indisponível";
    return false;
  }
  if (!start([this, a,retryRejected] { sendQueue(a,retryRejected); })) {
    error = "E-mail ocupado";
    return false;
  }
  return true;
}
bool Mail::synchronize(const std::string &folder, std::string &error, const std::string &target) {
  {std::lock_guard<std::mutex> lock(mutex_);load();if(!loadDeletions()){error="Falha ao ler exclusões locais; sincronização cancelada";return false;}}
  auto a = account();
  if (!mail::validAccount(a, error))
    return false;
  if (!wire) {
    error = "Transporte TLS de e-mail indisponível";
    return false;
  }
  if (!start([this, a, folder, target] {
        std::string error;
        std::vector<std::string> folders;
        unsigned count = 0;
        auto known = [&](const std::string &uid) {
          if(!target.empty())return uid!=target;
          std::lock_guard<std::mutex> g(mutex_);
          if(isDeleted(identity(a),uid))return true;
          load();
          for (auto &v : index_)
            if (v.second.account == identity(a) && v.second.uid == uid)
              return true;
          return false;
        };
        auto deliver = [&](const std::string &uid, const std::string &raw) {
          const auto messageId=sha256(identity(a)+uid).substr(0,24);
          MailMessage previous;std::string stored;
          if(disk_.read(root+messageId,stored,2*1024*1024))unpack(stored,previous);
          std::string attachmentError;unsigned attachmentParts=0;
          auto m = mail::decodeMessage(raw,[&](const auto &name,const auto &type,const auto &encoding,const auto &body,MailAttachment &attachment){if(++attachmentParts>8){attachmentError="Limite de oito anexos por mensagem";return false;}return saveAttachment(messageId,name,type,encoding,body,attachment,attachmentError);});
          auto cleanupNew=[&]{for(const auto &attachment:m.attachments){bool retained=false;for(const auto &old:previous.attachments)retained|=old.path==attachment.path;if(!retained)disk_.remove(attachment.path);}};
          if(!attachmentError.empty()){cleanupNew();error=attachmentError;return false;}
          m.id = sha256(identity(a) + uid).substr(0, 24);
          m.uid = uid;
          m.account = identity(a);
          m.folder = folder == "INBOX" || folder.empty() ? "inbox"
                                                         : "remote:" + folder;
          m.status = raw.rfind("X-Tab5-Header-Only: 1\r\n", 0) == 0 ? "headers"
                                                                    : "cached";
          std::lock_guard<std::mutex> g(mutex_);
          load();
          if (index_.size() >= mail::maxMessages && !index_.count(m.id)) {
            cleanupNew();error = "Cache cheio (100 mensagens); remova mensagens locais";
            return false;
          }
          auto existing=index_.find(m.id);m.seen=previous.seen || (existing!=index_.end()&&existing->second.seen);
          if (!put(m)) {
            cleanupNew();error = "Falha ao salvar mensagem no SD";
            return false;
          }
          for(const auto &old:previous.attachments){bool retained=false;for(const auto &attachment:m.attachments)retained|=old.path==attachment.path;if(!retained&&old.path.rfind("/data/mail/attachments/"+messageId+"/",0)==0)disk_.remove(old.path);}
          message_ = "Baixadas: " + std::to_string(++count);
          return true;
        };
        bool ok = false, failedTransport = false;
        for (unsigned attempt = 0; attempt < 2 && !cancel_; ++attempt) {
          auto socket = wire();
          if (!socket) {
            error = "Transporte indisponível";
            break;
          }
          socket->progress = [this, attempt](const std::string &s) {
            report("Tentativa " + std::to_string(attempt + 1) + "/2\n" + s);
          };
          error.clear();
          ok = mail::receive(*socket, a, folder.empty() ? "INBOX" : folder,
                             known, deliver, folders, cancel_, error, target);
          failedTransport = !ok && socket->retryableConnectFailure();
          bool retry = !ok && !cancel_ && attempt == 0 &&
                       socket->retryableConnectFailure();
          socket.reset();
          if (!retry)
            break;
          report("Timeout antes da autenticação; refazendo conexão uma vez");
          workerWait(std::chrono::milliseconds(200));
        }
        if (failedTransport && !cancel_ && recoverTransport &&
            recoverTransport())
          error += "\nRecuperação do Wi-Fi agendada; aguarde e tente Receber "
                   "novamente.";
        if (cancel_)
          error = "Operação cancelada";
        if (ok) {
          std::lock_guard<std::mutex> g(mutex_);
          folders_ = folders;
          foldersAccount_ = identity(a);
          auto saved = folders;
          saved.insert(saved.begin(), "account:" + foldersAccount_);
          disk_.write("/data/mail/folders", record(saved));
        }
        finish(
            ok ? std::to_string(count) +
                     " mensagem(ns) sincronizada(s); cache disponível offline"
            : error.empty() ? "Falha de recebimento"
                            : error);
      })) {
    error = "E-mail ocupado";
    return false;
  }
  return true;
}
void Mail::poll(uint64_t now, bool online) {
  auto a = account();
  if (!a.automatic || !online || now < nextAuto_)
    return;
  nextAuto_ = now + 30000;
  auto s = status();
  if (!s.busy && s.queued) {
    std::string error;
    flush(error);
  }
}
} // namespace tab5

namespace tab5 {
bool Mail::refresh(const std::string &id,std::string &error){
  MailMessage m;if(!read(id,m)||m.uid.empty()||(m.folder!="inbox"&&m.folder.rfind("remote:",0)!=0)){error="Somente mensagens recebidas podem ser baixadas novamente";return false;}
  return synchronize(m.folder=="inbox"?"INBOX":m.folder.substr(7),error,m.uid);
}
}
