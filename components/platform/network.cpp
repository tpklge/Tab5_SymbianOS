#include "html_allocation_scope.hpp"
// SPDX-License-Identifier: MIT
#include "network.hpp"
#include "browser_profile.hpp"
#include "browser_document.hpp"
#include "browser_web.hpp"
#include "browser_cookies.hpp"
#include <ctime>
#include "mail_document.hpp"
#include "http_fetch.hpp"
#include <set>
#include <algorithm>
#include <cctype>
#include <new>
namespace tab5 {
namespace {
struct TransferGuard {
  std::atomic<unsigned> *active = nullptr;
  TransferGuard(std::atomic<unsigned> &count,
                const std::atomic<bool> &recovering) {
    if (recovering)
      return;
    ++count;
    if (recovering) {
      --count;
      return;
    }
    active = &count;
  }
  explicit operator bool() const { return active != nullptr; }
  ~TransferGuard() {
    if (active)
      --*active;
  }
};
} // namespace
Network::~Network() {
  cancel_ = true;
  if (worker_)
    worker_->join();
  if (backend.share) {
    std::string a, t;
    backend.share(false, a, t);
  }
}
Network::Status Network::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto value = status_;
  value.restartRequired = backend.restartRequired && backend.restartRequired();
  if (value.restartRequired) {
    value.connected = value.sharing = value.signalValid = false;
    value.address.clear(); value.transferAddress.clear(); value.token.clear();
    value.message = "Transporte P4/C6 sem resposta. Salve os documentos e reinicie o Tab5.";
  }
  if (backend.linked && !backend.linked()) {
    value.connected = value.sharing = false;
    value.token.clear();value.transferAddress.clear();
    value.signalValid = false;
    if (!value.busy && (value.message.rfind("Conectado", 0) == 0 ||
                        value.message.rfind("Wi-Fi reconectado", 0) == 0))
      value.message = "Conexão perdida";
  }
  return value;
}
bool Network::start(std::function<void()> job, bool recovery) {
  if (completed_.exchange(false) && worker_)
    worker_->join();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_.busy || (backend.restartRequired && backend.restartRequired()) || (recovering_ && !recovery))
      return false;
    status_.busy = true;
    status_.bytes = 0;
    status_.message = "Aguarde…";
  }
  cancel_ = false;
  if (!worker_)
    worker_ = std::make_unique<Executor>(32768, 2,true);
  if (!worker_->submit([this, job = std::move(job)] {
        HtmlAllocationScope allocation;
        try {job();}
        catch(const std::bad_alloc &){finish(false,"Memória insuficiente para a operação de rede");}
        catch(const std::exception &){finish(false,"Falha interna na operação de rede");}
        completed_ = true;
        std::lock_guard<std::mutex> guard(mutex_);
        status_.busy = false;
      })) {
    {
      std::lock_guard<std::mutex> guard(mutex_);
      status_.busy = false;
    }
    finish(false, "Worker de rede indisponível");
    return false;
  }
  return true;
}
void Network::finish(bool ok, const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_.message = cancel_ ? "Cancelado" : message;
  (void)ok;
}
bool Network::connect(const std::string &ssid, const std::string &pass,
                      bool save) {
  if (ssid.empty() || ssid.size() > 32 || pass.size() > 63 ||
      ssid.find('\0') != std::string::npos ||
      ssid.find_first_of("\r\n") != std::string::npos ||
      pass.find('\0') != std::string::npos)
    return false;
  if (automaticJob_) {
    cancel_ = true;
    if (worker_)
      worker_->join();
    completed_ = false;
    automaticJob_ = false;
  }
  autoSuppressed_ = false;
  return start([this, ssid, pass, save] {
    std::string address;
    bool ok = backend.connect &&
              backend.connect(ssid, pass, address, cancel_) && !cancel_;
    bool persisted = !save || !ok || remember(ssid, pass);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      status_.connected = ok;
      status_.signalValid = false;
      status_.address = ok ? address : "";
      status_.ssid = ok ? ssid : "";
      if (ok) {
        lastSsid_ = ssid;
        lastPassword_ = pass;
      }
    }
    finish(ok,
           ok                   ? (persisted ? "Conectado"
                                             : "Conectado; não foi possível salvar a rede no SD")
           : backend.diagnostic ? backend.diagnostic()
                                : "Falha de conexão Wi-Fi");
  });
}
bool Network::recover(bool automatic) {
  std::string ssid, password;
  bool sharing = false;
  const uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
  {
    std::lock_guard<std::mutex> guard(mutex_);
    if ((backend.restartRequired && backend.restartRequired()) || status_.busy || recovering_ || lastSsid_.empty() ||
        (automatic && (autoSuppressed_ || activeTransfers_ ||
                       (lastRecoveryMs_ && now - lastRecoveryMs_ < 60000))))
      return false;
    recovering_ = true;
    if (automatic && activeTransfers_) {
      recovering_ = false;
      return false;
    }
    ssid = lastSsid_;
    password = lastPassword_;
    sharing = wantedSharing_;
  }
  if (!automatic)
    autoSuppressed_ = false;
  bool accepted = start(
      [this, ssid, password, sharing] {
        {
          std::lock_guard<std::mutex> guard(mutex_);
          status_.connected = false;
          status_.signalValid = false;
          status_.message = "Recuperando conexão Wi-Fi";
        }
        std::string recoveryError;
        bool prepared = true;
        if (backend.recover)
          prepared = backend.recover(cancel_, recoveryError);
        else if (backend.disconnect)
          backend.disconnect();
        if (sharing && backend.share) {
          std::string unusedAddress, unusedToken;
          backend.share(false, unusedAddress, unusedToken);
        }
        std::string address;
        bool ok = prepared && !cancel_ && backend.connect &&
                  backend.connect(ssid, password, address, cancel_) && !cancel_;
        std::string shareAddress, token;
        bool restored = ok && sharing && backend.share &&
                        backend.share(true, shareAddress, token);
        {
          std::lock_guard<std::mutex> guard(mutex_);
          status_.connected = ok;
          status_.address = ok ? address : "";
          status_.ssid = ok ? ssid : "";
          if (sharing) {
            status_.sharing = restored;
            status_.token = restored ? token : "";
            status_.transferAddress = restored ? shareAddress : "";
          }
        }
        const std::string result =
            ok ? "Wi-Fi reconectado; tente a operação novamente"
            : !recoveryError.empty() ? recoveryError
            : backend.diagnostic     ? backend.diagnostic()
                                     : "Falha ao recuperar Wi-Fi";
        disk_.mkdir("/logs");
        disk_.write("/logs/network-recovery.txt",
                    "runtime=1.0.0\nresult=" +
                        std::string(ok ? "connected" : "failed") + "\nfile_transfer=" +
                        (restored ? "restored" : "off") + "\n" + result + "\n");
        if (!ok)
          autoSuppressed_ = true;
        recovering_ = false;
        finish(ok, result);
      },
      true);
  if (accepted) {
    std::lock_guard<std::mutex> guard(mutex_);
    lastRecoveryMs_ = now;
  } else
    recovering_ = false;
  return accepted;
}
bool Network::scan() {
  return start([this] {
    std::vector<AccessPoint> found, result;
    bool ok = backend.scan && backend.scan(found, cancel_) && !cancel_;
    if (ok) {
      std::sort(found.begin(), found.end(),
                [](const AccessPoint &a, const AccessPoint &b) {
                  return a.signal > b.signal;
                });
      for (auto &ap : found) {
        if (ap.ssid.empty() || ap.ssid.size() > 32 ||
            ap.ssid.find_first_of("\r\n") != std::string::npos ||
            ap.ssid.find('\0') != std::string::npos)
          continue;
        bool duplicate = false;
        for (auto &old : result)
          if (old.ssid == ap.ssid)
            duplicate = true;
        if (!duplicate)
          result.push_back(ap);
        if (result.size() == 32)
          break;
      }
      std::lock_guard<std::mutex> lock(mutex_);
      status_.networks = std::move(result);
    }
    finish(ok, ok                   ? "Busca concluída: redes de 2,4 GHz"
               : backend.diagnostic ? backend.diagnostic()
                                    : "Falha ao buscar redes");
  });
}
bool Network::share(bool on) {
  return start([this, on] {
    std::string address, token;
    bool ok = backend.share && backend.share(on, address, token);
    if (!on) {
      std::lock_guard<std::mutex> transfer(transferMutex_);
      uploadId_.clear();
      disk_.remove("/tmp/web-upload.part");
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (ok || !on)
        wantedSharing_ = on;
      status_.sharing = ok && on;
      status_.token = status_.sharing ? token : "";
      status_.transferAddress = status_.sharing ? address : "";
    }
    const auto diagnostic=backend.shareDiagnostic?backend.shareDiagnostic():std::string();
    finish(ok,
           ok ? (on ? "Transferência local ativa" : "Transferência encerrada")
              : !diagnostic.empty() ? diagnostic
              : status().connected ? "Não foi possível iniciar o envio web; confira o diagnóstico de rede"
                                   : "Conecte o Wi-Fi antes de iniciar o envio web");
  });
}
void Network::disconnect() {
  autoSuppressed_ = true;
  cancel_ = true;
  if (worker_) {
    worker_->join();
    completed_ = false;
  }
  if (backend.share) {
    std::string a, t;
    backend.share(false, a, t);
  }
  if (backend.disconnect)
    backend.disconnect();
  {
    std::lock_guard<std::mutex> transfer(transferMutex_);
    uploadId_.clear();
    uploadPath_.clear();
    uploadHash_.clear();
    uploadOffset_=uploadTotal_=uploadLastOffset_=0;
    uploadComplete_=false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = Status{};
  wantedSharing_ = false;
}
bool Network::validUrl(const std::string &url) {
  if (url.size() > 1024 ||
      (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0))
    return false;
  for (unsigned char c : url)
    if (c <= 32 || c == 127 || c == '\\')
      return false;
  auto begin = url.find("://") + 3;
  auto end = url.find_first_of("/?#", begin);
  auto authority = url.substr(begin, end - begin);
  return !authority.empty() && authority.find('@') == std::string::npos &&
         url.find('#') == std::string::npos;
}
bool Network::sharedPath(const std::string &p) {
  return Storage::validPath(p) && p.rfind("/home/Transfer/", 0) == 0 &&
         p.size() > 15 && p.find('/', 15) == std::string::npos &&
         p.find(".part") == std::string::npos &&
         p.find('%') == std::string::npos;
}
// One atomic, URL-bound cache record per page; failed transfers preserve old pages.
bool Network::browse(const std::string &url,const std::string &method,const std::string &body,const std::string &initiator) {
  if((method!="GET"&&method!="POST")||body.size()>16384||(!initiator.empty()&&!validUrl(initiator)))return false;
  if(method=="POST"&&(!backend.browserFetch||BrowserCookies::origin(url)!=BrowserCookies::origin(initiator)))return false;
  if(!validUrl(url) || url.size()>1024) return false;
  const auto generation=disk_.generation();
  PageStatus previous;uint64_t serial,previousSerial;
  {std::lock_guard<std::mutex> lock(mutex_);previous=page_;previousSerial=pageSerial_;serial=++pageSerial_;page_={true,false,0,url,"Aguardando resposta da página"};}
  bool accepted=start([this,url,generation,serial,method,body,initiator] {
    auto done=[&](bool ok,const std::string &message){
      const auto result=cancel_?std::string("Cancelado"):message.empty()?std::string("Não foi possível receber a página"):message;
      uint64_t bytes=0;
      {std::lock_guard<std::mutex> lock(mutex_);status_.message=result;if(pageSerial_==serial){page_.ok=ok&&!cancel_;page_.busy=false;page_.message=result;bytes=page_.bytes;}}
      if(!ok){try{disk_.write("/logs/browser-network.txt","runtime=1.0.0\nbuild="+std::string(browserProfile::build)+"\nstage=receive\nbytes="+std::to_string(bytes)+"\nerror="+result.substr(0,2048)+"\n");}catch(...){}}
    };
    try {
    TransferGuard active(activeTransfers_,recovering_);
    std::unique_lock<std::mutex> transfer(transferMutex_,std::try_to_lock);
    if(!active || !transfer.owns_lock()) {done(false,"Outra transferência está ativa");return;}
    BrowserCookies cookies;std::string savedCookies;const auto now=int64_t(std::time(nullptr));if(disk_.read("/data/browser/cookies.tsv",savedCookies,100000)&&!cookies.load(savedCookies,now))cookies=BrowserCookies{};
    BrowserRequest request;request.method=method;request.body=body;request.initiator=initiator;
    request.cookies=[&](const std::string &target,bool safe){return cookies.header(target,now,false,initiator,safe);};
    request.setCookie=[&](const std::string &target,const std::string &value){cookies.set(target,value,now);};
    BrowserDocument document;BrowserWeb web;size_t received=0;std::string error,resolved=url,detail;
    auto sink=[&](const char *data,size_t n) {
      if(cancel_)return false;
      if(n>16384 || n>browserProfile::wireLimit-received){detail="Página excede 4 MiB de recebimento";return false;}
      document.feed(data,n);web.feed(data,n);received+=n;
      if(received%16384<n)workerWait(std::chrono::milliseconds(1));
      std::lock_guard<std::mutex> lock(mutex_);status_.bytes=received;status_.message="Recebendo página";if(pageSerial_==serial){page_.bytes=received;page_.message=status_.message;}return true;
    };
    bool ok=false;
    for(unsigned attempt=0;attempt<2;++attempt) {
      request.legacyTls=attempt>0;
      document=BrowserDocument{};web=BrowserWeb{};received=0;resolved=url;error.clear();
      {std::lock_guard<std::mutex> lock(mutex_);status_.bytes=0;status_.message=attempt?"Repetindo resposta interrompida":"Conectando; aguardando cabeçalhos HTTP";if(pageSerial_==serial){page_.bytes=0;page_.message=status_.message;}}
      ok=backend.browserFetch?backend.browserFetch(url,request,sink,cancel_,error,resolved):backend.fetchDocument?backend.fetchDocument(url,sink,cancel_,error,resolved):backend.fetch&&backend.fetch(url,sink,cancel_,error);
      if(method=="POST"||ok||cancel_||!detail.empty()||disk_.generation()!=generation||error.find("HTTP/TLS interrompida")==error.npos)break;
      workerWait(std::chrono::milliseconds(250));
    }
    if(cancel_ || disk_.generation()!=generation) {done(false,"Cancelado ou cartão substituído");return;}
    if(ok && validUrl(resolved) && resolved.size()<=1024 && received>0) {
      disk_.mkdir("/data/browser");disk_.mkdir("/data/browser/cache");
      const auto path=pageCachePath(url);
      auto data=document.finish();data.markup=web.finish();
      auto executed=browserScript(data.markup,web.script(),resolved,cookies.header(resolved,now,true),[this]{return bool(cancel_);});
      data.markup=std::move(executed.html);data.scriptError=std::move(executed.error);
      for(size_t at=0;at<executed.cookies.size();){auto end=executed.cookies.find('\n',at);cookies.set(resolved,executed.cookies.substr(at,end==std::string::npos?end:end-at),now,true);if(end==std::string::npos)break;at=end+1;}
      data.truncated=data.truncated||web.clipped();
      if(cancel_||disk_.generation()!=generation){done(false,"Cancelado ou cartão substituído");return;}
      ok=disk_.write(path,BrowserDocument::record(resolved,data));
      std::vector<File> files;
      // Publish first: a failed write must preserve all prior cache entries.
      if(ok && disk_.listDir("/data/browser/cache",files) && files.size()>12) {
        for(const auto &f:files)if(f.path!=path && f.path.size()>5 && f.path.substr(f.path.size()-5)==".page") {disk_.remove(f.path);break;}
      }
      if(!ok)error=disk_.lastError();
    } else if(ok) {ok=false;error="Página vazia ou endereço final inválido";}
    if(disk_.generation()==generation){disk_.mkdir("/data/browser");disk_.write("/data/browser/cookies.tsv",cookies.save());}
    done(ok,ok?"Página salva no cartão":!detail.empty()?detail:error);
    }catch(const std::bad_alloc &){done(false,"Memória insuficiente para carregar a página");}
     catch(const std::exception &){done(false,"Falha interna ao carregar a página");}
  });
  if(!accepted){std::lock_guard<std::mutex> lock(mutex_);if(pageSerial_==serial){page_=std::move(previous);pageSerial_=previousSerial;}}
  return accepted;
}
bool Network::download(const std::string &url, const std::string &path,
                       bool feed) {
  if (!validUrl(url) || !Storage::validPath(path) ||
      (feed ? path != "/data/feeds/latest.xml" : !sharedPath(path)))
    return false;
  const auto generation=disk_.generation();
  return start([this, url, path, feed,generation] {
    TransferGuard active(activeTransfers_, recovering_);
    if (!active) {
      finish(false, "Rede em recuperação");
      return;
    }
    std::unique_lock<std::mutex> transfer(transferMutex_, std::try_to_lock);
    if (!transfer.owns_lock()) {
      finish(false, "Outra transferência está ativa");
      return;
    }
    auto partial = path + ".part";
    std::string empty, error;
    std::vector<File> entries;
    auto slash = path.find_last_of('/');
    if (!disk_.listDir(path.substr(0, slash), entries)) {
      finish(false, disk_.lastError());
      return;
    }
    for (auto &f : entries)
      if (f.path == path && !feed) {
        finish(false, "O arquivo de destino já existe");
        return;
      }
    if (!feed && !disk_.write(partial, empty)) {
      finish(false, disk_.lastError());
      return;
    }
    uint64_t count = 0, limit = feed ? 65536 : maxUploadBytes;
    std::string sinkError, feedData;
    if (feed)
      feedData.reserve(size_t(limit));
    {std::lock_guard<std::mutex> lock(mutex_);status_.message="Conectando; aguardando cabeçalhos HTTP";}
    auto sink = [&](const char *data, size_t n) {
      if (cancel_||disk_.generation()!=generation)
        return false;
      if (n > 16384 || count + n > limit) {
        sinkError = feed ? "Feed excede o limite de 64 KiB."
                         : "Download excede o limite de 64 MiB.";
        return false;
      }
      if (feed)
        feedData.append(data, n);
      else if (!disk_.append(partial, std::string(data, n))) {
        sinkError = "Falha ao salvar no SD: " + disk_.lastError();
        return false;
      }
      count += n;
      std::lock_guard<std::mutex> lock(mutex_);
      status_.bytes = count;status_.message="Recebendo resposta HTTP";
      return true;
    };
    bool ok =
        backend.fetch && backend.fetch(url, sink, cancel_, error) && !cancel_;
    if (feed && !ok && !cancel_ && sinkError.empty() &&
        error.find("HTTP/TLS interrompida") != std::string::npos) {
      // Idempotent feed GET only: restart the bounded buffer, never concatenate
      // a truncated prefix with the next attempt, and never replace cache
      // early.
      feedData.clear();
      count = 0;
      error.clear();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.bytes = 0;
      }
      workerWait(std::chrono::milliseconds(250));
      ok = backend.fetch && !cancel_ &&
           backend.fetch(url, sink, cancel_, error) && !cancel_;
    }
    if(disk_.generation()!=generation){finish(false,"Cartão alterado durante o download");return;}
    if (!ok && !sinkError.empty())
      error = sinkError;
    if (ok && feed) {
      const auto &text = feedData;
      ok = !text.empty() && text.find("<!DOCTYPE") == std::string::npos &&
           text.find("<!ENTITY") == std::string::npos &&
           ((text.find("<rss") != std::string::npos &&
             text.find("</rss>") != std::string::npos) ||
            (text.find("<feed") != std::string::npos &&
             text.find("</feed>") != std::string::npos));
      if (ok)
        ok = disk_.write(path, text);
      if (!ok)
        error = "Resposta não é um feed RSS/Atom válido ou não pôde ser salva.";
    } else if (ok)
      ok = disk_.move(partial, path);
    if (!ok || feed)
      disk_.remove(partial);
    finish(ok,
           ok ? (feed ? "Feed atualizado" : "Download concluído")
              : (error.empty() ? "Transferência falhou ou excedeu espaço/limite"
                               : error));
  });
}
bool Network::receive(
    const std::string &path, const std::function<int(char *, size_t)> &read,
    size_t length, const std::function<void(const char *, size_t)> &progress) {
  TransferGuard active(activeTransfers_, recovering_);
  if (!active)
    return false;
  std::unique_lock<std::mutex> transfer(transferMutex_, std::try_to_lock);
  auto report = [&](const char *stage, size_t n) {
    if (progress)
      progress(stage, n);
  };
  if (!transfer.owns_lock())
    return false;
  report("checking", 0);
  if (!sharedPath(path) || !length || length > maxUploadBytes ||
      !disk_.canFit(length)) {
    report("rejected", 0);
    return false;
  }
  std::vector<File> files;
  if (!disk_.listDir("/home/Transfer", files)) {
    report("directory-failed", 0);
    return false;
  }
  for (auto &f : files)
    if (f.path == path) {
      report("destination-exists", 0);
      return false;
    }
  // HTTP handlers must leave stack space for the network and FAT call chains.
  constexpr size_t blockSize = 65536;
  std::unique_ptr<char[]> buffer(new (std::nothrow) char[blockSize]);
  if (!buffer) {
    report("buffer-unavailable", 0);
    return false;
  }
  auto partial = path + ".part";
  if (!disk_.write(partial, "")) {
    report("create-failed", 0);
    return false;
  }
  size_t received = 0, reported = 0;
  bool ok = true;
  const char *failure = "receive-failed";
  report("receiving", 0);
  while (received < length) {
    size_t requested = std::min(blockSize, length - received);
    size_t filled = 0;
    while (filled < requested) {
      int n = read(buffer.get() + filled, requested - filled);
      if (n <= 0 || size_t(n) > requested - filled)
        break;
      filled += size_t(n);
    }
    if (filled != requested) {
      ok = false;
      break;
    }
    if (!disk_.append(partial, std::string(buffer.get(), filled))) {
      failure = "write-failed";
      ok = false;
      break;
    }
    received += filled;
    if (received - reported >= 1024 * 1024) {
      report("receiving", received);
      reported = received;
    }
  }
  if (ok) {
    report("committing", received);
    ok = disk_.move(partial, path);
    failure = "commit-failed";
  }
  if (!ok)
    disk_.remove(partial);
  report(ok ? "complete" : failure, received);
  return ok;
}
bool Network::removeShared(const std::string &path,const std::string &session,std::string &error) {
  TransferGuard active(activeTransfers_,recovering_);
  if(!active||!sharedPath(path)){error="Arquivo indisponível";return false;}
  const auto generation=disk_.generation();
  std::lock_guard<std::mutex> lock(transferMutex_);
  if(!session.empty()) {
    if(session!=uploadId_||path!=uploadPath_){error="Sessão não encontrada";return false;}
    if(uploadGeneration_!=disk_.generation()){error="Cartão alterado";return false;}
    if(!uploadComplete_&&!disk_.removeAt("/tmp/web-upload.part",generation)){error=disk_.lastError();return false;}
    uploadId_.clear();uploadHash_.clear();return true;
  }
  uint64_t size=0;
  if((path==uploadPath_&&!uploadId_.empty()&&!uploadComplete_)||!disk_.fileSize(path,size)||!disk_.removeAt(path,generation)){error="Não foi possível apagar o arquivo";return false;}
  return true;
}
bool Network::send(const std::string &path, const Sink &sink) {
  TransferGuard active(activeTransfers_, recovering_);
  if (!active)
    return false;
  if (!sharedPath(path))
    return false;
  std::lock_guard<std::mutex> transfer(transferMutex_);
  const auto generation=disk_.generation();
  uint64_t length=0,offset=0;
  if(!disk_.fileSize(path,length))return false;
  for (;;) {
    if(disk_.generation()!=generation)return false;
    if(offset==length)return true;
    std::string data;
    if (!disk_.readRange(path, offset, std::min(uint64_t(16384),length-offset), data))
      return false;
    if (data.empty())
      return false;
    if (!sink(data.data(), data.size()))
      return false;
    offset += data.size();
  }
}
bool Network::receivePart(const std::string &id, const std::string &path,
                          size_t offset, size_t total, size_t length,
                          const std::function<int(char *, size_t)> &reader,
                          std::string &error) {
  TransferGuard active(activeTransfers_, recovering_);
  if (!active) {
    error = "Rede em recuperação; aguarde e tente novamente";
    return false;
  }
  std::lock_guard<std::mutex> lock(transferMutex_);
  auto fail = [&](const std::string &why) {
    error = why;
    return false;
  };
  if (id.size() != 16 ||
      id.find_first_not_of("0123456789abcdef") != std::string::npos ||
      !sharedPath(path) || !total || total > maxUploadBytes || !length ||
      length > 65536 || offset > total || length > total - offset)
    return fail(
        "Envio inválido: máximo 64 MiB por arquivo e 64 KiB por bloco.");
  const auto now =
      uint64_t(std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count());
  const std::string partial = "/tmp/web-upload.part";
  if (!uploadId_.empty() && uploadGeneration_ != disk_.generation()) {
    uploadId_.clear();
    uploadHash_.clear();
    if (offset)
      return fail("Cartão alterado. Reinicie o envio do arquivo.");
  }
  if (uploadId_ != id) {
    if (!uploadId_.empty() && !uploadComplete_ && now - uploadTime_ < 600)
      return fail("Outro envio está ativo. Aguarde ou encerre o serviço.");
    if (offset)
      return fail("Sessão perdida. Reinicie o envio do arquivo.");
    std::vector<File> files;
    if (!disk_.listDir("/home/Transfer", files))
      return fail("SD indisponível.");
    for (const auto &f : files)
      if (f.path == path)
        return fail("Arquivo já existe. Altere o nome antes de enviar.");
    if (!disk_.canFit(total) || !disk_.write(partial, ""))
      return fail("Sem espaço disponível no SD para este arquivo.");
    uploadId_ = id;
    uploadPath_ = path;
    uploadTotal_ = total;
    uploadOffset_ = 0;
    uploadHash_.clear();
    uploadComplete_ = false;
    uploadGeneration_ = disk_.generation();
  }
  if (path != uploadPath_ || total != uploadTotal_)
    return fail("Sessão não corresponde ao arquivo.");
  uploadTime_ = now;
  const bool repeated = !uploadHash_.empty() && offset == uploadLastOffset_;
  if ((!repeated && offset != uploadOffset_) || (uploadComplete_ && !repeated))
    return fail("Bloco fora de ordem. Reinicie o envio.");
  uint64_t storedBytes=0;
  if (!disk_.fileSize(uploadComplete_ ? uploadPath_ : partial, storedBytes) ||
      storedBytes != uploadOffset_) {
    uploadId_.clear();
    return fail("Arquivo parcial alterado. Reinicie o envio.");
  }
  std::string data(length, '\0');
  size_t got = 0;
  while (got < length) {
    int n = reader(data.data() + got, length - got);
    if (n <= 0 || size_t(n) > length - got)
      return fail("Bloco interrompido; pode repetir o mesmo bloco.");
    got += size_t(n);
  }
  const auto hash = sha256(data);
  if (repeated)
    return hash == uploadHash_ ||
           fail("Bloco repetido com conteúdo diferente.");
  if (!disk_.appendAt(partial, data, uploadOffset_, uploadGeneration_)) {
    // A short SD write cannot safely be retried at this offset.
    disk_.remove(partial);
    uploadId_.clear();
    return fail("Falha de gravação no SD. Reinicie o envio.");
  }
  uploadLastOffset_ = offset;
  uploadHash_ = hash;
  uploadOffset_ += length;
  if (uploadOffset_ == uploadTotal_) {
    if (!disk_.fileSize(partial, storedBytes) || storedBytes != uploadTotal_ ||
        !disk_.move(partial, path)) {
      disk_.remove(partial);
      uploadId_.clear();
      return fail("Falha ao concluir arquivo no SD.");
    }
    uploadComplete_ = true;
  }
  error.clear();
  return true;
}
} // namespace tab5

