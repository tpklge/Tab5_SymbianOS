-- SPDX-License-Identifier: MIT
local M={};local catalogs={}
local prefixes={"E-mail · ","Agenda • ","Artigos • ","Aplicativos • ","Arquivos • ","Soneca 5 min: ","Senha ","Usuário ","Atalho ","Dispensar ","Suspender por ","Canal ","Segunda ","Terça ","Quarta ","Quinta ","Sexta ","Sábado ","Domingo "}
function M.apply(v,language,appId)
 v.language=language or "pt"
 if language~="en" and language~="es" then return v end
 local dictionary=catalogs[language]
 if not dictionary then dictionary=sys.module("lang_"..language);catalogs[language]=dictionary end
 local function tr(s)
  if type(s)~="string" then return s end
  if dictionary[s] then return dictionary[s] end
  local count,unit=s:match("^(%d+) (.+)$");if count and dictionary[unit] then return count.." "..dictionary[unit] end
  for _,separator in ipairs({" · "," • "}) do
   local p=s:find(separator,1,true)
   if p then return tr(s:sub(1,p-1))..separator..tr(s:sub(p+#separator)) end
  end
  if s:find("\n",1,true) then return (s:gsub("[^\n]+",function(line)return dictionary[line] or line end)) end
  for _,prefix in ipairs(prefixes) do if s:sub(1,#prefix)==prefix then return (dictionary[prefix] or (dictionary[prefix:match("^(.-)%s*$")] and dictionary[prefix:match("^(.-)%s*$")].." ") or prefix)..s:sub(#prefix+1) end end
  return s
 end
 if v.titleUserContent and v.titlePrefix then v.title=(dictionary[v.titlePrefix] or v.titlePrefix)..v.title:sub(#v.titlePrefix+1) end
 for _,key in ipairs({"title","body","left","right","home","menu","switcher","keyboard","fields"}) do if not (key=="title" and (v.titleUserContent or appId=="feeds" and (v.layout=="html" or v.layout=="reader" or v.layout=="web_document"))) then v[key]=tr(v[key]) end end
 for _,list in ipairs({v.buttons,v.tools,v.footer,v.sidebar}) do
  for _,b in ipairs(list or {}) do
   local id=b.id or ""
   local user=(appId=="notes" and id:match("^open:")) or id:match("^edit:") or id:match("^file:") or id:match("^dir:") or id:match("^article:") or id:match("^saved:") or id:match("^export:") or id:match("^detach:") or id:match("^fileattach:") or id:match("^filedir:") or id:match("^note:") or id:match("^node:") or id:match("^open:[a-f0-9]+$")
   local system=appId=="shell" or appId=="settings" or appId=="energy" or appId=="agenda" and id:match("^day:") or id=="folder:inbox" or id=="folder:drafts" or id=="folder:outbox" or id=="folder:sent" or id=="folder:trash"
   if id:find(":",1,true) and not system then user=true end
   if not user then b.label=tr(b.label);b.subtitle=tr(b.subtitle) end
  end
 end
 for _,c in ipairs(v.controls or {}) do
  c.label=tr(c.label)
  if c.type=="select" and c.options then c.keys=c.keys or c.options;c.options=tr(c.options) end
 end
 for _,field in ipairs(v.statusFields or {}) do
  if field.label=="Conexão" or field.label=="Conexão automática" or field.label=="Hardware" or field.label=="Origem das medições" or field.label=="Registro no cartão" or field.value=="Sua agenda está livre." or field.value=="Você está em dia." then field.value=tr(field.value) end
  field.label=tr(field.label)
 end
 if appId=="updates" then v.text=tr(v.text) end
 v.profileLabel=tr(v.profile=="silent" and "Silencioso" or v.profile=="offline" and "Offline" or "Normal")
 v.optionLabel=tr("Ajustes")
 return v
end
function M.text(s,language)return M.apply({title=s},language).title end
return M
