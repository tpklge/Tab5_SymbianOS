-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local page,section,selected,playlist,query=0,"library","","",""
local library,message={},""
local repeatMode=sys.read("/config/public/music_repeat") or "none"
local shuffle=sys.read("/config/public/music_shuffle")=="1"
local function scan()
 local result={}
 for offset=0,1020,12 do
  local files,total=sys.fs("list","/home/Music","",offset,"audio");if not files then break end
  for _,f in ipairs(files) do if not f.directory then result[#result+1]=f.path;if #result>=1024 then return result end end end
  if offset+12>=total then break end
 end
 return result
end
local function listPath() return "/data/music/list-"..playlist..".tsv" end
function event(action,value)
 local lists,why=store.read("/data/music/playlists.tsv")
 if not lists then return chrome.view({title="Playlists",body=tostring(why).." • arquivo preservado",buttons={}}) end
 if action=="focus" or action=="refresh" then library=scan();sys.media("mode",repeatMode,shuffle and 1 or 0) end
 if action=="volume_up" or action=="volume_down" then local v=sys.media("volume","",sys.media("volume")+(action=="volume_up" and 10 or -10));sys.write("/config/public/volume",tostring(v))
 elseif action=="playlists" then section="playlists";page=0
 elseif action=="library" then section="library";page=0
 elseif action=="new_list" then section="new"
 elseif action=="rename_list" then section="rename"
 elseif action=="search_form" then section="search"
 elseif action=="save_list" then
  local name=(value or ""):match("^%s*(.-)%s*$")
  if name~="" and #name<=80 and (#lists<32 or section=="rename") then local id=section=="rename" and playlist or store.id(lists);if section=="rename" then for _,row in ipairs(lists) do if row[1]==id then row[2]=name end end else lists[#lists+1]={id,name} end;local ok,why=store.write("/data/music/playlists.tsv",lists);message=store.saveStatus(ok,why);if ok then playlist=id;if section~="rename" then store.write(listPath(),{}) end;section="playlist" end else message="Nome até 80 bytes; máximo 32 playlists." end
 elseif action:sub(1,5)=="list:" then playlist=action:sub(6);section="playlist";page=0
 elseif action=="add_tracks" then section="add";page=0
 elseif action=="search" then query=(value or ""):lower();page=0;section="library"
 elseif action=="options" then section="options"
 elseif action=="save_options" then local f=store.lines(value or "");repeatMode=f[1] or "none";shuffle=f[2]=="1";sys.media("mode",repeatMode,shuffle and 1 or 0);sys.write("/config/public/music_repeat",repeatMode);sys.write("/config/public/music_shuffle",shuffle and "1" or "0");section="library"
 elseif action:sub(1,5)=="pick:" then
  selected=action:sub(6)
  local tracks=store.read(listPath()) or {};local duplicate=false;for _,r in ipairs(tracks) do if r[1]==selected then duplicate=true end end
  if not duplicate and #tracks<256 then tracks[#tracks+1]={selected};local ok,why=store.write(listPath(),tracks);message=store.saveStatus(ok,why) else message="Faixa já adicionada ou limite de 256." end
 elseif action:sub(1,7)=="select:" then selected=action:sub(8)
 elseif action=="remove_track" or action=="move_up" or action=="move_down" then
  local tracks=store.read(listPath()) or {};for i,r in ipairs(tracks) do if r[1]==selected then
   if action=="remove_track" then table.remove(tracks,i)
   else local n=action=="move_up" and i-1 or i+1;if n>=1 and n<=#tracks then tracks[n],tracks[i]=tracks[i],tracks[n] end end;break end end
  local ok,why=store.write(listPath(),tracks);message=store.saveStatus(ok,why)
 elseif action=="delete_list" then section="delete"
 elseif action=="confirm_delete" then
  for i=#lists,1,-1 do if lists[i][1]==playlist then table.remove(lists,i) end end
  local ok,why=store.write("/data/music/playlists.tsv",lists);if ok then sys.fs("remove",listPath());section="playlists" end;message=store.saveStatus(ok,why)
 elseif action=="page_next" then page=page+1 elseif action=="page_prev" then page=math.max(0,page-1)
 elseif action=="back" then if section=="library" then next_app="shell" elseif section=="add" then section="playlist" else section="library" end
 elseif action=="home" then next_app="shell"
 end
 local all={}
 if section=="playlist" then for _,r in ipairs(store.read(listPath()) or {}) do all[#all+1]=r[1] end
 else for _,path in ipairs(library) do if query=="" or path:lower():find(query,1,true) then all[#all+1]=path end end end
 if action:sub(1,5)=="play:" or action=="play_list" then
  selected=action=="play_list" and (all[1] or "") or action:sub(6)
  local start=1;for i,path in ipairs(all) do if path==selected then start=i end end
  -- Keep a bounded native queue while allowing a larger, paginated library.
  local queue={};local first=math.max(1,math.min(start,#all-255));for i=first,math.min(first+255,#all) do queue[#queue+1]=all[i] end
  local ok,why=sys.media("queue",table.concat(queue,"\n"),start-first+1);message=ok and "" or tostring(why)
 elseif action=="toggle" then local s=sys.media("status");if s:match("^playing") or s:match("^paused") then sys.media("pause") else if #all>0 then local q={};for i=1,math.min(256,#all) do q[i]=all[i] end;sys.media("queue",table.concat(q,"\n")) end end
 elseif action=="stop" or action=="next_track" or action=="prev_track" then sys.media(action) end
 if section=="search" then return chrome.view({title="Buscar música",layout="editor",field=true,text=query,buttons={{id="search",label="Buscar"}}}) end
 if section=="new" or section=="rename" then return chrome.view({title=section=="rename" and "Renomear playlist" or "Nova playlist",layout="editor",field=true,text="",body=message,buttons={{id="save_list",label=section=="rename" and "Salvar nome" or "Criar playlist"}}}) end
 if section=="delete" then return chrome.view({title="Excluir playlist?",layout="list",body="As músicas serão preservadas.",buttons={{id="confirm_delete",label="Excluir playlist"},{id="back",label="Cancelar"}}}) end
 if section=="options" then return chrome.view({title="Opções de música",layout="form",field=true,text=repeatMode.."\n"..(shuffle and "1" or "0"),controls={{label="Repetição",type="select",options="Desligada\nToda a fila\nUma faixa",keys="none\nall\none"},{label="Ordem",type="select",options="Normal\nAleatória",keys="0\n1"}},buttons={{id="save_options",label="Aplicar"}}}) end
 if section=="playlists" then local b={};for i=page*6+1,math.min(page*6+6,#lists) do b[#b+1]={id="list:"..lists[i][1],label=lists[i][2],icon="music"} end;b[#b+1]={id="new_list",label="Nova playlist"};b[#b+1]={id="library",label="Biblioteca"};b[#b+1]={id="page_prev",label="Anterior"};b[#b+1]={id="page_next",label="Próxima"};return chrome.view({title="Playlists",layout="list",body=message,buttons=b}) end
 page=math.min(page,math.max(0,math.floor((#all-1)/6)))
 local b={};for i=page*6+1,math.min(page*6+6,#all) do b[#b+1]={id=(section=="add" and "pick:" or section=="playlist" and "select:" or "play:")..all[i],label=(all[i]==selected and "• " or "")..all[i]:match("[^/]+$"),icon="music"} end
 if section=="playlist" then
  for _,pair in ipairs({{"play_list","Reproduzir playlist"},{"add_tracks","Adicionar faixas"},{"remove_track","Remover selecionada"},{"move_up","Mover acima"},{"move_down","Mover abaixo"},{"rename_list","Renomear playlist"},{"delete_list","Excluir playlist"}}) do b[#b+1]={id=pair[1],label=pair[2]} end
 else b[#b+1]={id="search_form",label="Buscar música"};b[#b+1]={id="playlists",label="Playlists"};b[#b+1]={id="options",label="Repetição / aleatório"} end
 return chrome.view({title=section=="playlist" and "Playlist" or section=="add" and "Adicionar faixas" or "Música",layout="player",mediaStatus=true,body=message~="" and message or (#all.." faixas • "..sys.media("status")),buttons=b,
  tools={{id="volume_down",label="Vol -",icon="ui_volume_down"},{id="volume_up",label="Vol +",icon="ui_volume_up"},{id="prev_track",label="Anterior",icon="ui_prev"},{id="toggle",label="Tocar",icon="ui_play"},{id="stop",label="Parar",icon="ui_stop"},{id="next_track",label="Próxima",icon="ui_next"},{id="page_prev",label="Página -",icon="ui_prev"},{id="page_next",label="Página +",icon="ui_next"}}})
end
