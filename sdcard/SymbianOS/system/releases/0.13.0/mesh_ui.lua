-- SPDX-License-Identifier: MIT
local chrome,store=sys.module('chrome'),sys.module('store')
local M={};local page='main';local message='';local index=0;local target='';local text=''
local function rows(name) return store.read('/data/mesh/'..name..'.tsv') or {} end
local function send(command) local ok,why=sys.send('services','mesh',command);message=ok and '' or tostring(why or 'Não foi possível executar o comando') end
function M.view(kind,action,value)
 local title=kind=='ble' and 'Meshtastic BLE' or 'Meshtastic RAK'
 if action=='home' then next_app='shell'
 elseif action=='back' then if page=='main' then next_app='shell' else page='main' end
 elseif action=='connect' then if kind=='uart' then send('uart');page='main' else page='pair' end
 elseif action=='confirm_uart' then send('uart');page='main'
 elseif action=='scan' then send('scan');page='devices'
 elseif action:sub(1,7)=='device:' then target=action:sub(8);page='pair'
 elseif action=='connect_ble' then local f=store.lines(value or '');if f[1] and f[1]:match('^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x,[01]$') and f[2] and f[2]:match('^%d%d%d%d%d%d$') then target=f[1];send('ble\n'..target..','..f[2]);page='main' else message='Endereço BLE válido e PIN de seis dígitos necessários.' end
 elseif action=='diagnostic' then page='diagnostic'
 elseif action=='wiring' then page='wiring'
 elseif action=='disconnect' then send('disconnect')
 elseif action=='compose' then page='compose'
 elseif action=='send' then local f=store.lines(value or '');local node=(f[1] or ''):gsub('^!','0x');local destination=tonumber(node);local channel=tonumber(f[2]);local body=f[3] or ''
  if destination and destination%1==0 and destination>=1 and destination<=0xffffffff and channel and channel%1==0 and channel>=0 and channel<=7 and #body>0 and #body<=228 then send('send\n'..destination..'\n'..channel..'\n'..body);page='main' else message='Destino numérico ou !hex, canal 0–7 e texto até 228 bytes.' end
 elseif action=='history' or action=='nodes' or action=='channels' or action=='outbox' then page=action;index=0
 elseif action=='next' then index=index+1 elseif action=='prev' then index=math.max(0,index-1)
 elseif action:sub(1,5)=='read:' then text=action:sub(6);page='read' end
 local b={};local fields={};local status=rows('status')[1] or {'','0','Desconectado','0'}
 if action=='refresh' then message='' end
 local body=message~='' and message or status[3]
 if page=='main' then
  fields={{label='Rádio',value=(status[1]=='uart' and 'RAK / Grove' or status[1]=='ble' and 'BLE / C6' or 'Nenhum')..' • '..(status[2]=='1' and 'Pronto' or status[3])},{label='Nó / caixa de saída',value=status[4]..' • '..tostring(#rows('outbox'))..' pendentes'}}
  b={{id='connect',label='Conectar',icon='network'},{id='compose',label='Escrever',icon='notes'},{id='history',label='Conversas',icon='contacts'},{id='nodes',label='Nós',icon='contacts'},{id='channels',label='Canais',icon='network'},{id='outbox',label='Pendentes',icon='ui_export'},{id='disconnect',label='Desconectar',icon='ui_close'}}
  if kind=='ble' then b[#b+1]={id='scan',label='Buscar rádios',icon='ui_search'} end
  b[#b+1]={id='diagnostic',label='Diagnóstico',icon='ui_info'}
  return chrome.view({title=title,layout='status',body=body,statusFields=fields,buttons=b,refreshMs=1000})
 elseif page=='diagnostic' then return chrome.view({title=title..' • diagnóstico',layout='reader',field=true,readonly=true,text=(sys.radio('diagnostic') or 'Rádio indisponível')..'\n\n'..status[3],refreshMs=1000,footer={{id='wiring',label='Ligação Grove',icon='mesh_rak'}}})
 elseif page=='wiring' then
  return chrome.view({title='RAK3172H / Grove',layout='reader',field=true,readonly=true,body='Confirme a ligação antes de conectar',text='Meshtastic PROTO, UART 115200 8N1.\nTab5 TX GPIO53 -> RAK RX PB7\nTab5 RX GPIO54 <- RAK TX PB6\nGND comum; lógica 3,3 V.\n5 V somente em placa adaptadora compatível, nunca no RAK nu. Esta conexão não altera alimentação nem região do rádio.',footer={{id='confirm_uart',label='Conectar',icon='network'},{id='back',label='Cancelar',icon='ui_cancel'}}})
 elseif page=='pair' then
  return chrome.view({title='Parear Meshtastic BLE',layout='form',field=true,body=body,text=target..'\n',controls={{label='Endereço MAC,tipo (0 público / 1 aleatório)',type='text'},{label='PIN do rádio (seis dígitos)',type='password'}},footer={{id='connect_ble',label='Conectar',icon='network'}}})
 elseif page=='compose' then
  local channels=rows('channels');local options,keys={},{}
  for _,r in ipairs(channels) do options[#options+1]=r[2];keys[#keys+1]=r[1] end
  if #options==0 then options={'Canal 0'};keys={'0'} end
  return chrome.view({title='Nova mensagem',layout='form',field=true,body=body,text='4294967295\n'..keys[1]..'\n',controls={{label='Destino (4294967295 = todos)',type='text'},{label='Canal do rádio',type='select',options=table.concat(options,'\n'),keys=table.concat(keys,'\n')},{label='Texto (até 228 bytes UTF-8)',type='text'}},footer={{id='send',label='Enviar',icon='ui_export'}}})
 elseif page=='read' then return chrome.view({title=title,layout='reader',field=true,readonly=true,body='Histórico local',text=text})
 end
 local list={}
 if page=='devices' then
  local raw=sys.radio('devices') or '';for line in raw:gmatch('[^\n]+') do local address,name=line:match('^(.-)|(.*)$');if address then list[#list+1]={id='device:'..address,label=name,subtitle=address,icon='network'} end end
 else
  local all=rows(page)
  for i=#all,1,-1 do local r=all[i]
   if page=='history' then list[#list+1]={id='read:'..r[5]..'\n\n'..r[2]..' • canal '..r[4]..' • '..r[6],label=r[5],subtitle=(sys.date(tonumber(r[3]) or 0):sub(12,16))..' • '..r[2]..' • '..r[6],icon='notes',outgoing=r[2]=='eu'}
   elseif page=='nodes' then list[#list+1]={id='read:'..r[2]..'\nNó '..r[1],label=r[2],subtitle=r[1],icon='contacts'}
   elseif page=='channels' then list[#list+1]={id='read:Canal '..r[1]..' • '..r[2],label=r[2],subtitle='Canal '..r[1],icon='network'}
   else list[#list+1]={id='read:'..r[4],label=store.clip(r[4],38),subtitle='Pendente • tentativas '..r[5],icon='ui_export'} end
  end
 end
 index=math.min(index,math.max(0,math.floor((#list-1)/6)));for i=index*6+1,math.min(index*6+6,#list) do b[#b+1]=list[i] end
 return chrome.view({title=title..' • '..page,layout=page=='history' and 'chat' or 'packages',footer=page=='history' and {{id='compose',label='Escrever',icon='notes'}} or {},body=#list==0 and 'Nenhum item; conecte o rádio para sincronizar.' or body,buttons=b,refreshMs=1000,tools={{id='prev',label='Anterior',icon='ui_prev'},{id='next',label='Próxima',icon='ui_next'}}})
end
return M
