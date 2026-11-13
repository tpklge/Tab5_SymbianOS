// SPDX-License-Identifier: MIT
#include "http_fetch.hpp"
#include "http_body.hpp"
#include <algorithm>
#include <cctype>
namespace tab5 {
namespace {
std::string lower(std::string s) {
  for (char &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool canonical(const std::string &url, std::string &out) {
  auto colon = url.find("://");
  if (colon == std::string::npos)
    return false;
  auto scheme = lower(url.substr(0, colon));
  if (scheme != "http" && scheme != "https")
    return false;
  auto end = url.find_first_of("/?#", colon + 3);
  auto authority = lower(
      url.substr(colon + 3, end == std::string::npos ? end : end - colon - 3));
  if (authority.empty() || authority.find('@') != std::string::npos)
    return false;
  auto tail = end == std::string::npos ? std::string("/") : url.substr(end);
  auto fragment = tail.find('#');
  if (fragment != std::string::npos)
    tail.resize(fragment);
  if (tail.empty() || tail[0] == '?')
    tail = "/" + tail;
  auto q = tail.find('?');
  auto path = tail.substr(0, q),
       query = q == std::string::npos ? "" : tail.substr(q);
  std::vector<std::string> parts;
  size_t start = 1;
  while (start <= path.size()) {
    auto stop = path.find('/', start);
    if (stop == std::string::npos)
      stop = path.size();
    auto part = path.substr(start, stop - start);
    if (part == "..") {
      if (!parts.empty())
        parts.pop_back();
    } else if (part != ".")
      parts.push_back(part);
    if (stop == path.size())
      break;
    start = stop + 1;
  }
  std::string clean = "/";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i)
      clean += '/';
    clean += parts[i];
  }
  if ((path.size() >= 2 && path.substr(path.size() - 2) == "/.") ||
      (path.size() >= 3 && path.substr(path.size() - 3) == "/..")) {
    if (clean.back() != '/')
      clean += '/';
  }
  out = scheme + "://" + authority + clean + query;
  return Network::validUrl(out);
}
} // namespace
bool resolveHttpRedirect(const std::string &base, const std::string &location,
                         std::string &next, std::string &error) {
  if (location.empty() || location.size() > 1024) {
    error = "Redirecionamento sem Location válido";
    return false;
  }
  std::string current;
  if (!canonical(base, current)) {
    error = "URL de origem inválida";
    return false;
  }
  std::string loc = location;
  auto begin = loc.find_first_not_of(" \t"), end = loc.find_last_not_of(" \t");
  if (begin == std::string::npos) {
    error = "Location vazio";
    return false;
  }
  loc = loc.substr(begin, end - begin + 1);
  for (unsigned char c : loc)
    if (c <= 32 || c == 127 || c == '\\') {
      error = "Location contém caracteres inválidos";
      return false;
    }
  auto authorityEnd = current.find('/', current.find("://") + 3);
  auto origin = current.substr(0, authorityEnd);
  std::string candidate;
  if (loc.rfind("//", 0) == 0)
    candidate = current.substr(0, current.find(':')) + ":" + loc;
  else if (loc[0] == '/')
    candidate = origin + loc;
  else if (loc[0] == '?')
    candidate = current.substr(0, current.find('?')) + loc;
  else if (loc[0] == '#')
    candidate = current + loc;
  else if (loc.find(':') < loc.find_first_of("/?#"))
    candidate = loc;
  else {
    auto pathEnd = current.find('?');
    auto noQuery = current.substr(0, pathEnd);
    candidate = noQuery.substr(0, noQuery.rfind('/') + 1) + loc;
  }
  if (!canonical(candidate, next)) {
    error = "Destino de redirecionamento deve ser HTTP(S) válido";
    return false;
  }
  if (current.rfind("https://", 0) == 0 && next.rfind("http://", 0) == 0) {
    error = "Redirecionamento HTTPS para HTTP recusado";
    return false;
  }
  return true;
}
bool fetchHttp(HttpStream &http, const std::string &url,
               const Network::Sink &sink, const std::atomic<bool> &cancel,
               std::string &error, std::string *resolved,unsigned totalTimeoutMs) {
  error.clear();
  const auto started=http.nowMs();
  std::string current;
  if (!canonical(url, current)) {
    error = "URL HTTP(S) inválida";
    return false;
  }
  std::vector<std::string> visited{current};
  for (unsigned hop = 0;; ++hop) {
    if (cancel) {
      error = "Cancelado";
      return false;
    }
    int status = 0;
    std::string location;
    if (!http.open(current, status, location, error)) {
      http.close();
      return false;
    }
    bool redirect = status == 301 || status == 302 || status == 303 ||
                    status == 307 || status == 308;
    if (redirect) {
      http.close();
      if (cancel) {
        error = "Cancelado";
        return false;
      }
      if (hop >= 5) {
        error = "Limite de 5 redirecionamentos excedido";
        return false;
      }
      std::string next;
      if (!resolveHttpRedirect(current, location, next, error))
        return false;
      if (std::find(visited.begin(), visited.end(), next) != visited.end()) {
        error = "Ciclo de redirecionamentos detectado";
        return false;
      }
      visited.push_back(next);
      current = std::move(next);
      continue;
    }
    if (status != 200) {
      error = "Servidor respondeu HTTP " + std::to_string(status);
      http.close();
      return false;
    }
    char buffer[4096];
    bool ok = true;
    auto progress=http.nowMs();
    while (!cancel) {
      if(http.nowMs()-started>=totalTimeoutMs){error="Prazo total HTTP excedeu "+std::to_string(totalTimeoutMs/1000)+" segundos";ok=false;break;}
      int n = http.read(buffer, sizeof(buffer));
      if(n==HttpBody::again) {
        if(http.nowMs()-progress<30000){workerWait(std::chrono::milliseconds(10));continue;}
        error="Resposta HTTP/TLS interrompida: 30 segundos sem progresso";ok=false;break;
      }
      if(n>0)progress=http.nowMs();
      if (n < 0 || size_t(n) > sizeof(buffer)) {
        error = "Resposta HTTP/TLS interrompida";
        auto detail=http.diagnostic();if(!detail.empty()) error+=" • "+detail;
        ok = false;
        break;
      }
      if (n == 0)
        break;
      if (!sink(buffer, n)) {
        error = "Resposta não pôde ser salva no SD";
        ok = false;
        break;
      }
    }
    http.close();
    if (cancel) {
      error = "Cancelado";
      return false;
    }
    if(ok && resolved) *resolved=current;
    return ok;
  }
}
} // namespace tab5