namespace tab5 {
std::string Network::pageCachePath(const std::string &url){return "/data/browser/cache/"+sha256(std::string(browserProfile::cacheSalt)+url)+".page";}
std::string Network::imageCachePath(const std::string &url,bool mobile){return std::string(mobile?"/data/browser/assets/":"/cache/web-images/")+sha256(mobile?std::string(browserProfile::cacheSalt)+url:url)+".img";}
bool Network::images(const std::string &html,const std::string &base) {
  if(html.size()>96*1024 || base.size()>4096 || !status().connected)return false;
  auto blocks=mail::document(html);std::set<std::string> urls;
  for(const auto &block:blocks){
    std::string url,error;if(block.source.empty())continue;
    if(validUrl(block.source))url=block.source;
    else if(!base.empty())resolveHttpRedirect(base,block.source,url,error);
    if(validUrl(url))urls.insert(url);
    if(urls.size()>=6)break;
  }
  if(urls.empty())return false;
  return start([this,urls]{
    disk_.mkdir("/cache/web-images");unsigned done=0,failed=0;
    for(const auto &url:urls){
      if(cancel_)break;
      const auto path=imageCachePath(url);std::string old;
      if(disk_.read(path,old,256*1024)&&!old.empty()){++done;continue;}
      std::string data,error;
      TransferGuard active(activeTransfers_,recovering_);
      bool ok=active && backend.fetch && backend.fetch(url,[&](const char *p,size_t n){
        if(cancel_ || n>256*1024-data.size())return false;
        data.append(p,n);return true;
      },cancel_,error);
      if(ok&&!data.empty()){
        std::vector<File> cached;disk_.listDir("/cache/web-images",cached);
        while(cached.size()>=48){disk_.remove(cached.back().path);cached.pop_back();}
        ok=disk_.write(path,data);
      }else ok=false;
      if(ok)++done;else ++failed;
    }
    finish(!failed,"Imagens no cartão: "+std::to_string(done)+(failed?" • falharam/excederam 256 KiB: "+std::to_string(failed):""));
  });
}
}

