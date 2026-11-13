-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local M={}
local folder,mode,sort,section="all","grid","manual","apps"
local preferences=store.read("/data/shell/preferences.tsv") or {}
if preferences[1] then folder,mode,sort=preferences[1][1],preferences[1][2],preferences[1][3] end
local defaults={music="media",recorder="media",gallery="media",camera="media",notes="personal",contacts="personal",agenda="personal",feeds="personal"}
local folders={all="Todos",personal="Pessoal",media="Multimídia",tools="Ferramentas",favorites="Favoritos"}
local page,message=0,""
function M.view(where,action,value,registry)
 local rows=store.read("/data/shell/menu.tsv") or {};local config={}
 for _,r in ipairs(rows) do config[r[1]]=r end
 local list={};local labels,keys={},{}
 for i,app in ipairs(registry) do
  local r=config[app[1]] or {app[1],defaults[app[1]] or "tools",(app[1]=="notes" or app[1]=="music" or app[1]=="agenda") and "1" or "0",tostring(i)}
  config[app[1]]=r;list[#list+1]={id=app[1],name=app[2],folder=r[2],favorite=r[3]=="1",order=tonumber(r[4]) or i}
  labels[#labels+1]=app[2];keys[#keys+1]=app[1]
 end
 if where=="home" then
  local buttons={{id="menu",label="Menu",icon="menu"}}
  for _,app in ipairs(list) do if app.favorite and #buttons<4 then buttons[#buttons+1]={id="open:"..app.id,label=app.name,icon=app.id=="energy" and "settings" or app.id} end end
  buttons[#buttons+1]={id="open:energy",label="Ajustes",icon="settings"}
  buttons[#buttons+1]={id="notices",label="Avisos",icon="clock"}
  local upcoming={};for _,r in ipairs(store.read("/data/organizer/events.tsv") or {}) do if tonumber(r[3]) and tonumber(r[3])>=sys.epoch() and r[6]~="1" then upcoming[#upcoming+1]=r end end
  table.sort(upcoming,function(a,b)return tonumber(a[3])<tonumber(b[3]) end)
  local summary={"PRÓXIMOS COMPROMISSOS",""}
  for i=1,math.min(3,#upcoming) do local r=upcoming[i];local stamp=sys.date(tonumber(r[3]));summary[#summary+1]=stamp:sub(9,10).."/"..stamp:sub(6,7).." "..stamp:sub(12,16).."  "..store.clip(r[2],32) end
  if #upcoming==0 then summary[#summary+1]="Sua agenda está livre." end
  summary[#summary+1]="";summary[#summary+1]=#sys.notices().." notificações • "..(sys.read("/config/public/profile") or "normal")
  return chrome.view({title="Pessoal",layout="home",body="Seu dia, seus atalhos",text=table.concat(summary,"\n"),buttons=buttons})
 end
 if action=="menu" then section="apps"
 elseif action=="menu_next" then page=page+1 elseif action=="menu_prev" then page=math.max(0,page-1)
 elseif action=="folders" then section="folders"
 elseif action:sub(1,7)=="folder:" then folder=action:sub(8);section="apps";page=0
 elseif action=="menu_mode" then mode=mode=="grid" and "list" or "grid";page=0
 elseif action=="organize" then section="organize"
 elseif action=="menu_save" then
  local f=store.lines(value or "");local r=config[f[1]]
  local position=tonumber(f[4]);if r and folders[f[2]] and f[2]~="all" and f[2]~="favorites" and (f[3]=="0" or f[3]=="1") and position and position>=1 and position<=1000 then
   r[2],r[3],r[4]=f[2],f[3],tostring(math.floor(position));rows={};for _,app in ipairs(registry) do rows[#rows+1]=config[app[1]] end
   local ok,why=store.write("/data/shell/menu.tsv",rows);message=store.saveStatus(ok,why)
   mode=f[5]=="list" and "list" or "grid";sort=f[6]=="alpha" and "alpha" or "manual"
   if ok then section="apps";return M.view(where,"refresh","",registry) end
  else message="Selecione aplicativo, pasta e posição válida." end
 elseif action=="back" then section="apps" end
 store.write("/data/shell/preferences.tsv",{{folder,mode,sort}})
 if section=="folders" then
  local buttons={};for _,id in ipairs({"all","favorites","personal","media","tools"}) do buttons[#buttons+1]={id="folder:"..id,label=folders[id],icon=id=="favorites" and "profiles" or "files"} end
  return chrome.view({title="Pastas do menu",layout="list",buttons=buttons,body="Escolha uma pasta"})
 elseif section=="organize" then
  local current=list[1].id;local r=config[current]
  return chrome.view({title="Organizar menu",layout="form",field=true,body=message~="" and message or "Escolha o aplicativo e salve sua pasta, favorito e posição.",text=current.."\n"..r[2].."\n"..r[3].."\n"..r[4].."\n"..mode.."\n"..sort,
   controls={{label="Aplicativo",type="select",options=table.concat(labels,"\n"),keys=table.concat(keys,"\n")},
    {label="Pasta",type="select",options="Pessoal\nMultimídia\nFerramentas",keys="personal\nmedia\ntools"},
    {label="Atalho na Home / favorito",type="select",options="Não\nSim",keys="0\n1"},{label="Posição no menu",type="text"},
    {label="Visualização",type="select",options="Grade\nLista",keys="grid\nlist"},{label="Ordenação",type="select",options="Minha ordem\nAlfabética",keys="manual\nalpha"}},buttons={{id="menu_save",label="Salvar organização"},{id="menu",label="Voltar ao menu"}}})
 end
 table.sort(list,function(a,b)if sort=="alpha" then return a.name:lower()<b.name:lower() end;if a.order==b.order then return a.name<b.name end;return a.order<b.order end)
 local matches={};for _,app in ipairs(list) do if folder=="all" or folder==app.folder or folder=="favorites" and app.favorite then matches[#matches+1]=app end end
 page=math.min(page,math.max(0,math.floor((#matches-1)/9)))
 local buttons={};for i=page*9+1,math.min(page*9+9,#matches) do local app=matches[i];buttons[#buttons+1]={id="open:"..app.id,label=app.name,subtitle=mode=="list" and (app.favorite and "Favorito" or folders[app.folder]) or nil,icon=app.id=="energy" and "settings" or app.id} end
 return chrome.view({title="Aplicativos • "..(folders[folder] or "Todos"),layout=mode,classic=mode=="grid",body=message~="" and message or "Página "..(page+1).." • "..#matches.." aplicativos",buttons=buttons,
  tools={{id="menu_prev",label="Anterior",icon="ui_prev"},{id="menu_next",label="Próxima",icon="ui_next"},{id="folders",label="Pastas",icon="files"},{id="menu_mode",label=mode=="grid" and "Lista" or "Grade",icon="menu"},{id="organize",label="Organizar",icon="settings"}}})
end
return M
