-- SPDX-License-Identifier: MIT
local chrome,store,reader=sys.module('chrome'),sys.module('store'),sys.module('html_reader')
local mode,url,html,base,message='address','https://','','',''
local history=store.read('/data/browser/history.tsv') or {}
local favorites=store.read('/data/browser/favorites.tsv') or {}
local forms={};local selectedForm=0;local formPage=0;local formValues={};local rich=false
local cursor=#history;local pending=nil;local links={};local listPage=0;local revision=0
local function load(address,remember,target)
 local path,location=sys.net('page_path',address)
 if not path then return false end
 base=location or address;html=path;url=address;mode='document';revision=revision+1
 if target then cursor=target end
 if remember then
  while #history>cursor do table.remove(history) end
  if not history[#history] or history[#history][1]~=address then history[#history+1]={address} end
  while #history>32 do table.remove(history,1) end
  cursor=#history;store.write('/data/browser/history.tsv',history)
 end
 return true
end
local function navigate(address,remember,reload,target)
 address=(address or ''):match('^%s*(.-)%s*$'):gsub('#.*$','')
 if not address:match('^https?://') then message='Use um endereço HTTP ou HTTPS.';return end
 if #address>1024 then message='Endereço longo demais.';return end
 reader.close()
 if not reload and load(address,remember,target) then message='Página do cache no cartão.';return end
 if sys.net('page',address,base) then pending={address,remember,target};message='Carregando…';mode='loading'
 else message='Não foi possível iniciar. Confira Wi-Fi, perfil Offline e operações ativas.' end
end
local function captureForm(value)
 local at=formPage*4+1
 for item in ((value or '')..'\n'):gmatch('(.-)\n') do if at>formPage*4+4 then break end;formValues[at]=item;at=at+1 end
end
function event(action,value)
 local profile=sys.net('browser_profile')
 if type(profile)~='table' or profile.build~='20261113-R3' then
  if action=='back' or action=='home' then next_app='shell' end
  return chrome.view({title='Navegador · atualizar runtime',body='O BIN e os arquivos do cartão são de revisões diferentes. Atualize ambos pelo launcher.',buttons={}})
 end
 local net=sys.net()
 local pageStatus=pending and sys.net('page_status',pending[1])
 if pending and pageStatus and not pageStatus.busy then
  local request=pending;pending=nil
  -- A failed refresh must never be reported as a successful cached refresh.
  if pageStatus.ok and load(request[1],request[2],request[3]) then message='Página carregada.'
  else message=pageStatus.message;mode=html~='' and 'document' or 'address' end
 end
 reader.event(action,net.busy)
 if action=='go' then navigate(value,true,false)
 elseif action=='navigate_url' then navigate(value,true,false)
 elseif action=='address' then mode='address'
 elseif action=='toggle_render' then rich=not rich;revision=revision+1;message=rich and 'HTML/CSS limitado.' or 'Leitura nativa.'
 elseif action=='forms' then forms=sys.html('forms',html) or {};mode='forms'
 elseif action:sub(1,5)=='form:' then selectedForm=tonumber(action:sub(6)) or 0;formPage=0;formValues={};local f=forms[selectedForm];if f then for _,v in ipairs(f.fields) do if v.type~='hidden' and v.type~='submit' then formValues[#formValues+1]=v.value end end end;mode='form'
 elseif action=='form_next' or action=='form_prev' then captureForm(value);formPage=math.max(0,math.min(math.max(0,math.floor((#formValues-1)/4)),formPage+(action=='form_next' and 1 or -1)))
 elseif action=='form_submit' then
  captureForm(value)
  local ok,address=sys.html('submit',html,selectedForm,formValues)
  if ok then pending={address,false};mode='loading';message='Enviando formulário…' else message='Envio recusado: confira Wi-Fi, HTTPS e destino do formulário.' end
 elseif action=='clear_sessions' then
  message=sys.html('clear_sessions') and 'Cookies e páginas salvas removidos.' or 'Não foi possível limpar agora.';html='';mode='address'

 elseif action=='reload' then navigate(url,false,true)
 elseif action=='stop' then if pending then sys.net('cancel') end;reader.close();message='Cancelando…'
 elseif action=='previous' and cursor>1 then navigate(history[cursor-1][1],false,false,cursor-1)
 elseif action=='forward' and cursor<#history then navigate(history[cursor+1][1],false,false,cursor+1)
 elseif action=='links' then links=sys.html().links or {};mode='links';listPage=0
 elseif action=='favorites' or action=='history' then mode=action;listPage=0
 elseif action=='list_next' then listPage=listPage+1
 elseif action=='list_prev' then listPage=math.max(0,listPage-1)
 elseif action:sub(1,5)=='link:' then local item=links[tonumber(action:sub(6))];if item then navigate(item.url,true,false) end
 elseif action:sub(1,6)=='visit:' then local list=mode=='favorites' and favorites or history;local item=list[tonumber(action:sub(7))];if item then navigate(item[1],true,false) end
 elseif action=='star' and html~='' then
  local found=false;for _,item in ipairs(favorites) do if item[1]==url then found=true end end
  if not found then if #favorites>=32 then message='Limite de 32 favoritos.' else favorites[#favorites+1]={url};message=store.write('/data/browser/favorites.tsv',favorites) and 'Favorito salvo.' or 'Falha ao salvar favorito.' end else message='Já está nos favoritos.' end
 elseif action=='back' then if mode~='document' and html~='' then mode='document' else reader.close();next_app='shell' end
 elseif action=='home' then reader.close();next_app='shell' end
 if mode=='address' then return chrome.view({title='Navegador · 13/11 R3',layout='form',field=true,submit='go',text=url,controls={{label='Endereço HTTP(S)',type='text'}},body=message~='' and message or 'HTML/CSS limitado • JavaScript ES5 básico • formulários GET/POST • cookies no SD.',buttons={{id='go',label='Abrir / Enter'},{id='favorites',icon='ui_folder',label='Favoritos'},{id='history',icon='clock',label='Histórico'},{id='clear_sessions',label='Limpar sessões'}}}) end
 if mode=='forms' then
  local buttons={};for i,f in ipairs(forms) do buttons[#buttons+1]={id='form:'..i,label='Formulário '..i..' · '..f.method,subtitle=f.action} end
  return chrome.view({title='Formulários',layout='list',body=#forms==0 and 'Nenhum formulário compatível.' or 'Escolha um formulário. Confira o destino antes de enviar.',buttons=buttons})
 end
 if mode=='form' then
  local f=forms[selectedForm];if not f then mode='document' else
   local controls,values={},{};local at=0;for _,v in ipairs(f.fields) do if v.type~='hidden' and v.type~='submit' then at=at+1;if at>formPage*4 and at<=formPage*4+4 then controls[#controls+1]={label=v.label,type=v.type=='password' and 'password' or 'text'};values[#values+1]=formValues[at] or '' end end end
   local buttons={{id='form_submit',label='Enviar / Enter'},{id='back',label='Cancelar'}};if formPage>0 then buttons[#buttons+1]={id='form_prev',label='Campos anteriores'} end;if #formValues>(formPage+1)*4 then buttons[#buttons+1]={id='form_next',label='Próximos campos'} end
   return chrome.view({title='Formulário · '..f.method,layout='form',field=true,submit='form_submit',controls=controls,text=table.concat(values,'\n'),body=f.action..' · campos '..tostring(formPage*4+1)..'–'..tostring(math.min(#formValues,formPage*4+4)),buttons=buttons})
  end
 end
 if mode=='loading' then return chrome.view({title='Navegador',layout='status',refreshMs=500,body=((pageStatus and pageStatus.message) or net.message or message)..' • '..tostring((pageStatus and pageStatus.bytes) or net.bytes or 0)..' bytes',buttons={{id='stop',label='Cancelar'}}}) end
 if mode=='links' or mode=='favorites' or mode=='history' then
  local list=mode=='links' and links or mode=='favorites' and favorites or history;local buttons={}
  listPage=math.min(listPage,math.max(0,math.floor((#list-1)/6)))
  for i=listPage*6+1,math.min(#list,listPage*6+6) do local item=list[i];buttons[#buttons+1]={id=(mode=='links' and 'link:' or 'visit:')..i,label=mode=='links' and item.label or item[1],subtitle=mode=='links' and item.url or '',icon='network'} end
  if listPage>0 then buttons[#buttons+1]={id='list_prev',label='Anterior'} end
  if #list>(listPage+1)*6 then buttons[#buttons+1]={id='list_next',label='Próxima'} end
  buttons[#buttons+1]={id='address',icon='ui_search',label='Endereço'}
  return chrome.view({title=mode=='links' and 'Links da página' or mode=='favorites' and 'Favoritos' or 'Histórico',layout='list',body=#list==0 and 'Lista vazia.' or message,buttons=buttons})
 end
 local view={title='Navegador',body=base..' • '..message,htmlPath=html,baseUrl=base,refreshMs=0,
  buttons={{id='address',icon='ui_search',label='Endereço'},{id='links',icon='ui_open',label='Links'},{id='previous',icon='ui_back',label='Voltar página'},{id='forward',icon='ui_forward',label='Avançar'},{id='reload',icon='ui_refresh',label='Recarregar'},{id='star',icon='ui_star',label='Favoritar'},{id='favorites',icon='ui_folder',label='Favoritos'},{id='history',icon='clock',label='Histórico'},{id='toggle_render',icon='ui_info',label=rich and 'Leitura' or 'HTML/CSS'},{id='forms',icon='ui_keyboard',label='Formulários'},{id='load_images',icon='gallery',label='Carregar imagens'}},
  htmlMode=rich and 'rich' or 'reader',footer={}}
 return chrome.view(reader.view(view,url..':'..revision))
end
