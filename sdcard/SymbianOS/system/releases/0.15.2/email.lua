-- SPDX-License-Identifier: MIT
local chrome,store=sys.module('chrome'),sys.module('store')
local page,folder,remote,index='list','inbox','INBOX',0
local notice='';local selected='';local draft={id='',to='',subject='',body=''};local editing=false
local account=nil
local names={inbox='Entrada',drafts='Rascunhos',outbox='Saída',sent='Enviados',trash='Lixeira local'}
local states={queued='Na fila',sending='Enviando',accepted='Aceito pelo servidor',uncertain='Envio incerto — confira antes de repetir',failed='Falha — toque para conferir',draft='Rascunho',cached='Disponível offline',local_='Local'}
local function call(op,...)
 local ok,why=sys.mail(op,...);if not ok then notice=tostring(why or 'Operação indisponível') end;return ok
end
local function capture(value)
 local to,subject,body=(value or ''):match('^([^\n]*)\n([^\n]*)\n(.*)$')
 if to then draft.to,draft.subject,draft.body=to,subject,body end
end
local function save(queue)
 local id=call(queue and 'queue' or 'draft',draft.id,draft.to,draft.subject,draft.body)
 if id then draft.id=id;notice=queue and 'Mensagem na caixa de saída' or 'Rascunho salvo';return true end
 return false
