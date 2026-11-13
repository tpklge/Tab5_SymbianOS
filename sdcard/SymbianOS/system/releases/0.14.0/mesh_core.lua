-- SPDX-License-Identifier: MIT
-- Services VM alone owns the transport, protocol and bounded offline queue.
local store,p=sys.module('store'),sys.module('mesh_proto')
local channels=sys.module('mesh_channels')
local M={};local ownName='';local root='/data/mesh/'
local active,ready,nonce,lastAttempt,lastSend='',false,0,0,0
local message='Desconectado';local myNode=0;local reconnectAt=0;local syncTries=0;local syncStarted=0;local lastRequest="";local echoes,replies=0,0;local requestSequence,lastConfig=0,0
local function rows(name) return store.read(root..name..'.tsv') or {} end
local function save(name,r) return store.write(root..name..'.tsv',r) end
local lastStatus=nil;local connection=rows('connection')[1]
local function snapshot()
 local value={active,ready and '1' or '0',message,tostring(myNode),tostring(echoes),tostring(replies),tostring(requestSequence),tostring(lastConfig),ownName};local key=table.concat(value,'|')
 if key~=lastStatus and save('status',{value}) then lastStatus=key end
end
local function history(row)
 local all=rows('history');for _,r in ipairs(all) do if r[1]==row[1] and r[2]==row[2] then return false end end
 all[#all+1]=row;while #all>80 do table.remove(all,1) end
 if not save('history',all) then message='Falha ao salvar histórico';return false end;return true
end
local function txstate(id,status)
 local all=rows('history');for _,r in ipairs(all) do if r[1]==tostring(id) and r[2]=='eu' then if r[6]~='confirmada' then r[6]=status end end end;save('history',all)
end
local function updateNode(id,n)
 local all=rows('nodes');local row=nil
 for _,r in ipairs(all) do if r[1]==tostring(id) then row=r end end
 if not row then if #all>=128 then return end;row={tostring(id),string.format('!%08x',id)};all[#all+1]=row end
 for i=3,12 do row[i]=row[i] or '' end
 local user=p.decode(n[2] or '');if user then row[2]=user[2] or user[3] or row[2];row[3]=user[3] or row[3];row[12]=tostring(user[5] or '') end
 row[4]=tostring(n[5] or tonumber(row[4]) or 0)
 if n[4] then local f=p.float(n[4]);if f then row[5]=string.format('%.1f',f) end end
 if n[9] then row[6]=tostring(n[9]) end
 local metrics=p.decode(n[6] or '');if metrics then if metrics[1] then row[7]=tostring(metrics[1]) end;local voltage=p.float(metrics[2]);if voltage then row[8]=string.format('%.2f',voltage) end end
 local pos=p.decode(n[3] or '');if pos then if pos[1] and pos[2] then row[9]=string.format('%.6f',p.signed(pos[1])/1e7);row[10]=string.format('%.6f',p.signed(pos[2])/1e7) end;if pos[3] then row[11]=tostring(p.signed(pos[3]&0xffffffff)) end end
 if id==myNode then ownName=row[2] end
 save('nodes',all)
end
local function process(frame)
 if #lastRequest>0 and frame==lastRequest then echoes=echoes+1;message='Eco da solicitação; aguardando resposta Meshtastic';return end
 local f=p.decode(frame);if not f then message='Quadro Meshtastic inválido';return end
 if type(f[3])=='number' then message='Quadro ToRadio recebido no lugar de FromRadio';return end
 replies=replies+1
 if nonce~=0 and not ready then lastAttempt=sys.epoch() end
 if f[3] then local n=p.decode(f[3]);if n then myNode=n[1] or 0 end end
 if type(f[7])=='number' then lastConfig=f[7];if nonce~=0 and f[7]==nonce then ready=true;message='Sincronizado com o rádio' else message='Identificador de sincronização divergente' end end
 if f[8] then ready=false;nonce=0;message='Rádio reiniciou' end
 if f[4] then local node=p.decode(f[4]);if node and node[1] then updateNode(node[1],node) end end
 if f[10] then channels.record(f[10]) end
 if f[11] then local queue=p.decode(f[11]);if queue and queue[4] then txstate(queue[4],(queue[1] or 0)==0 and 'entregue ao rádio' or 'recusada pelo rádio') end end
 if f[2] then
  local packet=p.decode(f[2]);if not packet then return end
  local data=p.decode(packet[4] or '');if not data then return end
  if channels.receive(packet,data,myNode) then return end
  if data[1]==4 and packet[1] then updateNode(packet[1],{[2]=data[2],[5]=sys.epoch()})
  elseif data[1]==3 and packet[1] then updateNode(packet[1],{[3]=data[2],[5]=sys.epoch()})
  elseif data[1]==67 and packet[1] then local telemetry=p.decode(data[2] or '') or {};if telemetry[2] then updateNode(packet[1],{[6]=telemetry[2],[5]=sys.epoch()}) end
  elseif data[1]==1 and type(data[2])=='string' and #data[2]<=228 and utf8.len(data[2]) then
   local from=tostring(packet[1] or 0);local id=tostring(packet[6] or 0)
   if myNode~=0 and from==tostring(myNode) then txstate(id,'enviado pelo rádio')
   elseif history({id,from,tostring(sys.epoch()),tostring(packet[3] or 0),data[2],'recebida',tostring(packet[2] or 0xffffffff)}) then sys.notify('mesh:'..from..':'..id,'Meshtastic: '..store.clip(data[2],50),false,(sys.read('/config/public/profile') or 'normal')=='normal') end
  elseif data[1]==5 and data[6] then
   local routing=p.decode(data[2] or '') or {};txstate(data[6],(routing[3] or 0)==0 and 'confirmada' or 'falhou: rádio '..tostring(routing[3]))
  end
 end
end
function M.command(value)
 local f=store.lines(value or '');local op=f[1]
 if op=='disconnect' then sys.radio('disconnect');active='';ready=false;nonce=0;message='Desconectado';connection=nil;save('connection',{})
 elseif op=='uart' or op=='ble' then
  ready=false;nonce=0;lastRequest='';echoes=0;replies=0;lastConfig=0;lastAttempt=sys.epoch();active=op;syncTries=0
  local ok,why=sys.radio(op=='uart' and 'connect_uart' or 'connect_ble',f[2] or '')
  message=ok and 'Aguardando rádio' or tostring(why)
  -- Never persist the BLE PIN. Bond storage handles later reconnection.
  if ok then connection={op,(f[2] or ''):match('^(.-),%d+$') or ''};save('connection',{connection}) end
 elseif op=='channel_read' or op=='channel_save' or op=='channel_create' or op=='channel_disable' then channels.command(op,f,ready,myNode)
 elseif op=='scan' then local ok,why=sys.radio('scan');message=ok and 'Buscando BLE' or tostring(why)
 elseif op=='send' then
  local to,channel=tonumber(f[2]),tonumber(f[3]);local text=f[4] or ''
  if not to or to%1~=0 or to<1 or to>0xffffffff or not channel or channel%1~=0 or channel<0 or channel>7 or #text<1 or #text>228 or not utf8.len(text) then message='Destino/canal inválido ou mensagem acima de 228 bytes';snapshot();return end
  local queue=rows('outbox');if #queue>=32 then message='Caixa de saída cheia (32 mensagens)';snapshot();return end
  local counter=tonumber(sys.read(root..'counter') or '') or 0;counter=(counter+1)&0xffffffff;if counter==0 then counter=1 end
  if not sys.write(root..'counter',tostring(counter)) then message='Falha ao reservar ID';snapshot();return end
  local id=((sys.epoch()&0xffff)<<16)|(counter&0xffff)
  queue[#queue+1]={tostring(id),tostring(to),tostring(channel),text,'0','0'}
  if save('outbox',queue) then history({tostring(id),'eu',tostring(sys.epoch()),tostring(channel),text,'pendente',tostring(to)});message='Mensagem na caixa de saída' else message='Falha ao salvar caixa de saída' end
 end
 snapshot()
end
function M.tick()
 if (sys.read('/config/public/profile') or '')=='offline' then if active~='' then sys.radio('disconnect');active='';ready=false;nonce=0;message='Perfil offline' end;snapshot();return end
 local state=sys.radio('state') or 'Rádio indisponível'
 if (state=='Desconectado' or state=='BLE desconectado') and sys.epoch()>=reconnectAt then
  if connection then reconnectAt=sys.epoch()+30;M.command(connection[1]..'\n'..(connection[2] or '')..(connection[1]=='ble' and ',0' or '')) end
 end
 local liveLink=sys.radio('link') or '';local connected=liveLink~=''
 if connected and active=='' then active=liveLink;nonce=0;ready=false;syncTries=0 end
 if not connected and active~='' then message=state end
 if active~='' and connected and nonce==0 and syncTries<(active=='uart' and 12 or 3) then syncTries=syncTries+1;requestSequence=(requestSequence+1)&0xffffffff;if requestSequence==0 then requestSequence=1 end;nonce=requestSequence;lastRequest=p.number(3,nonce);local ok,why=sys.radio('send',lastRequest);if not ok then message=tostring(why);nonce=0 else lastAttempt=sys.epoch();syncStarted=sys.epoch() end end
 for _=1,6 do local frame=sys.radio('read');if not frame or #frame==0 then break end;process(frame) end
 if nonce~=0 and not ready and (sys.epoch()-lastAttempt>(active=='uart' and 5 or 20) or sys.epoch()-syncStarted>300) then nonce=0;message=echoes>0 and replies==0 and 'Sincronização esgotada: apenas eco, sem resposta do rádio' or 'Tempo de sincronização esgotado' end
 if ready and not connected then ready=false;nonce=0;message=state end
 channels.tick(ready,myNode)
 if ready and sys.epoch()-lastSend>=3 then
  local queue=rows('outbox');local row=queue[1]
  if row and sys.epoch()>=tonumber(row[6]) then
   local bytes=p.text(tonumber(row[1]),tonumber(row[2]),tonumber(row[3]),row[4]);local ok,why
   if bytes then ok,why=sys.radio('send',bytes) end
   lastSend=sys.epoch()
   if ok then txstate(row[1],'aceita pelo transporte');table.remove(queue,1)
   else row[5]=tostring((tonumber(row[5]) or 0)+1);row[6]=tostring(sys.epoch()+10);message=tostring(why or 'Mensagem inválida');if tonumber(row[5])>=3 then txstate(row[1],'falhou');table.remove(queue,1) end end
   save('outbox',queue)
  end
 end
 snapshot()
end
return M