namespace tab5 {
bool Network::documentAssets(const std::vector<std::pair<std::string,bool>> &requests,bool mobile){
 if(requests.empty()||requests.size()>16||!status().connected)return false;
 for(const auto &item:requests)if(!validUrl(item.first))return false;
 const auto generation=disk_.generation();
 std::string pageOrigin;{std::lock_guard<std::mutex> lock(mutex_);pageOrigin=page_.url;}
 return start([this,requests,mobile,generation,pageOrigin]{const std::string folder=mobile?"/data/browser/assets":"/cache/web-images";disk_.mkdir(folder);unsigned done=0,failed=0;
  BrowserCookies cookies;std::string saved;const auto now=int64_t(std::time(nullptr));if(mobile&&disk_.read("/data/browser/cookies.tsv",saved,100000)&&!cookies.load(saved,now))cookies=BrowserCookies{};
  BrowserRequest requestOptions;requestOptions.cookies=[&](const std::string &url,bool){return cookies.header(url,now,false,pageOrigin,false);};requestOptions.setCookie=[&](const std::string &url,const std::string &value){cookies.set(url,value,now);};
  for(const auto &request:requests){if(cancel_||disk_.generation()!=generation)break;const size_t limit=request.second?65536:256*1024;auto path=imageCachePath(request.first,mobile);std::string data,error;
   if(disk_.read(path,data,limit)&&!data.empty()){++done;continue;}data.clear();TransferGuard active(activeTransfers_,recovering_);
   const auto &fetch=mobile&&backend.fetchBrowserAsset?backend.fetchBrowserAsset:backend.fetch;
   const Sink sink=[&](const char *p,size_t n){if(cancel_||disk_.generation()!=generation||n>limit-data.size())return false;data.append(p,n);return true;};
   std::string resolved;bool ok=active&&(mobile&&backend.browserFetch?backend.browserFetch(request.first,requestOptions,sink,cancel_,error,resolved):fetch&&fetch(request.first,sink,cancel_,error));
   if(cancel_||disk_.generation()!=generation){++failed;break;}
   if(ok&&!data.empty()){std::vector<File> cached;disk_.listDir(folder,cached);while(cached.size()>=48){disk_.remove(cached.back().path);cached.pop_back();}ok=disk_.write(path,data);}else ok=false;
   if(ok)++done;else ++failed;
  }
  if(mobile&&!cancel_&&disk_.generation()==generation)disk_.write("/data/browser/cookies.tsv",cookies.save());
  finish(!failed&&!cancel_&&disk_.generation()==generation,"Recursos HTML no cartão: "+std::to_string(done)+(failed?" · falharam/excederam o limite: "+std::to_string(failed):""));
 });
}
}
