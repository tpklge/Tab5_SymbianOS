-- SPDX-License-Identifier: MIT
local store=sys.module('store');local M={}
local active=sys.read('/data/bench/enabled')=='1'
local interval=tonumber((sys.read('/data/bench/interval'))) or 30
local nextSample=0;local sequence=tonumber((sys.read('/data/bench/sequence'))) or 0
local rows={}
if active then sequence=sequence%16+1;sys.write('/data/bench/sequence',tostring(sequence)) end
function M.event(action,value)
 if action=='bench:start' then
  active=true;interval=math.max(5,math.min(60,tonumber(value) or 30));nextSample=0;rows={};sequence=sequence%16+1
  sys.write('/data/bench/enabled','1');sys.write('/data/bench/interval',tostring(interval));sys.write('/data/bench/sequence',tostring(sequence))
 elseif action=='bench:stop' then active=false;sys.write('/data/bench/enabled','0') end
 if action~='tick' or not active then return end
 local now=sys.memory('time');if now<nextSample then return end;nextSample=now+interval*1000
 local m=sys.memory()
 if #rows==0 then store.write('/data/bench/session-'..sequence..'.tsv',{{'runtime',m.runtime,'release',m.release,'interval_s',interval},{'columns','kind','unix','uptime_ms','heap_free','heap_largest','psram_free','video_frames','video_bytes','media_ms','media_state','battery_ma','battery_mv','battery_percent','wifi'}}) end
 rows[#rows+1]={m.device and 'device' or 'host',m.unix,m.uptime_ms,m.internal_free,m.internal_largest,m.psram_free,m.video_frames,m.video_bytes,m.media_ms,m.media_state,m.power_available and m.battery_ma or 'NA',m.power_available and m.battery_mv or 'NA',m.power_available and m.battery_percent or 'NA',m.wifi_connected and '1' or '0'}
 local ok,why=store.write('/data/bench/trace-'..sequence..'.tsv',rows)
 if not ok then active=false;sys.write('/data/bench/enabled','0');sys.notify('bench-io','Registro interrompido: '..tostring(why),false,false) end
 if #rows>=256 then rows={};sequence=sequence%16+1;sys.write('/data/bench/sequence',tostring(sequence)) end
end
return M