end
function event(action,value)
 if page=='compose' then capture(value) end
 if action=='focus' then account=sys.mail('account')
 elseif action=='checkpoint' or action=='suspend' then if page=='compose' and (draft.body~='' or draft.to~='' or draft.subject~='') then if not save(false) then navigation_blocked=true end end
 elseif action=='home' then if page=='compose' and not save(false) then navigation_blocked=true else next_app='shell' end
 elseif action=='back' then if page=='compose' then if save(false) then page='list' else navigation_blocked=true end elseif page=='list' then next_app='shell' else page='list' end
 elseif action=='settings' then account=sys.mail('account');page='settings'
 elseif action=='diagnostic' then page='diagnostic'
 elseif action=='general' or action=='incoming' or action=='outgoing' then page=action
 elseif action=='save_general' then local f=store.lines(value or '');local previous=account.protocol;account.address=f[1] or '';account.name=f[2] or '';account.protocol=f[3] or 'imap';if previous~=account.protocol and (tostring(account.inPort)=='993' or tostring(account.inPort)=='995' or tostring(account.inPort)=='143' or tostring(account.inPort)=='110') then account.inPort=account.inSecurity=='starttls' and (account.protocol=='pop' and '110' or '143') or (account.protocol=='pop' and '995' or '993') end;account.automatic=f[4]=='1';page='settings';notice='Revise Recebimento e Envio; depois toque em Salvar conta.'
 elseif action=='save_incoming' or action=='save_outgoing' then
  local f=store.lines(value or '');local prefix=action=='save_incoming' and 'in' or 'out'
  account[prefix..'Host']=f[1] or '';account[prefix..'Port']=f[2] or '';account[prefix..'Security']=f[3] or 'tls';account[prefix..'User']=f[4] or '';account[prefix..'Password']=f[5] or '';page='settings';notice='Ajustes preparados; toque em Salvar conta.'
 elseif action=='save_account' then if call('configure',account) then notice='Conta salva';page='list' end
 elseif action=='new' then draft={id='',to='',subject='',body=''};page='compose'
 elseif action=='save_draft' then if save(false) then folder='drafts';page='list' end
 elseif action=='send' then if save(true) then draft={id='',to='',subject='',body=''};folder='outbox';page='list' end
 elseif action=='sync' then if call('sync',remote) then notice='Recebendo em segundo plano...' end
 elseif action=='flush' then if call('flush') then notice='Enviando em segundo plano...' end
 elseif action=='cancel_mail' then call('cancel');notice='Operação cancelada'
 elseif action=='folders' then page='folders'
 elseif action:sub(1,7)=='folder:' then folder=action:sub(8);if folder:sub(1,7)=='remote:' then remote=folder:sub(8) else remote='INBOX' end;index=0;page='list'
 elseif action:sub(1,5)=='open:' then selected=action:sub(6);local m=sys.mail('read',selected);if m then if m.folder=='drafts' then draft={id=m.id,to=m.to,subject=m.subject,body=m.body};page='compose' else page='read' end end
 elseif action=='reply' then local m=sys.mail('read',selected);if m then local from=m.from:match('<([^>]+)>') or m.from;draft={id='',to=from,subject=m.subject:match('^[Rr][Ee]:') and m.subject or 'Re: '..m.subject,body='\n\n— Mensagem anterior —\n'..store.clip(m.body,2000)};page='compose' end
 elseif action=='delete' then if call('move',selected,'trash') then page='list';notice='Movida para a lixeira local; servidor preservado' end
 elseif action=='purge' then if call('move',selected,'delete') then page='list' end
 elseif action=='retry' then page='retry'
 elseif action=='confirm_retry' then if call('move',selected,'outbox') then page='list';notice='Nova tentativa autorizada; toque em Enviar fila' end
 elseif action=='next' then index=index+1 elseif action=='prev' then index=math.max(0,index-1)
 end
 account=account or sys.mail('account');local status=sys.mail('status') or {};local footer={};local buttons={}
 if page=='diagnostic' then return chrome.view({title='E-mail · Diagnóstico',layout='diagnostic',readonly=true,body='Última operação · registro em /SymbianOS/logs/mail.txt',text=status.message or 'Nenhuma operação',refreshMs=1000})
 elseif page=='settings' then return chrome.view({title='E-mail · Conta',layout='status',body=notice~='' and notice or 'Uma conta ativa. Senhas separadas, salvas no cartão SD.',statusFields={{label='Conta / protocolo',value=(account.address~='' and account.address or 'Não configurada')..' · '..account.protocol:upper()},{label='Recebimento',value=(account.inHost or '')..':'..tostring(account.inPort or '')},{label='Envio SMTP',value=(account.outHost or '')..':'..tostring(account.outPort or '')}},buttons={{id='general',label='Identidade / protocolo',icon='contacts'},{id='incoming',label='Recebimento',icon='ui_download'},{id='outgoing',label='Envio SMTP',icon='ui_export'},{id='diagnostic',label='Diagnóstico',icon='ui_info'}},footer={{id='save_account',label='Salvar conta',icon='ui_save'}}})
 elseif page=='general' then return chrome.view({title='E-mail · Identidade',layout='form',field=true,text=account.address..'\n'..account.name..'\n'..account.protocol..'\n'..(account.automatic and '1' or '0'),controls={{label='Endereço de e-mail',type='text'},{label='Nome do remetente',type='text'},{label='Recebimento',type='select',options='IMAP\nPOP3',keys='imap\npop'},{label='Enviar fila ao conectar Wi-Fi',type='select',options='Manual\nAutomático',keys='0\n1'}},footer={{id='save_general',label='OK',icon='ui_ok'}}})
 elseif page=='incoming' or page=='outgoing' then
  local prefix=page=='incoming' and 'in' or 'out';local port=account[prefix..'Port'];local default=page=='outgoing' and '465 TLS / 587 STARTTLS' or account.protocol=='pop' and '995 TLS / 110 STARTTLS' or '993 TLS / 143 STARTTLS'
  return chrome.view({title='E-mail · '..(page=='incoming' and account.protocol:upper() or 'SMTP'),layout='form',field=true,body='Porta livre para configurar. Valores comuns: '..default,text=table.concat({account[prefix..'Host'] or '',tostring(port or ''),account[prefix..'Security'] or 'tls',account[prefix..'User'] or '',account[prefix..'Password'] or ''},'\n'),controls={{label='Servidor',type='text'},{label='Porta (1–65535)',type='text'},{label='Segurança',type='select',options='SSL / TLS direto\nSTARTTLS',keys='tls\nstarttls'},{label='Usuário '..(page=='incoming' and 'de recebimento' or 'SMTP'),type='text'},{label='Senha '..(page=='incoming' and 'de recebimento' or 'SMTP'),type='password'}},footer={{id='save_'..page,label='OK',icon='ui_ok'}}})
 elseif page=='compose' then return chrome.view({title='E-mail · Escrever',layout='mail_compose',field=true,body=notice~='' and notice or 'Texto simples UTF-8; até 16 KiB. Envio fica na fila offline.',text=draft.to..'\n'..draft.subject..'\n'..draft.body,controls={{label='Para (vírgula separa destinatários)',type='text'},{label='Assunto',type='text'},{label='Mensagem',type='multiline'}},footer={{id='send',label='Enviar',icon='ui_export'},{id='save_draft',label='Salvar rascunho',icon='ui_save'}}})
 elseif page=='read' then local m=sys.mail('read',selected);if not m then page='list' else
  footer={{id='reply',label='Responder',icon='ui_back'},{id=m.folder=='trash' and 'purge' or 'delete',label=m.folder=='trash' and 'Excluir local' or 'Lixeira',icon='ui_delete'}}
  if m.status=='failed' or m.status=='uncertain' then footer[#footer+1]={id='retry',label='Tentar novamente',icon='ui_refresh'} end
  return chrome.view({title='E-mail · '..store.clip(m.subject,36),layout='diagnostic',readonly=true,body=states[m.status] or m.status,text='De: '..m.from..'\nPara: '..m.to..'\nData: '..m.date..'\nAssunto: '..m.subject..'\n\n'..m.body,footer=footer}) end
 elseif page=='retry' then return chrome.view({title='E-mail · Reenvio',layout='reader',readonly=true,field=true,text='O servidor pode ter aceitado a mensagem antes da conexão cair. Confira sua caixa de enviados/servidor para evitar duplicação.\n\nSe quiser uma nova tentativa, confirme abaixo.',footer={{id='confirm_retry',label='Autorizar reenvio',icon='ui_export'}}})
 elseif page=='folders' then
  for _,f in ipairs({'inbox','drafts','outbox','sent','trash'}) do buttons[#buttons+1]={id='folder:'..f,label=names[f],icon=f=='trash' and 'ui_delete' or 'ui_folder'} end
  for _,f in ipairs(sys.mail('folders') or {}) do if f~='INBOX' then buttons[#buttons+1]={id='folder:remote:'..f,label=f,icon='ui_folder'} end end
  return chrome.view({title='E-mail · Pastas',layout='list',body='POP3 mantém apenas Entrada no servidor; demais pastas são locais.',buttons=buttons})
 end
 local all=sys.mail('list',folder) or {};index=math.min(index,math.max(0,math.floor((#all-1)/6)))
 for i=index*6+1,math.min(index*6+6,#all) do local m=all[i];buttons[#buttons+1]={id='open:'..m.id,label=(not m.seen and (folder=='inbox' or folder:sub(1,7)=='remote:') and '• ' or '')..store.clip(m.subject,70),subtitle=store.clip(m.from,35)..' · '..(states[m.status] or m.status),icon='email'} end
 return chrome.view({title='E-mail · '..(names[folder] or remote),layout='list',body=(notice~='' and notice..' · ' or '')..(status.message or '')..' · '..#all..' mensagens',buttons=buttons,centerTools=true,refreshMs=1000,tools={{id='prev',label='Anterior',icon='ui_prev'},{id='next',label='Próxima',icon='ui_next'}},footer={{id='new',label='Escrever',icon='ui_new'},{id='folders',label='Pastas',icon='ui_folder'},{id=status.busy and 'cancel_mail' or folder=='outbox' and 'flush' or 'sync',label=status.busy and 'Cancelar' or folder=='outbox' and 'Enviar fila' or 'Receber',icon=status.busy and 'ui_cancel' or folder=='outbox' and 'ui_export' or 'ui_download'}},option='settings'})
end
