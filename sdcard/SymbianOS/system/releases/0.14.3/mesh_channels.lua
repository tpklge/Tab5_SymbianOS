-- SPDX-License-Identifier: MIT
-- Original asynchronous local-radio administration. Never treats a write as confirmation.
local store,p=sys.module('store'),sys.module('mesh_proto')
local M={};local job=nil;local seq=0;local own=0;local issue=''
local function rows() return store.read('/data/mesh/channels.tsv') or {} end
local function status(s) issue=s;store.write('/data/mesh/channel-status.tsv',{{s}}) end
local function hex(s) return (s:gsub('.',function(c)return string.format('%02x',c:byte())end)) end
local function unhex(s) if #s%2~=0 or s:find('[^%x]') then return nil end;return (s:gsub('%x%x',function(c)return string.char(tonumber(c,16))end)) end
function M.record(raw)
 local c=p.decode(raw);if not c then return nil end;local index=c[1] or 0;local role=c[3] or 0
 if type(index)~='number' or type(role)~='number' or index<0 or index>7 or role<0 or role>2 or (c[2]~=nil and type(c[2])~='string') then return nil end
 local settings=p.decode(c[2] or '');if not settings or (settings[2]~=nil and type(settings[2])~='string') or (settings[3]~=nil and (type(settings[3])~='string' or not utf8.len(settings[3]))) then return nil end;local row={tostring(index),settings[3] or ('Canal '..index),tostring(role),hex(settings[2] or ''),hex(c[2] or '')}
 local all=rows();local found=false;for i,r in ipairs(all) do if r[1]==row[1] then all[i]=row;found=true end end;if not found then all[#all+1]=row end
 store.write('/data/mesh/channels.tsv',all);return c,settings
end
local function request(index,stage)
 seq=(seq+1)&0x7fffffff;if seq==0 then seq=1 end
 job.id=0x80000000|seq;job.stage=stage;job.at=sys.epoch();job.tries=(job.tries or 0)+1
 local ok,why=sys.radio('send',p.admin(job.id,own,p.number(1,index+1),true));if not ok then status(tostring(why));job=nil end
end
function M.command(op,f,ready,node)
 if not ready or node==0 then status('Conecte e sincronize o rádio primeiro.');return end
 if job then status('Aguarde a operação de canais em andamento.');return end
 own=node
 if op=='channel_read' then job={index=0,read=true};status('Lendo os oito canais do rádio...');request(0,'read');return end
 local index=tonumber(f[2]);local name=f[3] or '';local key=f[4] or '';local bytes=unhex(key)
 if not index or index%1~=0 or index<0 or index>7 or #name>12 or not utf8.len(name) or not bytes or (#bytes~=0 and #bytes~=1 and #bytes~=16 and #bytes~=32) then status('Canal 0–7, nome até 12 bytes e chave hex de 0, 2, 32 ou 64 dígitos.');return end
 job={index=index,name=name,key=bytes,disable=op=='channel_disable',create=op=='channel_create'};status('Consultando canal e autorização do rádio...');request(index,'prepare')
end
function M.receive(packet,data,node)
 if not job or not data[6] or data[6]~=job.id or (packet[1]~=nil and packet[1]~=0 and packet[1]~=node) then return false end
 if data[1]==5 then local routing=p.decode(data[2] or '') or {};if (routing[3] or 0)~=0 then status('Rádio recusou a alteração: '..tostring(routing[3]));job=nil end;return true end
 if data[1]~=6 then return false end
 local a=p.decode(data[2] or '');if not a or not a[2] then return false end
 local c,settings=M.record(a[2]);if not c or (c[1] or 0)~=job.index then return false end
 if job.stage=='read' then
  if job.index==7 then status('Canais atualizados do rádio.');job=nil else job.index=job.index+1;job.tries=0;request(job.index,'read') end
 elseif job.stage=='prepare' then
  local role=c[3] or 0
  if job.create and role~=0 then status('O canal escolhido está ocupado; releia os canais.');job=nil;return true end
  if job.disable and role==1 then status('O canal primário não pode ser desativado.');job=nil;return true end
  job.role=job.disable and 0 or (role==1 and 1 or 2)
  -- Preserve every unrelated settings field, including unknown future fields.
  local rawSettings=p.replace(c[2] or '',{[2]=p.bytes(2,job.key),[3]=p.bytes(3,job.name)})
  job.expected=p.number(1,job.index)..p.bytes(2,rawSettings)..p.number(3,job.role)
  local admin=p.bytes(33,job.expected)
  if type(a[101])=='string' then admin=admin..p.bytes(101,a[101]) end
  seq=(seq+1)&0x7fffffff;local ok,why=sys.radio('send',p.admin(0x80000000|seq,node,admin,false))
  if not ok then status(tostring(why));job=nil;return true end
  job.stage='settle';job.at=sys.epoch();job.tries=0;status('Alteração enviada; aguardando conferência no rádio...')
 elseif job.stage=='verify' then
  if (c[3] or 0)==job.role and (job.role==0 or (settings[3] or '')==job.name and (settings[2] or '')==job.key) then status('Canal confirmado pelo rádio.') else status('O rádio não confirmou os valores solicitados. Releia os canais.') end
  job=nil
 end
 return true
end
function M.tick(ready,node)
 if not job then return end
 if not ready or node~=own then job=nil;status('Operação interrompida: rádio desconectado.');return end
 if job.stage=='settle' then if sys.epoch()>job.at then request(job.index,'verify') end
 elseif sys.epoch()-job.at>=4 then
  if job.tries>=3 then job=nil;status('Sem resposta administrativa; alteração não confirmada.') else request(job.index,job.stage) end
 end
end
return M
