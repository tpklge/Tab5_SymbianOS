// SPDX-License-Identifier: MIT
// Original bounded IMAP/POP3/SMTP client. Wire backend owns certificate
// validation.
#include "mail_protocol.hpp"
#include "executor.hpp"
#include "text_lines.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
namespace tab5::mail {
static std::string upper(std::string s) {
  for (char &c : s)
    c = char(std::toupper((unsigned char)c));
  return s;
}
static std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \t\r\n");
  return a == s.npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
static bool clean(const std::string &s, size_t n) {
  return s.size() <= n && s.find_first_of("\r\n\0", 0, 3) == s.npos;
}
bool address(const std::string &s) {
  auto at = s.find('@');
  if (at == s.npos || !at || at == s.size() - 1 || s.size() > 254 ||
      s.find('@', at + 1) != s.npos)
    return false;
  for (unsigned char c : s)
    if (c <= 32 || c >= 127 || c == '<' || c == '>' || c == '"' || c == '\\' ||
        c == ',' || c == ';')
      return false;
  return true;
}
bool validAccount(const MailAccount &a, std::string &error) {
  auto host = [](const std::string &s) {
    return !s.empty() && s.size() <= 253 &&
           s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU"
                               "VWXYZ0123456789.-:") == s.npos;
  };
  if (!address(a.address) || !clean(a.name, 80) ||
      (a.protocol != "imap" && a.protocol != "pop") || !host(a.inHost) ||
      !host(a.outHost) || !a.inPort || a.inPort > 65535 || !a.outPort ||
      a.outPort > 65535 ||
      (a.inSecurity != "tls" && a.inSecurity != "starttls") ||
      (a.outSecurity != "tls" && a.outSecurity != "starttls") ||
      a.inUser.empty() || !clean(a.inUser, 254) || !clean(a.inPassword, 256) ||
      !clean(a.outUser, 254) || !clean(a.outPassword, 256)) {
    error = "Confira endereço, servidores, portas 1–65535 e credenciais.";
    return false;
  }
  return true;
}
std::string hex(const std::string &s) {
  const char *h = "0123456789abcdef";
  std::string o;
  o.reserve(s.size() * 2);
  for (unsigned char c : s) {
    o += h[c >> 4];
    o += h[c & 15];
  }
  return o;
}
std::string unhex(const std::string &s) {
  if (s.size() % 2)
    return {};
  std::string o;
  for (size_t i = 0; i < s.size(); i += 2) {
    auto n = [](unsigned char c) {
      return c >= '0' && c <= '9'   ? c - '0'
             : c >= 'a' && c <= 'f' ? c - 'a' + 10
             : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                    : -1;
    };
    int a = n(s[i]), b = n(s[i + 1]);
    if (a < 0 || b < 0)
      return {};
    o += char(a * 16 + b);
  }
  return o;
}
std::string base64(const std::string &s) {
  const char *t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < s.size(); i += 3) {
    unsigned n = unsigned((unsigned char)s[i]) << 16;
    if (i + 1 < s.size())
      n |= unsigned((unsigned char)s[i + 1]) << 8;
    if (i + 2 < s.size())
      n |= (unsigned char)s[i + 2];
    o += t[n >> 18];
    o += t[(n >> 12) & 63];
    o += i + 1 < s.size() ? t[(n >> 6) & 63] : '=';
    o += i + 2 < s.size() ? t[n & 63] : '=';
  }
  return o;
}
std::string unbase64(const std::string &s) {
  std::string o;
  unsigned v = 0, bits = 0;
  const std::string t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (unsigned char c : s) {
    if (c == '=')
      break;
    auto n = t.find(c);
    if (n == t.npos) {
      if (std::isspace(c))
        continue;
      return {};
    }
    v = (v << 6) | unsigned(n);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      o += char(v >> bits);
    }
  }
  return o;
}
static std::string qp(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '=' && i + 1 < s.size()) {
      if (s[i + 1] == '\n') {
        ++i;
        continue;
      }
      if (s[i + 1] == '\r' && i + 2 < s.size() && s[i + 2] == '\n') {
        i += 2;
        continue;
      }
      if (i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) &&
          std::isxdigit((unsigned char)s[i + 2])) {
        o += unhex(s.substr(i + 1, 2));
        i += 2;
        continue;
      }
    }
    o += s[i];
  }
  return o;
}
static std::string utf8(const std::string &s, const std::string &charset) {
  std::string out;
  const auto encoding = upper(charset);
  const bool windows = encoding.find("1252") != std::string::npos;
  bool latin = encoding.find("8859-1") != std::string::npos || windows;
  for (size_t i = 0; i < s.size() && out.size() < maxRaw * 3; ++i) {
    unsigned char c = s[i];
    if (c == 0)
      continue;
    if (latin && c >= 128) {
      if (windows && c < 160) {
        static const unsigned short map[] = {
            0x20ac, 0xfffd, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
            0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0xfffd, 0x017d, 0xfffd,
            0xfffd, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
            0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0xfffd, 0x017e, 0x0178};
        unsigned cp = map[c - 128];
        if (cp < 0x800) {
          out += char(0xc0 | (cp >> 6));
          out += char(0x80 | (cp & 63));
        } else {
          out += char(0xe0 | (cp >> 12));
          out += char(0x80 | ((cp >> 6) & 63));
          out += char(0x80 | (cp & 63));
        }
        continue;
      }
      out += char(0xc0 | (c >> 6));
      out += char(0x80 | (c & 63));
      continue;
    }
    if (c < 128) {
      out += char(c);
      continue;
    }
    unsigned n = c >= 0xc2 && c <= 0xdf   ? 2
                 : c >= 0xe0 && c <= 0xef ? 3
                 : c >= 0xf0 && c <= 0xf4 ? 4
                                          : 0;
    bool valid = n && i + n <= s.size();
    for (unsigned j = 1; valid && j < n; ++j)
      valid = ((unsigned char)s[i + j] & 0xc0) == 0x80;
    if (valid && n >= 3) {
      unsigned char b = s[i + 1];
      valid = !(c == 0xe0 && b < 0xa0) && !(c == 0xed && b >= 0xa0) &&
              !(c == 0xf0 && b < 0x90) && !(c == 0xf4 && b >= 0x90);
    }
    if (valid) {
      out.append(s, i, n);
      i += n - 1;
    } else
      out += "�";
  }
  return out;
}
static std::map<std::string, std::string> headers(const std::string &s) {
  std::map<std::string, std::string> h;
  TextLines in(s);
  std::string line, key;
  unsigned n = 0;
  while (nextLine(in, line) && n++ < 200) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break;
    if ((line[0] == ' ' || line[0] == '\t') && !key.empty())
      h[key] += " " + trim(line);
    else {
      auto p = line.find(':');
      if (p != line.npos) {
        key = upper(line.substr(0, p));
        if (!h.count(key))
          h[key] = trim(line.substr(p + 1));
      }
    }
  }
  return h;
}
static std::string parameter(const std::string &s, const std::string &name) {
  auto p = upper(s).find(upper(name) + "=");
  if (p == s.npos)
    return {};
  p += name.size() + 1;
  if (p >= s.size())
    return {};
  if (s[p] == '"') {
    auto e = s.find('"', p + 1);
    return e == s.npos ? "" : s.substr(p + 1, e - p - 1);
  }
  auto e = s.find_first_of("; \r\n", p);
  return s.substr(p, e == s.npos ? s.npos : e - p);
}
static std::string words(const std::string &s) {
  std::string out;
  size_t at = 0;
  while (at < s.size()) {
    auto p = s.find("=?", at);
    if (p == s.npos) {
      out += s.substr(at);
      break;
    }
    out += s.substr(at, p - at);
    auto a = s.find('?', p + 2), b = a == s.npos ? s.npos : s.find('?', a + 1),
         e = b == s.npos ? s.npos : s.find("?=", b + 1);
    if (e == s.npos) {
      out += s.substr(p);
      break;
    }
    auto data = s.substr(b + 1, e - b - 1);
    if (upper(s.substr(a + 1, b - a - 1)) == "B")
      data = unbase64(data);
    else {
      std::replace(data.begin(), data.end(), '_', ' ');
      data = qp(data);
    }
    out += utf8(data, s.substr(p + 2, a - p - 2));
    at = e + 2;
    if (s.compare(at, 3, " =?") == 0)
      ++at;
  }
  return utf8(out, "");
}
static std::string html(const std::string &s) {
  std::string o;
  bool tag = false, hidden = false;
  std::string token;
  for (char c : s) {
    if (c == '<') {
      tag = true;
      token.clear();
    } else if (c == '>' && tag) {
      auto t = upper(trim(token));
      if (t.rfind("SCRIPT", 0) == 0 || t.rfind("STYLE", 0) == 0)
        hidden = true;
      else if (t == "/SCRIPT" || t == "/STYLE")
        hidden = false;
      else if (t == "BR" || t == "BR/" || t == "/P" || t == "/DIV" ||
               t == "/LI")
        o += '\n';
      tag = false;
    } else if (tag) {
      if (token.size() < 256)
        token += c;
    } else if (!hidden)
      o += c;
  }
  for (auto pair : {std::pair<const char *, const char *>{"&nbsp;", " "},
                    {"&lt;", "<"},
                    {"&gt;", ">"},
                    {"&amp;", "&"},
                    {"&quot;", "\""}}) {
    size_t p = 0;
    while ((p = o.find(pair.first, p)) != o.npos) {
      o.replace(p, strlen(pair.first), pair.second);
      p += strlen(pair.second);
    }
  }
  return o;
}
static std::string mime(const std::string &raw, unsigned depth, MailMessage *rich = nullptr,const AttachmentSink &sink = {}) {
  if (depth > 4)
    return "[MIME muito profundo]";
  auto split = raw.find("\r\n\r\n");
  size_t skip = 4;
  if (split == raw.npos) {
    split = raw.find("\n\n");
    skip = 2;
  }
  if (split == raw.npos)
    return utf8(raw, "");
  auto h = headers(raw.substr(0, split));
  auto body = raw.substr(split + skip);
  auto type = upper(h["CONTENT-TYPE"]);
  if (type.find("MULTIPART/") != type.npos) {
    auto boundary = parameter(h["CONTENT-TYPE"], "boundary");
    if (boundary.empty() || boundary.size() > 200)
      return "[MIME sem separador válido]";
    std::string mark = "--" + boundary, best;
    auto validBoundary=[&](size_t at) {
      if(at && body[at-1]!='\n')return false;
      auto next=at+mark.size();
      if(body.compare(next,2,"--")==0)next+=2;
      return next==body.size()||body.compare(next,2,"\r\n")==0||body.compare(next,1,"\n")==0;
    };
    size_t p = body.find(mark);
    while(p!=body.npos&&!validBoundary(p))p=body.find(mark,p+1);
    unsigned parts = 0;
    while (p != body.npos && parts++ < 30) {
      p += mark.size();
      if (body.compare(p, 2, "--") == 0)
        break;
      if (body.compare(p, 2, "\r\n") == 0)
        p += 2;
      else if (body.compare(p, 1, "\n") == 0)
        ++p;
      auto e = body.find("\n" + mark, p);
      while(e!=body.npos&&!validBoundary(e+1))e=body.find("\n"+mark,e+1);
      if (e == body.npos)
        break;
      // The CRLF introducing a MIME boundary belongs to the delimiter.
      auto end = e;
      if (end > p && body[end - 1] == '\r') --end;
      auto part = body.substr(p, end - p);
      auto ph = headers(part);
      if (sink || upper(ph["CONTENT-DISPOSITION"]).find("ATTACHMENT") == std::string::npos) {
        auto text = mime(part, depth + 1, rich, sink);
        if ((!text.empty() && best.empty()) ||
            upper(ph["CONTENT-TYPE"]).find("TEXT/PLAIN") != std::string::npos)
          best = text;
      }
      p = e + 1;
    }
    return best.empty() ? (rich&&!rich->attachments.empty()?"[Mensagem com anexos; abra Anexos]":"[Mensagem sem parte de texto; anexos não baixados]")
                        : best;
  }
  auto disposition=upper(h["CONTENT-DISPOSITION"]);
  bool attached=disposition.find("ATTACHMENT")!=std::string::npos;
  bool binary=!type.empty()&&type.rfind("TEXT/",0)!=0&&type.rfind("IMAGE/",0)!=0;
  if(attached || binary) {
    if(rich && sink){
      auto name=parameter(h["CONTENT-DISPOSITION"],"filename");if(name.empty())name=parameter(h["CONTENT-TYPE"],"name");
      auto extended=parameter(h["CONTENT-DISPOSITION"],"filename*");auto split=extended.find("''");
      if(split!=extended.npos){auto escaped=extended.substr(split+2);std::string decoded;for(size_t p=0;p<escaped.size();++p){if(escaped[p]=='%'&&p+2<escaped.size()){auto pair=unhex(escaped.substr(p+1,2));if(!pair.empty()){decoded+=pair;p+=2;continue;}}decoded+=escaped[p];}name=utf8(decoded,extended.substr(0,split));}
      if(name.empty())name="attachment.bin";
      name=words(name);for(auto &c:name)if((unsigned char)c<32 || c=='/'||c=='\\'||c==':'||c=='"')c='_';
      if(name.size()>120){size_t n=120;while(n&&((unsigned char)name[n]&0xc0)==0x80)--n;name.resize(n);}
      MailAttachment a;if(sink(name,type,upper(h["CONTENT-TRANSFER-ENCODING"]),body,a))rich->attachments.push_back(std::move(a));
    }
    return {};
  }
  if (type.rfind("IMAGE/PNG",0)==0 || type.rfind("IMAGE/JPEG",0)==0 || type.rfind("IMAGE/GIF",0)==0) {
    if(rich && rich->images.size()<6) {
      auto data=upper(h["CONTENT-TRANSFER-ENCODING"])=="BASE64"?unbase64(body):body;
      size_t total=data.size();for(const auto &image:rich->images)total+=image.data.size();
      auto cid=trim(h["CONTENT-ID"]);if(cid.size()>1&&cid.front()=='<'&&cid.back()=='>')cid=cid.substr(1,cid.size()-2);
      if(cid.empty())cid=h["CONTENT-LOCATION"];
      if(!cid.empty()&&cid.size()<=4096&&total<=512*1024)rich->images.push_back({cid,type,std::move(data)});
    }
    return {};
  }
  if ((!type.empty() && type.find("TEXT/") == type.npos) ||
      (type.find("TEXT/") != type.npos &&
       type.find("TEXT/PLAIN") == type.npos &&
       type.find("TEXT/HTML") == type.npos))
    return "[Parte não textual omitida]";
  auto encoding = upper(h["CONTENT-TRANSFER-ENCODING"]);
  if (encoding == "BASE64")
    body = unbase64(body);
  else if (encoding == "QUOTED-PRINTABLE")
    body = qp(body);
  body = utf8(body, parameter(h["CONTENT-TYPE"], "charset"));
  if(rich && type.find("TEXT/HTML")!=type.npos && rich->html.empty() && body.size()<=96*1024)rich->html=body;
  return type.find("TEXT/HTML") != type.npos ? html(body) : body;
}
MailMessage decodeMessage(const std::string &raw,const AttachmentSink &sink) {
  auto h = headers(raw);
  MailMessage m;
  m.from = words(h["FROM"]);
  m.to = words(h["TO"]);
  m.subject = words(h["SUBJECT"]);
  if (m.subject.empty())
    m.subject = "(sem assunto)";
  m.date = words(h["DATE"]);
  m.messageId = h["MESSAGE-ID"];
  auto cut = [](std::string &s, size_t cap) {
    if (s.size() > cap) {
      size_t n = cap;
      while (n && ((unsigned char)s[n] & 0xc0) == 0x80)
        --n;
      s.resize(n);
    }
  };
  cut(m.from, 400);
  cut(m.to, 800);
  cut(m.subject, 200);
  cut(m.date, 120);
  m.body = mime(raw, 0, &m, sink);
  if (m.body.size() > maxBody) {
    const std::string note = "\n\n[Texto limitado a 48 KiB no Tab5]";
    cut(m.body, maxBody - note.size());
    m.body += note;
  }
  return m;
}
std::string encodeMessage(const MailAccount &a, const MailMessage &m) {
  auto encoded = [](const std::string &s) {
    std::string out;
    size_t at = 0;
    do {
      size_t end = std::min(at + 45, s.size());
      while (end < s.size() && end > at &&
             ((unsigned char)s[end] & 0xc0) == 0x80)
        --end;
      if (!out.empty())
        out += "\r\n ";
      out += "=?UTF-8?B?" + base64(s.substr(at, end - at)) + "?=";
      at = end;
    } while (at < s.size());
    return out;
  };
  auto body = base64(m.body);
  std::string lines;
  for (size_t i = 0; i < body.size(); i += 76)
    lines += body.substr(i, 76) + "\r\n";
  time_t now = time(nullptr);
  tm date{};
  gmtime_r(&now, &date);
  char dateText[80];
  strftime(dateText, sizeof(dateText), "%a, %d %b %Y %H:%M:%S +0000", &date);
  return "From: " +
         (a.name.empty() ? a.address
                         : encoded(a.name) + " <" + a.address + ">") +
         "\r\nTo: " + m.to + "\r\nSubject: " + encoded(m.subject) +
         "\r\nDate: " + dateText + "\r\nMessage-ID: " + m.messageId +
         "\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; "
         "charset=UTF-8\r\nContent-Transfer-Encoding: base64\r\n\r\n" +
         lines;
}
class Session {
  MailWire &wire;
  const std::atomic<bool> &cancel;
  std::string pending;
  std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + std::chrono::minutes(5);
  std::chrono::steady_clock::time_point idleDeadline;
  std::string stage = "Conexão";
  void activity() {
    idleDeadline = std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(wire.protocolTimeoutMs);
  }
  unsigned tag = 0;

public:
  std::string &error;
  Session(MailWire &w, const std::atomic<bool> &c, std::string &e)
      : wire(w), cancel(c), error(e) {
    wire.cancellation = &c;
    activity();
  }
  void step(const std::string &name) {
    stage = name;
    activity();
    if (wire.progress)
      wire.progress("Etapa=" + stage + "\n" + wire.diagnostic());
  }
  bool alive() {
    if (cancel) {
      error = "Operação cancelada";
      return false;
    }
    if (std::chrono::steady_clock::now() > deadline ||
        std::chrono::steady_clock::now() > idleDeadline) {
      error = "Tempo limite do servidor de e-mail\nEtapa=" + stage + "\n" +
              wire.diagnostic();
      return false;
    }
    return true;
  }
  bool put(const std::string &s) {
    size_t at = 0;
    while (at < s.size()) {
      if (!alive())
        return false;
      int n = wire.write(s.data() + at, s.size() - at);
      if (n == -2) {
        workerWait(std::chrono::milliseconds(10));
        continue;
      }
      if (n <= 0) {
        error = "Conexão de e-mail interrompida";
        return false;
      }
      activity();
      at += size_t(n);
    }
    return true;
  }
  bool bytes(size_t n, std::string &out) {
    out.clear();
    if (n > maxRaw) {
      error = "Resposta acima de 1 MiB";
      return false;
    }
    while (n) {
      if (!alive())
        return false;
      if (pending.empty()) {
        char buf[2048];
        int got = wire.read(buf, sizeof(buf));
        if (got == -2) {
          workerWait(std::chrono::milliseconds(10));
          continue;
        }
        if (got <= 0) {
          error = "Conexão de e-mail interrompida";
          return false;
        }
        activity();
        pending.assign(buf, size_t(got));
      }
      auto take = std::min(n, pending.size());
      out.append(pending, 0, take);
      pending.erase(0, take);
      n -= take;
    }
    return true;
  }
  bool line(std::string &out) {
    while (true) {
      if (!alive())
        return false;
      auto end = pending.find('\n');
      if (end != pending.npos) {
        if (end > 16384) {
          error = "Linha de servidor muito longa";
          return false;
        }
        out = pending.substr(0, end);
        pending.erase(0, end + 1);
        if (!out.empty() && out.back() == '\r')
          out.pop_back();
        return true;
      }
      if (pending.size() > 16384) {
        error = "Linha de servidor muito longa";
        return false;
      }
      char b[2048];
      int n = wire.read(b, sizeof(b));
      if (n == -2) {
        workerWait(std::chrono::milliseconds(10));
        continue;
      }
      if (n <= 0) {
        error = "Conexão de e-mail interrompida";
        return false;
      }
      activity();
      pending.append(b, size_t(n));
    }
  }
  bool upgrade(const std::string &host) {
    if (!pending.empty()) {
      error = "Dados inesperados antes do TLS";
      return false;
    }
    if (!alive() || !wire.upgrade(host, error))
      return false;
    activity();
    return true;
  }
  bool smtp(int want, std::string *caps = nullptr, int *observed = nullptr,
            int alternate = 0, int alternate2 = 0) {
    std::string lineText;
    for (unsigned n = 0; n < 80; ++n) {
      if (!line(lineText))
        return false;
      if (lineText.size() < 4 || !std::isdigit((unsigned char)lineText[0]) ||
          !std::isdigit((unsigned char)lineText[1]) ||
          !std::isdigit((unsigned char)lineText[2]))
        break;
      int code = atoi(lineText.c_str());
      if (observed)
        *observed = code;
      if (code != want && code != alternate && code != alternate2) {
        error = code==535 ? "SMTP 535: autenticação recusada. Confira usuário SMTP completo e senha de envio em Conta > Envio SMTP (credenciais separadas do recebimento)." : "SMTP recusou a operação (" + std::to_string(code) + ")";
        return false;
      }
      if (caps) {
        if (caps->size() + lineText.size() > 65536) {
          error = "Capacidades SMTP acima do limite";
          return false;
        }
        *caps += upper(lineText) + "\n";
      }
      if (lineText[3] == ' ')
        return true;
      if (lineText[3] != '-')
        break;
    }
    error = "Resposta SMTP inválida";
    return false;
  }
  bool pop(const std::string &command) {
    step("POP " + (command.empty() ? std::string("saudação")
                                   : command.substr(0, command.find(' '))));
    std::string s;
    if ((!command.empty() && !put(command + "\r\n")) || !line(s))
      return false;
    if (s.rfind("+OK", 0) != 0) {
      error = "POP recusou a operação/autenticação";
      return false;
    }
    return true;
  }
  bool multiline(std::vector<std::string> &out, size_t max = maxRaw) {
    std::string s;
    size_t total = 0;
    out.clear();
    while (line(s)) {
      if (s == ".")
        return true;
      if (s.rfind("..", 0) == 0)
        s.erase(0, 1);
      total += s.size() + 2;
      if (total > max) {
        error = "Mensagem POP acima do limite de 1 MiB";
        return false;
      }
      out.push_back(s);
    }
    return false;
  }
  static std::string quote(const std::string &s) {
    std::string o = "\"";
    for (char c : s) {
      if (c == '"' || c == '\\')
        o += '\\';
      o += c;
    }
    return o + '"';
  }
  bool imap(const std::string &command, std::vector<std::string> &lines,
            std::string *literal = nullptr) {
    auto id = "T" + std::to_string(++tag);
    const auto operation = command.substr(
        0, command.find(' ', command.rfind("UID ", 0) == 0 ? 4 : 0));
    step("IMAP " + operation + " " + id + " · enviando");
    if (!put(id + " " + command + "\r\n"))
      return false;
    step("IMAP " + operation + " " + id + " · aguardando resposta");
    lines.clear();
    std::string s;
    size_t total = 0;
    for (unsigned count = 0; count < 4096 && line(s); ++count) {
      total += s.size();
      if (total > 65536) {
        error = "Índice IMAP acima do limite";
        return false;
      }
      if (s.rfind(id + " ", 0) == 0) {
        if (s.rfind(id + " OK", 0) == 0)
          return true;
        const auto reply = s.rfind(id + " NO", 0) == 0 ? "NO" : "BAD";
        error = std::string("IMAP recusou a operação/autenticação (") + reply +
                ")\nEtapa=" + stage + "\n" + wire.diagnostic();
        return false;
      }
      if (s.rfind("* BYE", 0) == 0) {
        error = "Servidor IMAP encerrou a sessão";
        return false;
      }
      lines.push_back(s);
      if (!s.empty() && s.back() == '}') {
        auto p = s.rfind('{');
        if (p == s.npos) {
          error = "Literal IMAP inválido";
          return false;
        }
        auto length = s.substr(p + 1, s.size() - p - 2);
        if (length.empty() ||
            length.find_first_not_of("0123456789") != length.npos) {
          error = "Literal IMAP inválido";
          return false;
        }
        auto number = strtoull(length.c_str(), nullptr, 10);
        if (number > maxRaw) {
          error = "Literal IMAP acima do limite";
          return false;
        }
        size_t n = size_t(number);
        std::string raw;
        if (!bytes(n, raw))
          return false;
        if (literal)
          *literal = std::move(raw);
        else if (n < 1024)
          lines.back() += " " + raw;
      }
    }
    if (error.empty())
      error = "Resposta IMAP incompleta";
    return false;
  }
};
bool receive(MailWire &wire, const MailAccount &a, const std::string &folder,
             const Known &known, const Delivery &deliver,
             std::vector<std::string> &folders, const std::atomic<bool> &cancel,
             std::string &error, const std::string &target) {
  if (cancel) {
    error = "Operação cancelada";
    return false;
  }
  Session s(wire, cancel, error);
  if (!wire.open(a.inHost, a.inPort, a.inSecurity == "tls", error))
    return false;
  std::vector<std::string> lines;
  unsigned downloaded = 0;
  if (a.protocol == "imap") {
    s.step("IMAP · aguardando saudação");
    std::string greeting;
    if (!s.line(greeting))
      return false;
    if (greeting.rfind("* OK", 0) != 0) {
      error = "Saudação IMAP inválida";
      return false;
    }
    if (!s.imap("CAPABILITY", lines))
      return false;
    if (a.inSecurity == "starttls") {
      std::string caps;
      for (auto &l : lines)
        caps += upper(l);
      if (caps.find("STARTTLS") == caps.npos) {
        error = "Servidor IMAP não oferece STARTTLS";
        return false;
      }
      if (!s.imap("STARTTLS", lines) || !s.upgrade(a.inHost) ||
          !s.imap("CAPABILITY", lines))
        return false;
    }
    if (!s.imap("LOGIN " + Session::quote(a.inUser) + " " +
                    Session::quote(a.inPassword),
                lines))
      return false;
    if (!s.imap("LIST \"\" \"*\"", lines)) {
      return false;
    }
    folders.clear();
    folders.push_back("INBOX");
    for (auto &l : lines) {
      if (l.rfind("* LIST", 0) != 0 || upper(l).find("\\NOSELECT") != l.npos)
        continue;
      auto p = l.find(')');
      if (p == l.npos)
        continue;
      ++p;
      auto token = [&]() {
        while (p < l.size() && l[p] == ' ')
          ++p;
        std::string out;
        if (p == l.size())
          return out;
        if (l[p] == '"') {
          ++p;
          while (p < l.size()) {
            char c = l[p++];
            if (c == '"')
              break;
            if (c == '\\' && p < l.size())
              c = l[p++];
            out += c;
          }
        } else {
          auto e = l.find(' ', p);
          out = l.substr(p, e == l.npos ? l.npos : e - p);
          p = e == l.npos ? l.size() : e;
        }
        return out;
      };
      token();
      auto name = token();
      if (!name.empty() && name[0] == '{') {
        while (p < l.size() && l[p] == ' ')
          ++p;
        name = l.substr(p);
      }
      if (!name.empty() && name != "INBOX" && clean(name, 200) &&
          folders.size() < 20)
        folders.push_back(name);
    }
    if (!clean(folder, 200) ||
        !s.imap("SELECT " + Session::quote(folder.empty() ? "INBOX" : folder),
                lines))
      return false;
    std::string validity = "unknown";
    for (auto &l : lines) {
      auto p = upper(l).find("UIDVALIDITY ");
      if (p != l.npos) {
        p += 12;
        auto e = l.find_first_not_of("0123456789", p);
        validity = l.substr(p, e - p);
      }
    }
    if (validity == "unknown") {
      error = "Servidor não informou UIDVALIDITY";
      return false;
    }
    // SELECT includes the message count; bound SEARCH to the latest 200
    // entries.
    unsigned long long exists = 0;
    for (const auto &line : lines) {
      auto end = line.find(" EXISTS");
      if (line.rfind("* ", 0) == 0 && end != line.npos) {
        auto number = line.substr(2, end - 2);
        if (!number.empty() &&
            number.find_first_not_of("0123456789") == number.npos)
          exists = strtoull(number.c_str(), nullptr, 10);
      }
    }
    std::vector<std::string> ids;
    if(!target.empty()) {
      auto prefix="imap:"+folder+":"+validity+":";
      auto id=target.rfind(prefix,0)==0?target.substr(prefix.size()):std::string();
      if(id.empty()||id.find_first_not_of("0123456789")!=id.npos){error="Mensagem de outra geração/caixa IMAP; atualize a lista";return false;}
      ids.push_back(id);
    }else {
    auto search = exists > 200 ? std::to_string(exists - 199) + ":*" : "ALL";
    if (!s.imap("UID SEARCH " + search, lines)) {
      return false;
    }
    for (auto &l : lines)
      if (l.rfind("* SEARCH", 0) == 0) {
        TextLines in(l.substr(8));
        std::string id;
        while (nextLine(in, id, ' ')) {
          if (id.empty())
            continue;
          if (id.find_first_not_of("0123456789") != id.npos ||
              ids.size() >= 8192) {
            error = "Índice IMAP inválido ou grande demais";
            return false;
          }
          ids.push_back(id);
        }
      }
    }
    for (auto it = ids.rbegin(); it != ids.rend() && downloaded < 10; ++it) {
      auto uid = "imap:" + folder + ":" + validity + ":" + *it;
      if (known(uid))
        continue;
      if (!s.imap("UID FETCH " + *it + " (RFC822.SIZE)", lines))
        return false;
      unsigned long long size = 0;
      for (const auto &line : lines) {
        auto pos = upper(line).find("RFC822.SIZE ");
        if (pos != line.npos)
          size = strtoull(line.c_str() + pos + 12, nullptr, 10);
      }
      std::string raw;
      if (size > maxRaw) {
        if (!s.imap("UID FETCH " + *it +
                        " (BODY.PEEK[HEADER.FIELDS (FROM TO SUBJECT DATE "
                        "MESSAGE-ID)])",
                    lines, &raw))
          return false;
        auto boundary = raw.find("\r\n\r\n");
        if (boundary == raw.npos)
          boundary = raw.find("\n\n");
        if (boundary != raw.npos)
          raw.resize(boundary);
        raw = "X-Tab5-Header-Only: 1\r\n" + raw + "\r\n\r\n";
        raw += "[Mensagem de " + std::to_string(size) +
               " bytes: corpo/anexos acima de 1 MiB não foram baixados. O "
               "conteúdo completo permanece no servidor.]";
      } else if (!s.imap("UID FETCH " + *it + " (BODY.PEEK[]<0.1048577>)", lines,
                         &raw))
        return false;
      if (!deliver(uid, raw))
        return false;
      ++downloaded;
    }
  } else {
    if (!s.pop(""))
      return false;
    if (a.inSecurity == "starttls") {
      if (!s.pop("STLS") || !s.upgrade(a.inHost))
        return false;
    }
    if (!s.pop("USER " + a.inUser) || !s.pop("PASS " + a.inPassword) ||
        !s.pop("UIDL") || !s.multiline(lines, 65536))
      return false;
    folders = {"INBOX"};
    for (auto it = lines.rbegin(); it != lines.rend() && downloaded < 10;
         ++it) {
      auto p = it->find(' ');
      if (p == it->npos)
        continue;
      auto number = it->substr(0, p);
      auto uid = "pop:" + it->substr(p + 1);
      if (number.empty() ||
          number.find_first_not_of("0123456789") != number.npos ||
          uid.size() > 256) {
        error = "UIDL POP inválido";
        return false;
      }
      if (known(uid))
        continue;
      if (!s.pop("RETR " + number))
        return false;
      std::vector<std::string> text;
      if (!s.multiline(text))
        return false;
      std::string raw;
      for (auto &l : text)
        raw += l + "\r\n";
      if (!deliver(uid, raw))
        return false;
      ++downloaded;
    }
  }
  return true;
}
SendResult send(MailWire &wire, const MailAccount &a, const MailMessage &m,
                const std::atomic<bool> &cancel, std::string &error,const AttachmentReader &reader) {
  if (cancel) {
    error = "Operação cancelada";
    return SendResult::rejected;
  }
  if(!a.outUser.empty() && a.outPassword.empty()){error="Senha SMTP de envio vazia. Configure Conta > Envio SMTP; a senha IMAP/POP não é usada no envio.";return SendResult::rejected;}
  Session s(wire, cancel, error);
  if (!wire.open(a.outHost, a.outPort, a.outSecurity == "tls", error))
    return SendResult::rejected;
  s.step("SMTP · saudação / EHLO");
  std::string caps;
  if (!s.smtp(220) || !s.put("EHLO tab5.local\r\n") || !s.smtp(250, &caps))
    return SendResult::rejected;
  if (a.outSecurity == "starttls") {
    if (caps.find("STARTTLS") == caps.npos) {
      error = "Servidor SMTP não oferece STARTTLS";
      return SendResult::rejected;
    }
    if (!s.put("STARTTLS\r\n") || !s.smtp(220) || !s.upgrade(a.outHost) ||
        !s.put("EHLO tab5.local\r\n"))
      return SendResult::rejected;
    caps.clear();
    if (!s.smtp(250, &caps))
      return SendResult::rejected;
  }
  if (!a.outUser.empty()) {
    if (caps.find("PLAIN") != caps.npos) {
      auto auth = base64(std::string(1, '\0') + a.outUser +
                         std::string(1, '\0') + a.outPassword);
      int response = 0;
      if (!s.put("AUTH PLAIN " + auth + "\r\n") ||
          !s.smtp(235, nullptr, &response, 334))
        return SendResult::rejected;
      if (response == 334 && (!s.put(auth + "\r\n") || !s.smtp(235)))
        return SendResult::rejected;
    } else if (caps.find("LOGIN") != caps.npos) {
      if (!s.put("AUTH LOGIN\r\n") || !s.smtp(334) ||
          !s.put(base64(a.outUser) + "\r\n") || !s.smtp(334) ||
          !s.put(base64(a.outPassword) + "\r\n") || !s.smtp(235))
        return SendResult::rejected;
    } else {
      error = "SMTP não oferece AUTH PLAIN/LOGIN; OAuth não implementado";
      return SendResult::rejected;
    }
  }
  if (!s.put("MAIL FROM:<" + a.address + ">\r\n") || !s.smtp(250))
    return SendResult::rejected;
  TextLines recipients(m.to);
  std::string recipient;
  unsigned count = 0;
  while (nextLine(recipients, recipient, ',')) {
    recipient = trim(recipient);
    if (!address(recipient) || ++count > 10) {
      error = "Destinatário inválido (até dez endereços)";
      return SendResult::rejected;
    }
    if (!s.put("RCPT TO:<" + recipient + ">\r\n") ||
        !s.smtp(250, nullptr, nullptr, 251, 252))
      return SendResult::rejected;
  }
  if (!count || !s.put("DATA\r\n") || !s.smtp(354))
    return SendResult::rejected;
  bool written=true;
  if(m.attachments.empty())written=s.put(encodeMessage(a,m));
  else {
    const std::string boundary="tab5-"+hex(m.id).substr(0,48);
    auto encoded=encodeMessage(a,m);auto headerEnd=encoded.find("MIME-Version:");
    if(headerEnd==encoded.npos || !reader)written=false;
    else {
      written=s.put(encoded.substr(0,headerEnd)+"MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=\""+boundary+"\"\r\n\r\n--"+boundary+"\r\n"+encoded.substr(headerEnd+strlen("MIME-Version: 1.0\r\n")));
      for(const auto &attachment:m.attachments){
        if(!written || cancel)break;
        std::string filename;for(unsigned char c:attachment.name){if(isalnum(c)||c=='.'||c=='-'||c=='_')filename+=char(c);else {const char *h="0123456789ABCDEF";filename+='%';filename+=h[c>>4];filename+=h[c&15];}}
        written=s.put("\r\n--"+boundary+"\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"attachment.bin\"; filename*=UTF-8''"+filename+"\r\nContent-Transfer-Encoding: base64\r\n\r\n");
        for(uint64_t offset=0;written && offset<attachment.size;){
          std::string bytes;
          if(!reader(attachment.path,offset,std::min<uint64_t>(3078,attachment.size-offset),bytes)||bytes.empty()||bytes.size()>attachment.size-offset){written=false;break;}
          offset+=bytes.size();auto b64=base64(bytes);std::string wrapped;
          for(size_t p=0;p<b64.size();p+=76)wrapped+=b64.substr(p,76)+"\r\n";
          written=s.put(wrapped);
        }
      }
      if(written&&!cancel)written=s.put("\r\n--"+boundary+"--\r\n");else written=false;
    }
  }
  if(written)written=s.put(".\r\n");
  if (!written) {
    error = "Envio incerto: conexão interrompida durante DATA; confira no "
            "servidor antes de reenviar";
    return SendResult::uncertain;
  }
  if (!s.smtp(250)) {
    if (error.find("SMTP recusou") != error.npos || error.rfind("SMTP 535:",0)==0)
      return SendResult::rejected;
    error =
        "Envio incerto: servidor não confirmou DATA; confira antes de reenviar";
    return SendResult::uncertain;
  }
  return SendResult::accepted;
}
} // namespace tab5::mail
