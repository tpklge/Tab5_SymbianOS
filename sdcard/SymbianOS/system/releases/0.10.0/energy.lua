-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local page,message,pending="main","",nil
function event(action,value)
 if action=="screen" or action=="charge" or action=="sound" or action=="sleep" then page=action
 elseif action:sub(1,8)=="timeout:" then sys.power("screen_timeout",tonumber(action:sub(9)))
 elseif action=="apply" then
  local f=store.lines(value or "")
  if page=="screen" then message=(sys.power("screen_timeout",tonumber(f[1])) and sys.power("brightness",tonumber(f[2]))) and "Tela configurada." or "Falha ao salvar."
  elseif page=="charge" then
   if sys.power(f[1]) then sys.write("/config/public/charge",f[1]);message="Modo de carga aplicado." else message="Não foi possível aplicar." end
  elseif page=="sound" then local v=sys.media("volume","",tonumber(f[1]) or 60);sys.write("/config/public/volume",tostring(v));message="Volume salvo."
  elseif page=="sleep" then pending=tonumber(f[1]) end
 elseif action:sub(1,6)=="sleep:" then pending=tonumber(action:sub(7))
 elseif action=="confirm" and pending then local ok,why=sys.power("sleep",pending);pending=nil;message=why or "Retomado."
 elseif action=="cancel" then pending=nil
 elseif action=="screen_off" then sys.power("screen_off")
 elseif action=="fast" or action=="normal" or action=="off" then
  if sys.power(action) then sys.write("/config/public/charge",action);message="Modo aplicado." end
 elseif action=="back" then if page=="main" then next_app="shell" else page="main" end
 elseif action=="home" then next_app="shell" end
 if pending then return chrome.view({title="Suspender",layout="list",body="Retoma no próprio Symbian após "..pending.." min. Música e rede param; alarmes são verificados na retomada.",buttons={{id="confirm",label="Confirmar suspensão"},{id="cancel",label="Cancelar"}}}) end
 local p=sys.power()
 local battery=p.available and (p.percent.."% • "..p.millivolts.." mV • "..p.milliamps.." mA • "..(p.charging and "Carregando" or "Em uso")) or "Monitor de bateria indisponível"
 local controls,text
 if page=="screen" then
  controls={{label="Apagar tela após",type="select",options="Nunca\n1 minuto\n2 minutos\n5 minutos\n10 minutos\n30 minutos",keys="0\n1\n2\n5\n10\n30"},
   {label="Brilho",type="select",options="10%\n25%\n50%\n75%\n100%",keys="10\n25\n50\n75\n100"}}
  text=sys.power("screen_timeout").."\n"..sys.power("brightness")
 elseif page=="charge" then controls={{label="Modo de carregamento",type="select",options="Normal\nRápido / QC\nPausado",keys="normal\nfast\noff"}};text=sys.read("/config/public/charge") or "normal"
 elseif page=="sound" then controls={{label="Volume geral",type="select",options="0%\n10%\n20%\n30%\n40%\n50%\n60%\n70%\n80%\n90%\n100%",keys="0\n10\n20\n30\n40\n50\n60\n70\n80\n90\n100"}};text=tostring(sys.media("volume"))
 elseif page=="sleep" then controls={{label="Tempo de suspensão",type="select",options="1 minuto\n5 minutos\n15 minutos",keys="1\n5\n15"}};text="1" end
 if controls then return chrome.view({title="Energia • "..({screen="Tela",charge="Carga",sound="Som",sleep="Suspensão"})[page],layout="form",field=true,text=text,controls=controls,body=message~="" and message or battery,buttons={{id="apply",label=page=="sleep" and "Continuar" or "Aplicar"},{id="back",label="Voltar"}}}) end
 return chrome.view({title="Energia e volume",layout="list",body=message~="" and message or battery,
  buttons={{id="screen",label="Tela",subtitle="Brilho e tempo até apagar",icon="profiles"},{id="charge",label="Carregamento",subtitle="Normal, QC ou pausado",icon="energy"},
   {id="sound",label="Som",subtitle="Volume: "..sys.media("volume").."%",icon="music"},{id="sleep",label="Suspender",subtitle="Temporizador de 1, 5 ou 15 minutos",icon="clock"},{id="screen_off",label="Apagar tela agora",icon="lock"}}})
end
