-- SPDX-License-Identifier: MIT
local appearance=sys.module("settings")
local chrome,store=sys.module("chrome"),sys.module("store")
local page,message,pending,subpage="main","",nil,nil
function event(action,value)
 if action=="appearance" or action=="security" or action=="language" or action=="alarm_tone" then subpage=action end
 if subpage then
  local view=appearance.event(action,value)
  if action~="back" and action~="home" then return view end
  if action=="home" then return view end
  subpage=nil;page="main";action="refresh"
 end
 if action=="screen" or action=="charge" or action=="sound" or action=="sleep" then page=action
 elseif action:sub(1,5)=="open:" then next_app=action:sub(6)
 elseif action:sub(1,8)=="timeout:" then sys.power("screen_timeout",tonumber(action:sub(9)))
 elseif action=="apply" then
  local f=store.lines(value or "")
  if page=="screen" then message=(sys.power("screen_timeout",tonumber(f[1])) and sys.power("brightness",tonumber(f[2]))) and "Tela configurada." or "Falha ao salvar."
  elseif page=="sound" then local v=sys.media("volume","",tonumber(f[1]) or 60);sys.write("/config/public/volume",tostring(v));message="Volume salvo." end
 elseif action:sub(1,6)=="sleep:" then pending=tonumber(action:sub(7))
 elseif action=="confirm" and pending then local ok,why=sys.power("sleep",pending);pending=nil;message=why or "Retomado."
 elseif action=="cancel" then pending=nil
 elseif action=="screen_off" then sys.power("screen_off")
 elseif action=="fast" or action=="normal" or action=="off" then
  if sys.power(action) then sys.write("/config/public/charge",action);message="Modo aplicado." else message="Falha ao aplicar carga." end
 elseif action=="back" then if page=="main" then next_app="shell" else page="main" end
 elseif action=="home" then next_app="shell" end
 if pending then return chrome.view({title="Suspender por "..pending.." minutos?",layout="status",body="Música e rede param; a retomada é temporizada no próprio runtime.",buttons={{id="confirm",label="OK",icon="ui_sleep"},{id="cancel",label="Cancelar"}}}) end
 local p=sys.power();local battery=p.available and (p.percent.."% • "..p.millivolts.." mV") or "Indisponível"
 if page=="charge" then
  local current=sys.read("/config/public/charge") or "normal"
  return chrome.view({title="Carregamento",layout="options",body=message~="" and message or "Escolha uma opção",refreshMs=1000,statusFields={{label="Bateria",value=battery},{label="Corrente medida",value=p.available and p.milliamps.." mA" or "Indisponível"}},
   buttons={{id="normal",label="Normal",icon="ui_charge",subtitle=current=="normal" and "selected" or ""},{id="fast",label="Rápido / QC",icon="ui_fast",subtitle=current=="fast" and "selected" or ""},{id="off",label="Pausado",icon="ui_stop",subtitle=current=="off" and "selected" or ""}}})
 elseif page=="sleep" then return chrome.view({title="Suspender",layout="choices",body="Selecione o tempo de suspensão",buttons={{id="sleep:1",label="1 minuto",icon="ui_sleep"},{id="sleep:5",label="5 minutos",icon="ui_sleep"},{id="sleep:15",label="15 minutos",icon="ui_sleep"}}}) end
 local controls,text
 if page=="screen" then controls={{label="Apagar tela após",type="select",options="Nunca\n1 minuto\n2 minutos\n5 minutos\n10 minutos\n30 minutos",keys="0\n1\n2\n5\n10\n30"},{label="Brilho",type="slider",options="10",keys="brightness"}};text=sys.power("screen_timeout").."\n"..sys.power("brightness")
 elseif page=="sound" then controls={{label="Volume",type="slider",keys="volume"}};text=tostring(sys.media("volume")) end
 if controls then return chrome.view({title=page=="screen" and "Tela e brilho" or "Som",layout="form",field=true,text=text,controls=controls,body=message,buttons={{id="apply",label="OK"}}}) end
 return chrome.view({title="Ajustes",layout="list",columns=2,refreshMs=1000,body=message,
  statusFields={{label="Bateria",value=battery},{label="Alimentação",value=p.charging and "Carregando" or "Em uso"}},
  buttons={{id="language",label="Idioma",icon="settings"},{id="alarm_tone",label="Tom do alarme",icon="clock"},{id="open:bench",label="Testes e medições",icon="memory"},{id="screen",label="Tela",icon="ui_brightness"},{id="appearance",label="Temas",icon="themes"},{id="security",label="Bloqueio por PIN",icon="lock"},
   {id="sound",label="Som",icon="ui_normal"},{id="charge",label="Carregamento",icon="ui_charge"},{id="sleep",label="Suspender",icon="ui_sleep"},
   {id="open:profiles",label="Perfis",icon="ui_silent"},{id="open:network",label="Wi-Fi",icon="network"},{id="open:memory",label="Memória",icon="ui_memory"}},
  footer={{id="screen_off",label="Apagar tela",icon="lock"}}})
end
