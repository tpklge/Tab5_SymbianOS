-- SPDX-License-Identifier: MIT
local chrome,store=sys.module('chrome'),sys.module('store')
local M={};local page='main';local message='';local index=0;local target='';local draft='';local peer='4294967295';local channel='';local selected='';local editChannel='1';local channelDraft='';local creating=false;local lastChatCount=0;local diagnosticText=''
local function rows(name) return store.read('/data/mesh/'..name..'.tsv') or {} end
local function send(command) local ok,why=sys.send('services','mesh',command);message=ok and '' or tostring(why or 'Não foi possível executar o comando');return ok end
local nodeCache={}
local function name(id) for _,r in ipairs(nodeCache) do if r[1]==id then return r[2] end end;local n=tonumber(id);return n and string.format('!%08x',n) or id end
local function shown(s) return s and s~='' and s or '—' end
local function heard(s) local n=tonumber(s);if not n or n<=0 then return 'Não informado' end;local d=sys.date(n);return d:sub(9,10)..'/'..d:sub(6,7)..'/'..d:sub(1,4)..' '..d:sub(12,16) end
local function nav() return {{id='prev',label='Anterior',icon='ui_prev'},{id='next',label='Próxima',icon='ui_next'}} end
local function choices()
 local labels,keys={},{};local all=rows('channels');table.sort(all,function(a,b)return tonumber(a[1])<tonumber(b[1])end)
 for _,r in ipairs(all) do if r[3]~='0' then labels[#labels+1]=r[1]..' · '..r[2];keys[#keys+1]=r[1] end end
 if #keys==0 then labels={'0 · Canal principal'};keys={'0'} end
 local found=false;for _,k in ipairs(keys) do if channel==k then found=true end end;if not found then channel=keys[1];for _,r in ipairs(all) do if r[3]=='1' then channel=r[1];break end end end
 return table.concat(labels,'\n'),table.concat(keys,'\n')
end
local function peerLabel(s)
 s=store.clip(s:gsub('[\r\n]',' '),32)
 while #s>48 do s=s:sub(1,utf8.offset(s,-1)-1) end
 return s
end
local function peers()
 local labels,keys={'Todos no canal'},{'4294967295'};local all=nodeCache;table.sort(all,function(a,b)return a[2]:lower()<b[2]:lower()end)
 local own=(rows('status')[1] or {})[4];local found=peer=='4294967295'
 for _,r in ipairs(all) do if r[1]~=own and (#keys<63 or r[1]==peer) then labels[#labels+1]=peerLabel(r[2]);keys[#keys+1]=r[1];if peer==r[1] then found=true end end end
 if not found then labels[#labels+1]=peerLabel(name(peer));keys[#keys+1]=peer end
 return table.concat(labels,'\n'),table.concat(keys,'\n')
end
function M.view(kind,action,value)
 nodeCache=rows('nodes')
 local title=kind=='ble' and 'Meshtastic BLE' or 'Meshtastic RAK'
 -- UI supplies live selector values and draft on refresh/navigation as well as send.
 if page=='chat' and value and value~='' then local f=store.lines(value);if tonumber(f[1]) and tonumber(f[2]) then if peer~=f[1] or channel~=f[2] then index=100000 end;peer=f[1];channel=f[2];draft=f[3] or '' end end
 if action=='focus' and kind=='uart' and (sys.radio('link') or '')=='' then send('uart');page='main'
 elseif action=='home' then next_app='shell'
 elseif action=='back' then if page=='main' then next_app='shell' else page='main' end
 elseif action=='connect' then if kind=='uart' then send('uart');page='main' else page='pair' end
 elseif action=='confirm_uart' then send('uart');page='main'
 elseif action=='scan' then send('scan');page='devices'
 elseif action:sub(1,7)=='device:' then target=action:sub(8);page='pair'
 elseif action=='connect_ble' then local f=store.lines(value or '');if f[1] and f[1]:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x,[01]$') and f[2] and f[2]:match('^%d%d%d%d%d%d$') then target=f[1];send('ble\n'..target..','..f[2]);page='main' else message='Endereço BLE válido e PIN de seis dígitos necessários.' end
 elseif action=='diagnostic' then page='diagnostic'
 elseif action=='save_diagnostic' then message=sys.write('/data/mesh/diagnostic.txt',diagnosticText) and 'Salvo: /SymbianOS/data/mesh/diagnostic.txt' or 'Não foi possível salvar diagnóstico'
 elseif action=='wiring' then page='wiring'
 elseif action=='disconnect' then send('disconnect')
 elseif action=='compose' or action=='history' then page='chat';index=100000
 elseif action=='send' then local f=store.lines(value or '');local node=(f[1] or ''):gsub('^!','0x');local destination=tonumber(node);local c=tonumber(f[2]);local body=f[3] or ''
  if destination and destination%1==0 and destination>=1 and destination<=0xffffffff and c and c%1==0 and c>=0 and c<=7 and #body>0 and #body<=228 and utf8.len(body) then
   if send('send\n'..destination..'\n'..c..'\n'..body) then draft='';peer=tostring(destination);channel=tostring(c);message='Mensagem na caixa de saída';index=100000 end;page='chat'
  else message='Selecione destino/canal e escreva até 228 bytes UTF-8.' end
 elseif action=='nodes' or action=='channels' or action=='outbox' then page=action;index=0
 elseif action:sub(1,5)=='node:' then selected=action:sub(6)
 elseif action=='dm' then if selected~='' then peer=selected;page='chat';index=100000 end
 elseif action:sub(1,8)=='message:' then local id=action:sub(9);for _,r in ipairs(rows('history')) do if r[1]==id and r[2]=='eu' and r[6]:sub(1,6)=='falhou' then draft=r[5];message='Escolha o canal comum aos rádios e toque em Enviar. ' end end
 elseif action=='channel_read' then send('channel_read')
 elseif action=='channel_new' then
  local occupied={};for _,r in ipairs(rows('channels')) do if r[3]~='0' then occupied[tonumber(r[1])]=true end end
  local free=nil;for i=1,7 do if not occupied[i] then free=i;break end end
  if free then creating=true;editChannel=tostring(free);channelDraft='\n';page='channel_edit';message='Novo canal secundário' else message='Os oito canais estão ocupados. Edite ou desative um secundário.' end
 elseif action:sub(1,8)=='channel:' then creating=false;editChannel=action:sub(9);for _,r in ipairs(rows('channels')) do if r[1]==editChannel then channelDraft=r[2]..'\n'..(r[4] or '') end end;page='channel_edit'
 elseif action=='channel_save' then local f=store.lines(value or '');if send((creating and 'channel_create' or 'channel_save')..'\n'..editChannel..'\n'..(f[1] or '')..'\n'..(f[2] or '')) then page='channels' end
 elseif action=='channel_disable' then if send('channel_disable\n'..editChannel..'\n\n') then page='channels' end
 elseif action=='next' then index=index+1 elseif action=='prev' then index=math.max(0,index-1)
 end
 local b={};local fields={};local status=rows('status')[1] or {'','0','Desconectado','0'}
 local body=message~='' and message or status[3]
 if page=='main' then
  local own=status[9];if not own or own=='' then own=name(status[4]) end;if own==name('0') or own:sub(1,1)=='!' then own='Nome aguardando sincronização' end
  fields={{label='Rádio',value=(status[1]=='uart' and 'RAK / Grove' or status[1]=='ble' and 'BLE / C6' or 'Nenhum')..' · '..(status[2]=='1' and 'Pronto' or status[3])},{label='Nó local / caixa de saída',value=own..' · '..tostring(#rows('outbox'))..' pendentes'}}
  b={{id='connect',label='Conectar',icon='network'},{id='history',label='Mensagens',icon='notes'},{id='nodes',label='Nós',icon='contacts'},{id='channels',label='Canais',icon='network'},{id='outbox',label='Pendentes',icon='ui_export'},{id='disconnect',label='Desconectar',icon='ui_close'}}
  if kind=='ble' then b[#b+1]={id='scan',label='Buscar rádios',icon='ui_search'} end;b[#b+1]={id='diagnostic',label='Diagnóstico',icon='ui_info'}
  return chrome.view({title=title,layout='status',body=body,statusFields=fields,buttons=b,refreshMs=1000})
 elseif page=='diagnostic' then
  local code,origin,reason=(status[14] or ''):match('^código (%d+) · (.-) · (.*)$')
  local region=tostring(status[13] or 'não informada');if region=='6' then region='6 (ANZ)' end
  diagnosticText='CÓDIGO: '..(code or 'nenhum')..'\nORIGEM: '..(origin or 'não informada')..'\nDESTINO: '..tostring(status[15] or 'Nenhum')..'\nCHAVE PÚBLICA LOCAL: '..tostring(status[10] or 'não informada')..'\nCHAVE PRIVADA NO RÁDIO: '..tostring(status[11] or 'não informada')..'\nLICENCIADO: '..tostring(status[12] or 'não informado')..' · REGIÃO: '..region..'\nMOTIVO: '..(reason or status[3] or '')..'\n\n'..(sys.radio('diagnostic') or 'Rádio indisponível')..'\nEcos de ToRadio='..tostring(status[5] or '0')..'\nRespostas FromRadio='..tostring(status[6] or '0')..'\nConfig ID esperado='..tostring(status[7] or '0')..'\nConfig ID recebido='..tostring(status[8] or '0')..'\nCriptografia=decidida pelo RAK (sem flag/chave do app)'
  return chrome.view({title=title..' · diagnóstico',layout='reader',field=true,readonly=true,text=diagnosticText,body=message,refreshMs=1000,footer={{id='save_diagnostic',label='Salvar diagnóstico',icon='ui_save'},{id='wiring',label='Ligação Grove',icon='mesh_rak'}}})
 elseif page=='wiring' then return chrome.view({title='RAK3172H / Grove',layout='reader',field=true,readonly=true,body='Ligação utilizada pelo Tab5-Meshtastic',text='Meshtastic PROTO, UART 115200 8N1.\nTab5 TX GPIO53 -> RAK RX PB7\nTab5 RX GPIO54 <- RAK TX PB6\nGND comum; lógica 3,3 V.\nA conexão habilita a alimentação Grove para a placa adaptadora compatível. Não altera região nem firmware do rádio.',footer={{id='confirm_uart',label='Conectar',icon='network'}}})
 elseif page=='pair' then return chrome.view({title='Parear Meshtastic BLE',layout='form',field=true,body=body,text=target..'\n',controls={{label='Endereço MAC,tipo (0 público / 1 aleatório)',type='text'},{label='PIN do rádio (seis dígitos)',type='password'}},footer={{id='connect_ble',label='Conectar',icon='network'}}})
 elseif page=='channel_edit' then return chrome.view({title='Canal '..editChannel,layout='form',field=true,body='Nome até 12 bytes. Chave hex: 2/32/64 dígitos; vazia = sem criptografia.',text=channelDraft,controls={{label='Nome (até 12 bytes)',type='text'},{label='Chave PSK hexadecimal',type='password'}},footer={{id='channel_save',label='Salvar no rádio',icon='ui_save'},{id='channel_disable',label='Desativar',icon='ui_stop'}}})
 elseif page=='chat' then
  local options,keys=choices();local dest,destkeys=peers();local list={}
  for _,r in ipairs(rows('history')) do
   local outgoing=r[2]=='eu';local to=r[7] or '4294967295';local matches=peer=='4294967295' and to=='4294967295' or peer~='4294967295' and ((outgoing and to==peer) or (not outgoing and r[2]==peer and to~='4294967295'))
   local failed=outgoing and r[6]:sub(1,6)=='falhou'
   if (peer~='4294967295' or r[4]==channel) and matches then list[#list+1]={id='message:'..r[1],label=r[5]..(failed and '\n\n'..r[6] or ''),subtitle=(sys.date(tonumber(r[3]) or 0):sub(12,16))..' · '..(outgoing and 'Você' or name(r[2]))..' · '..(failed and 'falhou · toque para editar' or r[6]),icon='notes',outgoing=outgoing} end
  end
  if action=='refresh' and #list>lastChatCount and index>=math.max(0,math.floor((lastChatCount-1)/6)) then index=100000 end;lastChatCount=#list
  index=math.min(index,math.max(0,math.floor((#list-1)/6)));for i=index*6+1,math.min(index*6+6,#list) do b[#b+1]=list[i] end
  return chrome.view({title=title..' · Mensagens',layout='chat',field=true,text=peer..'\n'..channel..'\n'..draft,controls={{label='Conversa / destinatário',type='select',options=dest,keys=destkeys},{label='Canal solicitado',type='select',options=options,keys=keys},{label='Mensagem',type='text'}},body=body..' · '..tostring(index+1)..'/'..tostring(math.max(1,math.ceil(#list/6))),buttons=b,centerTools=true,tools=nav(),refreshMs=1000})
 end
 local list={};local footer={}
 if page=='devices' then local raw=sys.radio('devices') or '';for line in raw:gmatch('[^\n]+') do local address,n=line:match('^(.-)|(.*)$');if address then list[#list+1]={id='device:'..address,label=n,subtitle=address,icon='network'} end end
 elseif page=='nodes' then
  local all=nodeCache;table.sort(all,function(a,b)return a[2]:lower()<b[2]:lower()end)
  if selected=='' and all[1] then selected=all[1][1] end
  local node=nil;for _,r in ipairs(all) do if r[1]==selected then node=r end;list[#list+1]={id='node:'..r[1],label=r[2],outgoing=r[1]==selected,subtitle=(r[3] and r[3]~='' and r[3]..' · ' or '')..'SNR '..(r[5] and r[5]~='' and r[5] or '—')..' dB',icon='contacts'} end
  node=node or all[1];if node then selected=node[1];fields={{label='Nome / identificação',value=node[2]..' · '..(node[3] or '')},{label='Último contato',value=heard(node[4])},{label='Sinal / saltos',value=shown(node[5])..' dB · '..shown(node[6])..' saltos'},{label='Bateria / tensão',value=shown(node[7])..'% · '..shown(node[8])..' V'},{label='Posição / altitude',value=shown(node[9])..', '..shown(node[10])..' · '..shown(node[11])..' m'}};footer={{id='dm',label='Privada',icon='notes'}} end
 elseif page=='channels' then
  local all=rows('channels');table.sort(all,function(a,b)return tonumber(a[1])<tonumber(b[1])end)
  for _,r in ipairs(all) do list[#list+1]={id='channel:'..r[1],label=r[1]..' · '..r[2],subtitle=r[3]=='0' and 'Desativado' or r[3]=='1' and 'Primário' or 'Secundário',icon='network'} end
  local s=rows('channel-status')[1];if s then body=s[1] end
  footer={{id='channel_new',label='Novo canal',icon='ui_new'},{id='channel_read',label='Ler do rádio',icon='ui_refresh'}}
 else for _,r in ipairs(rows('outbox')) do list[#list+1]={id='pending:'..r[1],label=r[4],subtitle=name(r[2])..' · canal '..r[3]..' · tentativas '..r[5],icon='ui_export'} end end
 index=math.min(index,math.max(0,math.floor((#list-1)/6)));for i=index*6+1,math.min(index*6+6,#list) do b[#b+1]=list[i] end
 return chrome.view({title=title..' · '..(page=='nodes' and 'Nós' or page=='channels' and 'Canais' or page=='outbox' and 'Pendentes' or 'Rádios'),layout=page=='nodes' and 'mesh_nodes' or 'list',statusFields=fields,footer=footer,body=#list==0 and 'Nenhum item; conecte o rádio para sincronizar.' or body,buttons=b,refreshMs=1000,centerTools=true,tools=nav()})
end
return M
