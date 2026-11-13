-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local path,status,page="/backups/installation.t5bak","","main"
local session=store.resume("backup");if session then path=session[2] end
function event(action,value)
 if value and value:match("^/backups/") then path=value end
 if action=="suspend" or action=="checkpoint" then local ok,why=store.session("backup",page,path);if not ok then error(why) end
 elseif action=="create" and value and value:match("^/backups/") or action=="create_backup" then
  local ok,why=sys.fs("backup",path);status=ok and "Backup verificado criado." or tostring(why);page="main"
 elseif action=="create" then page="create"
 elseif action=="restore" then page="confirm";status="Restaurar este backup? Arquivos de mesmo nome serão substituídos."
 elseif action=="browse" then page="browse"
 elseif action:sub(1,5)=="file:" then path=action:sub(6);page="main"
 elseif action=="confirm_restore" then local ok,why=sys.fs("restore",path);status=ok and "Restauração concluída. Reinicie para recarregar os serviços." or tostring(why);page="main"
 elseif action=="cancel" then page="main"
 elseif action=="back" then if page=="main" then next_app="shell" else page="main" end
 elseif action=="home" then next_app="shell" end
 if page=="create" then return chrome.view({title="Criar backup",layout="form",field=true,text=path,controls={{label="Arquivo de destino",type="text"}},body="Todo o snapshot conta na quota do SD.",buttons={{id="create_backup",label="OK",icon="ui_backup"},{id="cancel",label="Cancelar"}}}) end
 if page=="confirm" then return chrome.view({title="Restaurar backup",layout="status",body=status,statusFields={{label="Arquivo selecionado",value=path}},buttons={{id="confirm_restore",label="Restaurar",icon="ui_restore"},{id="cancel",label="Cancelar"}}}) end
 if page=="browse" then
  local b={};for _,f in ipairs(sys.fs("list","/backups") or {}) do if not f.directory and f.path:match("%.t5bak$") then b[#b+1]={id="file:"..f.path,label=f.path:match("[^/]+$"),subtitle=math.floor(f.size/1024).." KiB",icon="ui_backup"} end end
  return chrome.view({title="Meus backups",layout="list",body="Selecione um snapshot",buttons=b})
 end
 local u=sys.usage();return chrome.view({title="Backup e restauração",layout="status",body=status~="" and status or "Snapshots verificados no cartão",
  statusFields={{label="Arquivo selecionado",value=path},{label="Instalação / quota",value=u and string.format("%.1f / 4000 MB",u.used/1e6) or "Indisponível",percent=u and math.floor(u.used*100/4e9) or 0},
   {label="Backups",value=u and string.format("%.1f MB",u.backups/1e6) or "Indisponível"},{label="Espaço físico livre",value=u and string.format("%.1f MB",u.physical_free/1e6) or "Indisponível"}},
  buttons={{id="create",label="Criar backup",icon="ui_backup"},{id="browse",label="Escolher arquivo",icon="ui_open"},{id="restore",label="Restaurar",icon="ui_restore"}}})
end
