-- SPDX-License-Identifier: MIT
local chrome=sys.module('chrome')
local listPage,page,path,target,status,details=0,'list','','','Pacotes SIS em Transferências ou /update/packages',{}
local function downgrade(a,b)
 local x,y={},{};for n in (a or ''):gmatch('%d+') do x[#x+1]=tonumber(n) end;for n in (b or ''):gmatch('%d+') do y[#y+1]=tonumber(n) end
 for i=1,3 do if (x[i] or 0)~=(y[i] or 0) then return (x[i] or 0)<(y[i] or 0) end end;return false
end
local function install(operation)
 local ok,why=sys.package(operation,path);status=ok and 'Instalado. Abra o aplicativo para validar a primeira execução.' or tostring(why);page='list'
end
function event(action)
 if action=='next_list' then listPage=listPage+1 elseif action=='prev_list' then listPage=math.max(0,listPage-1)
 elseif action:sub(1,8)=='inspect:' then
  path=action:sub(9);local info,why=sys.package('inspect',path)
  if info then details=info;page='review';status=info.uid and info.uid~=0 and 'SIS sem assinatura. Autoria não verificada. Revise versão, migração e permissões.' or 'Revise as permissões antes de instalar.';if info.compatible==false then status=info.compatibility end else status=tostring(why) end
 elseif action:sub(1,8)=='details:' then target=action:sub(9);for _,a in ipairs(sys.catalog() or {}) do if a.id==target then details=a;page='details';break end end
 elseif action=='install' and page=='review' then if details.uid and details.uid~=0 and downgrade(details.version,details.old_version) then page='downgrade' else install('install') end
 elseif action=='confirm_downgrade' and page=='downgrade' then install('install_downgrade')
 elseif action=='technical' then page='technical'
 elseif action:sub(1,7)=='launch:' then next_app=action:sub(8)
 elseif action:sub(1,9)=='rollback:' then target=action:sub(10);page='rollback'
 elseif action=='confirm_rollback' and page=='rollback' then local ok,why=sys.package('rollback',target);status=ok and 'Versão/dados restaurados. Abra o aplicativo para validar.' or tostring(why);page='list'
 elseif action:sub(1,7)=='remove:' then target=action:sub(8);page='remove';status='Preservar os dados é a opção padrão. Dependentes impedem a remoção.'
 elseif action=='confirm_remove' or action=='confirm_erase' then local ok,why=sys.package(action=='confirm_erase' and 'remove_data' or 'remove',target);status=ok and 'Aplicativo removido.' or tostring(why);page='list'
 elseif action:sub(1,6)=='theme:' then local ok,why=sys.package('theme',action:sub(7));status=ok and 'Tema aplicado.' or tostring(why)
 elseif action=='default_theme' then local ok,why=sys.package('theme_default','');status=ok and 'Tema integrado restaurado.' or tostring(why)
 elseif action=='back' then if page=='list' then next_app='shell' else page='list' end
 elseif action=='cancel' then page='list'
 elseif action=='home' then next_app='shell' end
 if page=='downgrade' then return chrome.view({title='Confirmar downgrade?',layout='status',body='A versão anterior receberá uma cópia dos dados atuais. Uma mudança de esquema exige migração compatível. A versão e os dados atuais ficam disponíveis para rollback.',statusFields={{label='Versão atual',value=details.old_version or ''},{label='Versão escolhida',value=details.version or ''}},buttons={{id='confirm_downgrade',label='Confirmar downgrade'},{id='cancel',label='Cancelar'}}}) end
 if page=='rollback' then return chrome.view({title='Restaurar versão e dados?',layout='status',body='Restaura a cópia anterior dos dados. Alterações feitas depois daquela cópia não entram na versão restaurada. Uma tentativa pendente será cancelada.',statusFields={{label='Aplicativo',value=target}},buttons={{id='confirm_rollback',label='Restaurar'},{id='cancel',label='Cancelar'}}}) end
 if page=='remove' then return chrome.view({title='Remover aplicativo?',layout='status',body=status,statusFields={{label='Aplicativo',value=target}},buttons={{id='confirm_remove',label='Remover e manter dados',icon='ui_delete'},{id='confirm_erase',label='Apagar app e dados',icon='ui_delete'},{id='cancel',label='Cancelar'}}}) end
 if page=='technical' then return chrome.view({title='Detalhes do pacote',layout='diagnostic',readonly=true,body='Hash identifica os bytes revisados; não autentica o autor.',text='SHA-256: '..(details.sha256 or '')..'\nUID: '..tostring(details.uid or '')..'\nInspeção: '..tostring(details.inspection_ms or 0)..' ms\nDependências: '..(details.dependencies or '')}) end
 if page=='review' or page=='details' then
  local b={}
  if page=='review' then if details.compatible~=false then b[#b+1]={id='install',label=details.is_update and 'Atualizar / revisar downgrade' or 'Instalar',icon='ui_install'} end;b[#b+1]={id='technical',label='Detalhes técnicos'};b[#b+1]={id='cancel',label='Cancelar'}
  else b={{id='remove:'..target,label='Remover',icon='ui_delete'}};if details.kind~='theme' then table.insert(b,1,{id='launch:'..target,label='Abrir aplicativo'}) end;if details.can_rollback then b[#b+1]={id='rollback:'..target,label='Restaurar '..(details.previous_version or 'anterior')} end end
  if page=='details' and details.kind=='theme' then b[#b+1]={id='theme:'..target,label='Aplicar tema',icon='profiles'} end
  return chrome.view({title=details.title or 'Pacote',layout='status',body=status,statusFields={
   {label='Versão',value=(details.old_version and details.old_version~='' and details.old_version..' → ' or '')..(details.version or '')},
   {label='Autor declarado',value=details.author or details.kind or ''},
   {label=details.new_permissions and details.new_permissions~='' and 'NOVAS permissões' or 'Permissões',value=details.new_permissions and details.new_permissions~='' and details.new_permissions or details.permissions and details.permissions~='' and details.permissions or 'Somente dados próprios'},
   {label='Dependências',value=details.dependencies and #details.dependencies>400 and 'Lista extensa; veja Detalhes técnicos' or details.dependencies or 'Catálogo instalado'},
   {label='Dados',value=details.migration or ('Esquema '..tostring(details.schema or 1))},
   {label='Formato / confiança',value=(details.format or 'Instalado')..' • '..(details.trust or 'Sem assinatura')},
   {label='Tamanho extraído',value=details.bytes and details.bytes..' bytes' or 'Já instalado'},
   {label='UID',value=details.uid and string.format('0x%08x',details.uid) or '—'}},buttons=b})
 end
 local folders={};local total=0
 for _,folder in ipairs({'/home/Transfer','/update/packages'}) do
  local files,count=sys.fs('list',folder,'',0,'packages');folders[#folders+1]={path=folder,total=count or 0,first=files or {}};total=total+(count or 0)
 end
 local all=sys.catalog() or {};total=total+#all
 local pageSize=40;local pages=math.max(1,math.ceil(total/pageSize));listPage=math.min(listPage,pages-1)
 local start=listPage*pageSize;local last=math.min(start+pageSize,total);local items={};local base=0
 for _,folder in ipairs(folders) do
  local at=math.max(0,start-base);local stop=math.min(folder.total,last-base)
  while at<stop do
   local files=at==0 and folder.first or (sys.fs('list',folder.path,'',at,'packages') or {})
   if #files==0 then break end
   for j=1,math.min(#files,stop-at) do local f=files[j];items[#items+1]={id='inspect:'..f.path,label=f.path:match('[^/]+$'),subtitle=folder.path..' • Revisar pacote sem assinatura',icon='installer'} end
   at=at+#files
  end
  base=base+folder.total
 end
 for i=math.max(1,start-base+1),math.min(#all,last-base) do local a=all[i];items[#items+1]={id='details:'..a.id,label=a.title,subtitle=a.version..' • Instalado',icon=a.kind=='theme' and 'profiles' or 'installer',close='remove:'..a.id} end
 local b={};local pending=sys.package('pending','') or {}
 if pending.id then b[#b+1]={id='launch:'..pending.id,label='Abrir / validar '..pending.id,icon='ui_open'};b[#b+1]={id='rollback:'..pending.id,label='Cancelar tentativa pendente',icon='ui_restore'} end
 for _,item in ipairs(items) do b[#b+1]=item end
 local range=total==0 and 'Nenhum pacote ou aplicativo instalado' or ((start+1)..'–'..last..' de '..total..' • Página '..(listPage+1)..'/'..pages)
 return chrome.view({title='Instalador de aplicativos',layout='packages',scrollKey='installer:'..listPage,body=range..' • '..status,buttons=b,tools={{id='prev_list',label='Anterior',icon='ui_back'},{id='next_list',label='Próxima',icon='ui_forward'},{id='refresh',label='Atualizar',icon='ui_refresh'},{id='default_theme',label='Tema padrão',icon='profiles'}}})
end
