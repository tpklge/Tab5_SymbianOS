-- SPDX-License-Identifier: MIT
local chrome=sys.module("chrome")
local listPage,page,path,target,status,details=0,"list","","","Pacotes locais em /update/packages",{}
function event(action)
 if action=="next_list" then listPage=listPage+1 elseif action=="prev_list" then listPage=math.max(0,listPage-1)
 elseif action:sub(1,8)=="inspect:" then
  path=action:sub(9);local info,why=sys.package("inspect",path)
  if info then details=info;page="review";status="Revise as permissões antes de instalar." else status=tostring(why) end
 elseif action:sub(1,8)=="details:" then target=action:sub(9);for _,a in ipairs(sys.catalog() or {}) do if a.id==target then details=a;page="details";break end end
 elseif action=="install" and page=="review" then local ok,why=sys.package("install",path);status=ok and "Instalado. Disponível no Menu." or tostring(why);page="list"
 elseif action:sub(1,7)=="remove:" then target=action:sub(8);page="remove";status="Preservar os documentos é a opção padrão."
 elseif action=="confirm_remove" or action=="confirm_erase" then local ok,why=sys.package(action=="confirm_erase" and "remove_data" or "remove",target);status=ok and "Aplicativo removido." or tostring(why);page="list"
 elseif action:sub(1,6)=="theme:" then local ok,why=sys.package("theme",action:sub(7));status=ok and "Tema aplicado." or tostring(why)
 elseif action=="default_theme" then local ok,why=sys.package("theme_default","");status=ok and "Tema integrado restaurado." or tostring(why)
 elseif action=="back" then if page=="list" then next_app="shell" else page="list" end
 elseif action=="cancel" then page="list"
 elseif action=="home" then next_app="shell" end
 if page=="remove" then return chrome.view({title="Remover aplicativo?",layout="status",body=status,statusFields={{label="Aplicativo",value=target}},buttons={{id="confirm_remove",label="Remover app",icon="ui_delete"},{id="confirm_erase",label="Apagar app e dados",icon="ui_delete"},{id="cancel",label="Cancelar"}}}) end
 if page=="review" or page=="details" then
  local b=page=="review" and {{id="install",label="Instalar",icon="ui_install"},{id="cancel",label="Cancelar"}} or {{id="remove:"..target,label="Remover",icon="ui_delete"}}
  if page=="details" and details.kind=="theme" then b[#b+1]={id="theme:"..target,label="Aplicar tema",icon="profiles"} end
  return chrome.view({title=details.title or "Pacote",layout="status",body=status,
   statusFields={{label="Versão",value=details.version or ""},{label="Autor / tipo",value=details.author or details.kind or ""},
    {label="Permissões",value=details.permissions~="" and (details.permissions or "") or "Somente dados próprios"},{label="Tamanho extraído",value=details.bytes and (details.bytes.." bytes") or "Já instalado"}},buttons=b})
 end
 local b={};for _,f in ipairs(sys.fs("list","/update/packages","",listPage*6) or {}) do if #b<6 and not f.directory and f.path:match("%.t5pkg$") then b[#b+1]={id="inspect:"..f.path,label=f.path:match("[^/]+$"),subtitle="Revisar pacote",icon="installer"} end end
 local all=sys.catalog() or {};for i=listPage*6+1,math.min(listPage*6+6,#all) do local a=all[i];b[#b+1]={id="details:"..a.id,label=a.title,subtitle=a.version.." • "..a.kind,icon=a.kind=="theme" and "profiles" or "installer",close="remove:"..a.id} end
 return chrome.view({title="Instalador de aplicativos",layout="packages",body=status,buttons=b,tools={{id="prev_list",label="Anterior",icon="ui_back"},{id="next_list",label="Próxima",icon="ui_forward"},{id="default_theme",label="Tema padrão",icon="profiles"}}})
end
