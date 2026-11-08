// SPDX-License-Identifier: MIT
#include "network.hpp"
#include <algorithm>
#include <cctype>
#include <new>
namespace tab5 {
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
  if (backend.linked && !backend.linked()) {
    value.connected = false;
    if (!value.busy && value.message == "Conectado")
      value.message = "Conexão perdida";
  }
  return value;
}
bool Network::start(std::function<void()> job) {
  if (completed_.exchange(false) && worker_)
    worker_->join();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_.busy)
      return false;
    status_.busy = true;
    status_.bytes = 0;
    status_.message = "Aguarde…";
  }
  cancel_ = false;
  if (!worker_)
    worker_ = std::make_unique<Executor>(49152, 2);
  if (!worker_->submit([this, job = std::move(job)] {
        job();
        completed_ = true;
      })) {
    finish(false, "Worker de rede indisponível");
    return false;
  }
  return true;
}
void Network::finish(bool ok, const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_.busy = false;
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
      status_.address = ok ? address : "";
      status_.ssid = ok ? ssid : "";
    }
    finish(ok,
           ok                   ? (persisted ? "Conectado"
                                             : "Conectado; não foi possível salvar a rede no SD")
           : backend.diagnostic ? backend.diagnostic()
                                : "Falha de conexão Wi-Fi");
  });
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
    {
      std::lock_guard<std::mutex> lock(mutex_);
      status_.sharing = ok && on;
      status_.token = status_.sharing ? token : "";
      if (ok && on)
        status_.address = address;
    }
    finish(ok,
           ok ? (on ? "Transferência local ativa" : "Transferência encerrada")
              : "Conecte o Wi-Fi antes de compartilhar");
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
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = Status{};
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
bool Network::download(const std::string &url, const std::string &path,
                       bool feed) {
  if (!validUrl(url) || !Storage::validPath(path) ||
      (feed ? path != "/data/feeds/latest.xml" : !sharedPath(path)))
    return false;
  return start([this, url, path, feed] {
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
    if (!disk_.write(partial, empty)) {
      finish(false, disk_.lastError());
      return;
    }
    uint64_t count = 0, limit = feed ? 65536 : 64 * 1024 * 1024;
    auto sink = [&](const char *data, size_t n) {
      if (cancel_ || n > 16384 || count + n > limit)
        return false;
      if (!disk_.append(partial, std::string(data, n)))
        return false;
      count += n;
      std::lock_guard<std::mutex> lock(mutex_);
      status_.bytes = count;
      return true;
    };
    bool ok =
        backend.fetch && backend.fetch(url, sink, cancel_, error) && !cancel_;
    if (ok && feed) {
      std::string text;
      ok = disk_.read(partial, text, 65536) &&
           text.find("<!DOCTYPE") == std::string::npos &&
           text.find("<!ENTITY") == std::string::npos &&
           (text.find("<rss") != std::string::npos ||
            text.find("<feed") != std::string::npos);
      if (ok)
        ok = disk_.write(path, text);
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
  std::unique_lock<std::mutex> transfer(transferMutex_, std::try_to_lock);
  auto report = [&](const char *stage, size_t n) {
    if (progress)
      progress(stage, n);
  };
  if (!transfer.owns_lock())
    return false;
  report("checking", 0);
  if (!sharedPath(path) || !length || length > 64 * 1024 * 1024 ||
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
  constexpr size_t blockSize = 4096;
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
    int n = read(buffer.get(), requested);
    if (n <= 0 || size_t(n) > requested) {
      ok = false;
      break;
    }
    if (!disk_.append(partial, std::string(buffer.get(), n))) {
      failure = "write-failed";
      ok = false;
      break;
    }
    received += n;
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
bool Network::send(const std::string &path, const Sink &sink) {
  if (!sharedPath(path))
    return false;
  uint64_t offset = 0;
  for (;;) {
    std::string data;
    if (!disk_.readRange(path, offset, 16384, data))
      return false;
    if (data.empty())
      return true;
    if (!sink(data.data(), data.size()))
      return false;
    offset += data.size();
  }
}
} // namespace tab5
