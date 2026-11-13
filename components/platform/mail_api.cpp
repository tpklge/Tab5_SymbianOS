// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include <climits>
#include <cstring>
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-__LONG_LONG_MAX__ - 1LL)
#define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)
#endif
extern "C" {
#include "lauxlib.h"
#include "lua.h"
}
static void str(lua_State *L, const char *key, const std::string &s) {
  lua_pushlstring(L, s.data(), s.size());
  lua_setfield(L, -2, key);
}

namespace tab5 {
int Runtime::mailApi(lua_State *L) {
  auto *r = static_cast<Runtime *>(lua_touserdata(L, lua_upvalueindex(1)));
  if (!r->capability("mail"))
    return luaL_error(L, "mail capability denied");
  std::string op = luaL_checkstring(L, 1);
  auto arg = [&](int i, size_t cap = 65536) {
    size_t n = 0;
    const char *v = luaL_optlstring(L, i, "", &n);
    if (n > cap || memchr(v, 0, n))
      luaL_error(L, "mail argument too large/invalid");
    return std::string(v, n);
  };
  if (op == "status") {
    auto s = r->mail_.status();
    lua_newtable(L);
    lua_pushboolean(L, s.busy);
    lua_setfield(L, -2, "busy");
    str(L, "message", s.message);
    lua_pushboolean(L,true);lua_setfield(L,-2,"attachments_supported");
    lua_pushboolean(L,true);lua_setfield(L,-2,"html_supported");
    auto network = r->network_.status();
    lua_pushboolean(L,network.busy);lua_setfield(L,-2,"images_busy");
    str(L,"images_message",network.message);
    lua_pushboolean(L,network.restartRequired);lua_setfield(L,-2,"restart_required");
    str(L,"network_message",network.message);
    str(L, "diagnostic",
        "runtime=1.0.0\nRelogio UI=" + std::to_string(r->now()) +
            "\nRelogio POSIX=" + std::to_string(time(nullptr)) +
            "\nRede=" + network.message + "\n" + s.message);
    lua_pushinteger(L, s.queued);
    lua_setfield(L, -2, "queued");
    return 1;
  }
  if (op == "account") {
    auto a = r->mail_.account();
    lua_newtable(L);
    for (auto pair : {std::make_pair("address", a.address),
                      {"name", a.name},
                      {"protocol", a.protocol},
                      {"inHost", a.inHost},
                      {"inPort", std::to_string(a.inPort)},
                      {"inSecurity", a.inSecurity},
                      {"inUser", a.inUser},
                      {"inPassword", a.inPassword},
                      {"outHost", a.outHost},
                      {"outPort", std::to_string(a.outPort)},
                      {"outSecurity", a.outSecurity},
                      {"outUser", a.outUser},
                      {"outPassword", a.outPassword}})
      str(L, pair.first, pair.second);
    lua_pushboolean(L, a.automatic);
    lua_setfield(L, -2, "automatic");
    return 1;
  }
  auto message = [&](const MailMessage &m, bool body) {
    lua_newtable(L);
    for (auto pair : {std::make_pair("id", m.id),
                      {"from", m.from},
                      {"to", m.to},
                      {"subject", m.subject},
                      {"date", m.date},
                      {"folder", m.folder},
                      {"status", m.status}})
      str(L, pair.first, pair.second);
    lua_pushboolean(L, m.seen);
    lua_setfield(L, -2, "seen");
    if (body) {
      str(L, "body", m.body);
      lua_newtable(L);int number=0;for(const auto &a:m.attachments){lua_newtable(L);str(L,"name",a.name);str(L,"type",a.type);lua_pushinteger(L,a.size);lua_setfield(L,-2,"bytes");lua_rawseti(L,-2,++number);}lua_setfield(L,-2,"attachments");
      lua_pushboolean(L,!m.html.empty());lua_setfield(L,-2,"has_html");
    }
  };
  if (op == "list") {
    auto all = r->mail_.list(arg(2, 220));
    lua_newtable(L);
    int i = 1;
    for (auto &m : all) {
      message(m, false);
      lua_rawseti(L, -2, i++);
    }
    return 1;
  }
  if(op=="files") {
    auto path=arg(2,240);std::vector<File> files;
    if((path!="/home"&&path.rfind("/home/",0)!=0)||!Storage::validPath(path)||!r->store_.listDir(path,files)){lua_pushnil(L);return 1;}
    lua_newtable(L);unsigned count=0;
    for(const auto &f:files){if(f.path.substr(f.path.find_last_of('/')+1)==".gitkeep")continue;if(count>=128)break;lua_newtable(L);str(L,"path",f.path);lua_pushboolean(L,f.directory);lua_setfield(L,-2,"directory");lua_pushinteger(L,f.size);lua_setfield(L,-2,"bytes");lua_rawseti(L,-2,++count);}return 1;
  }
  if (op == "folders") {
    auto all = r->mail_.folders();
    lua_newtable(L);
    int i = 1;
    for (auto &f : all) {
      lua_pushlstring(L, f.data(), f.size());
      lua_rawseti(L, -2, i++);
    }
    return 1;
  }
  if (op == "read") {
    MailMessage m;
    if (!r->readEventMail(arg(2, 24), m)) {
      lua_pushnil(L);
      return 1;
    }
    message(m, true);
    return 1;
  }
  std::string error;
  bool ok = false;
  if (op == "configure") {
    luaL_checktype(L, 2, LUA_TTABLE);
    auto f = [&](const char *key) {
      lua_getfield(L, 2, key);
      size_t n = 0;
      const char *v = lua_tolstring(L, -1, &n);
      std::string result = v && n <= 256 ? std::string(v, n) : "";
      lua_pop(L, 1);
      return result;
    };
    MailAccount a;
    a.address = f("address");
    a.name = f("name");
    a.protocol = f("protocol");
    a.inHost = f("inHost");
    auto port = [&](const char *key) {
      auto text = f(key);
      if (text.empty() || text.size() > 5 ||
          text.find_first_not_of("0123456789") != text.npos)
        return 0u;
      return unsigned(strtoul(text.c_str(), nullptr, 10));
    };
    a.inPort = port("inPort");
    a.inSecurity = f("inSecurity");
    a.inUser = f("inUser");
    a.inPassword = f("inPassword");
    a.outHost = f("outHost");
    a.outPort = port("outPort");
    a.outSecurity = f("outSecurity");
    a.outUser = f("outUser");
    a.outPassword = f("outPassword");
    lua_getfield(L, 2, "automatic");
    a.automatic = lua_toboolean(L, -1);
    lua_pop(L, 1);
    ok = r->mail_.configure(a, error);
  } else if (op == "draft" || op == "queue") {
    MailMessage m;
    m.id = arg(2, 24);
    m.to = arg(3, 1024);
    m.subject = arg(4, 200);
    m.body = arg(5);
    ok = r->mail_.draft(m, op == "queue", error);
    if (ok) {
      lua_pushlstring(L, m.id.data(), m.id.size());
      return 1;
    }
  } else if (op == "move")
    ok = r->mail_.move(arg(2, 24), arg(3, 16), error);
  else if (op == "cancel") {
    r->mail_.cancel();
    ok = true;
  } else if (op == "recover") {
    std::string profile;
    r->store_.read("/config/public/profile", profile);
    if (profile == "offline")
      error = "Saia do perfil offline para recuperar Wi-Fi";
    else {
      r->mail_.cancel();
      ok = r->network_.recover();
      if (!ok)
        error = r->network_.status().restartRequired ? r->network_.status().message : "Rede ocupada ou sem conexão anterior; use o aplicativo Wi-Fi";
    }
  } else if(op=="attach") {
    ok=r->mail_.attach(arg(2,24),arg(3,240),error);
  } else if(op=="detach"||op=="export") {
    unsigned index=luaL_checkinteger(L,3);if(!index||index>8)error="Anexo inválido";
    else ok=op=="detach"?r->mail_.detach(arg(2,24),index-1,error):r->mail_.exportAttachment(arg(2,24),index-1,error);
  } else if (op == "reload") {
    std::string profile;r->store_.read("/config/public/profile",profile);
    if(profile=="offline" || !r->network_.status().connected)error="Conecte o Wi-Fi para baixar novamente";
    else ok=r->mail_.refresh(arg(2,24),error);
  } else if (op == "images") {
    MailMessage m;std::string profile;r->store_.read("/config/public/profile",profile);
    if(profile=="offline")error="Saia do perfil offline para carregar imagens";
    else if(!r->mail_.read(arg(2,24),m))error="Mensagem indisponível";
    else {ok=r->network_.images(m.html,"");if(!ok)error="Sem imagens externas, rede ocupada ou desconectada";}
  } else if (op == "sync" || op == "flush") {
    std::string profile;
    r->store_.read("/config/public/profile", profile);
    if (r->network_.status().restartRequired)
      error = r->network_.status().message;
    else if (profile == "offline" || !r->network_.status().connected)
      error = "Conecte o Wi-Fi para sincronizar/enviar";
    else if (r->now() < 1700000000)
      error = "Ajuste data e hora antes de usar TLS";
    else
      ok = op == "sync" ? r->mail_.synchronize(arg(2, 200), error)
                        : r->mail_.flush(error,true);
  }
  lua_pushboolean(L, ok);
  if (!ok) {
    lua_pushlstring(L, error.data(), error.size());
    return 2;
  }
  return 1;
}
} // namespace tab5
