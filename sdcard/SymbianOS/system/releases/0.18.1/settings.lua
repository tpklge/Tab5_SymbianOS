-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local page,message="main",""
local function settingsEvent(action,value)
 if action=="appearance" or action=="security" or action=="language" or action=="alarm_tone" then page=action;message=""
 elseif action=="apply" then
  local f=store.lines(value or "")
  if page=="appearance" then
   local theme=f[1];local themes=sys.module("themes")
   if themes[theme] then
    local ok,why=sys.write("/config/public/builtin_theme",theme)
    if ok then ok=sys.write("/config/public/theme","") end
    if ok then ok=sys.power("brightness",tonumber(f[2]) or 100) and sys.power("screen_timeout",tonumber(f[3]) or 2) end
    message=ok and "Aparência e tela salvas." or tostring(why or "Falha ao salvar")
   end
  elseif page=="language" then
   if f[1]=="pt" or f[1]=="en" or f[1]=="es" then message=sys.write("/config/public/language",f[1]) and "Idioma salvo" or "Falha ao salvar" end
  elseif page=="alarm_tone" then
   if f[1]=="classic" or f[1]=="soft" or f[1]=="double" then message=sys.write("/config/public/alarm_tone",f[1]) and "Tom do alarme salvo" or "Falha ao salvar" end
  elseif page=="security" then
   if f[2]~=f[3] or not (f[2] or ""):match("^%d%d%d%d$") then message="Digite e confirme quatro números."
   else message=sys.security("set",f[1] or "",f[2]) and "PIN configurado. Use Bloquear agora ou aguarde a tela apagar." or "PIN atual incorreto ou espera ativa." end
  end
 elseif action=="test_tone" then message=sys.media("test") and "Operação concluída" or "Falha ao testar som"
 elseif action=="remove_pin" then
  local f=store.lines(value or "");message=sys.security("set",f[1] or "","") and "Bloqueio por PIN desativado." or "Informe o PIN atual."
 elseif action=="lock" then message=sys.security("lock") and "Bloqueando…" or "Configure um PIN primeiro."
 elseif action:sub(1,5)=="open:" then next_app=action:sub(6)
 elseif action=="back" then if page=="main" then next_app="shell" else page="main" end
 elseif action=="home" then next_app="shell" end
 if page=="language" then return chrome.view({title="Idioma",layout="form",field=true,text=sys.read("/config/public/language") or "pt",controls={{label="Idioma da interface",type="select",options="Português\nEnglish\nEspañol",keys="pt\nen\nes"}},buttons={{id="apply",label="Aplicar"}}})
 elseif page=="alarm_tone" then return chrome.view({title="Tom do alarme",layout="form",field=true,text=sys.read("/config/public/alarm_tone") or "classic",controls={{label="Tom",type="select",options="Clássico\nSuave\nDuplo",keys="classic\nsoft\ndouble"}},buttons={{id="apply",label="Aplicar"},{id="test_tone",label="Testar som do alarme",icon="ui_normal"}}})
 elseif page=="appearance" then
  return chrome.view({title="Aparência e tela",layout="form",field=true,
   body=message~="" and message or "Português brasileiro • orientação fixa em paisagem",
   text=(sys.theme().builtin or "aurora").."\n"..sys.power("brightness").."\n"..sys.power("screen_timeout"),
   controls={{label="Tema e papel de parede",type="select",options="Aurora • diagonais oceânicas\nSolar • horizonte quente\nPrisma • mosaico colorido",keys="aurora\nsolar\nprisma"},
    {label="Brilho",type="slider",options="10",keys="brightness"},
    {label="Apagar tela após",type="select",options="Nunca\n1 minuto\n2 minutos\n5 minutos\n10 minutos\n30 minutos",keys="0\n1\n2\n5\n10\n30"}},
   buttons={{id="apply",label="Aplicar"},{id="back",label="Voltar"}}})
 elseif page=="security" then
  return chrome.view({title="Bloqueio por PIN",layout="form",field=true,body=message~="" and message or "Bloqueia a interface; não criptografa o cartão.",text="\n\n",
   controls={{label="PIN atual (vazio no primeiro uso)",type="pin"},{label="Novo PIN • 4 dígitos",type="pin"},{label="Confirmar novo PIN",type="pin"}},
   buttons={{id="apply",label="Salvar PIN"},{id="remove_pin",label="Desativar PIN"},{id="lock",label="Bloquear agora"}}})
 end
 return chrome.view({title="Configurações",layout="list",body=message~="" and message or "Personalize seu espaço",
  buttons={{id="language",label="Idioma",icon="settings"},{id="alarm_tone",label="Tom do alarme",icon="clock"},{id="open:bench",label="Testes e medições",icon="memory"},{id="appearance",label="Aparência e tela",subtitle="Temas, papel de parede, brilho e tempo de tela",icon="profiles"},
   {id="security",label="Segurança",subtitle="PIN numérico de quatro dígitos",icon="lock"},
   {id="open:energy",label="Energia e som",icon="energy"},{id="open:clock",label="Data, hora e fuso",icon="clock"},
   {id="open:network",label="Wi-Fi e transferências",icon="network"},{id="lock",label="Bloquear agora",icon="lock"}}})
end

event=settingsEvent
return {event=settingsEvent}
