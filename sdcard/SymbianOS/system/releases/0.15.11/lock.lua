-- SPDX-License-Identifier: MIT
local chrome=sys.module("chrome")
local digits,message="",""
local cached=nil
function event(action)
 if action:sub(1,6)=="digit:" and #digits<4 then digits=digits..action:sub(7,7)
 elseif action=="erase" then digits=digits:sub(1,-2)
 elseif action=="unlock" then
  if #digits==4 and sys.security("unlock",digits) then digits="";next_app=sys.security("return")
  else digits="";message="PIN incorreto ou aguarde antes de tentar." end
 end
 local state=sys.security()
 if state.wait>0 then message="Aguarde "..state.wait.." segundos." elseif message:match("^Aguarde") then message="" end
 if not cached then
  local buttons={}
  for i=1,9 do buttons[#buttons+1]={id="digit:"..i,label=tostring(i)} end
  buttons[#buttons+1]={id="erase",label="Apagar"};buttons[#buttons+1]={id="digit:0",label="0"};buttons[#buttons+1]={id="unlock",label="Entrar"}
  cached=chrome.view({title="Bloqueado",layout="lock",buttons=buttons,refreshMs=1000})
 end
 cached.title=message~="" and message or "Bloqueado • PIN de quatro dígitos"
 cached.body=string.rep("• ",#digits)..string.rep("- ",4-#digits)
 return cached
end
