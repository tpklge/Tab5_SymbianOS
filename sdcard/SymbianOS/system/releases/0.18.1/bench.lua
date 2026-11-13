-- SPDX-License-Identifier: MIT
local chrome,store=sys.module('chrome'),sys.module('store')
local message='';local started=0;local startFrames=0;local startBytes=0
function event(action)
 if action=='ota_diagnostic' then next_app='updates'
 elseif action=='home' or action=='back' then next_app='shell'
 elseif action=='start' or action=='start_fast' then
  local m=sys.memory();started=m.uptime_ms;startFrames=m.video_frames;startBytes=m.video_bytes
  sys.send('services','bench:start',action=='start_fast' and '5' or '30');message='Monitoramento em segundo plano iniciado'
 elseif action=='stop' then sys.send('services','bench:stop','');message='Monitoramento encerrado'
 elseif action=='sd_test' then
  local data=string.rep('Tab5-SD-check-0123456789\n',1024);local begin=sys.memory('time')
  local ok,why=sys.write('/data/bench/io-test.tmp',data)
  if ok then local read=sys.read('/data/bench/io-test.tmp');ok=read==data;sys.fs('remove','/data/bench/io-test.tmp') end
  local ms=sys.memory('time')-begin;message=ok and ('SD: '..#data..' bytes • '..ms..' ms') or tostring(why or 'Falha na verificação do SD')
  store.write('/data/bench/sd.tsv',{{'result',ok and 'pass' or 'fail','bytes',#data,'duration_ms',ms}})
 elseif action=='ota_review' then
  local ok,why=sys.update('inspect');message=tostring(why);store.write('/data/bench/ota-review.tsv',{{'review_only',ok and 'pass' or 'fail',message}})
 end
 local m=sys.memory();if started==0 or m.video_frames<startFrames then started=m.uptime_ms;startFrames=m.video_frames;startBytes=m.video_bytes end;local elapsed=started>0 and math.max(0,m.uptime_ms-started)/1000 or 0
 local fields={{label='Origem das medições',value=m.device and 'Tab5' or 'Simulador • não é medição física'},
  {label='Heap livre / maior bloco',value=tostring(m.internal_free)..' / '..tostring(m.internal_largest)},
  {label='PSRAM livre',value=tostring(m.psram_free)},{label='Áudio / reprodução',value=m.media_state..' • '..math.floor(m.media_ms/1000)..' s'},
  {label='Vídeo / quadros / bytes',value=m.video_frames..' / '..m.video_bytes},
  {label='Taxa de vídeo no intervalo',value=elapsed>0 and string.format('%.1f fps • %.1f KiB/s',(m.video_frames-startFrames)/elapsed,(m.video_bytes-startBytes)/elapsed/1024) or '—'},
  {label='Bateria / corrente / tensão',value=m.power_available and (m.battery_percent..'% • '..m.battery_ma..' mA • '..m.battery_mv..' mV') or 'Indisponível'},
  {label='Registro no cartão',value='/data/bench • '..(sys.read('/data/bench/enabled')=='1' and 'Ativo' or 'Parado')}}
 return chrome.view({title='Testes e medições',layout='status',centerTools=true,refreshMs=2000,body=message~='' and message or 'Monitore enquanto usa música, notas, câmera ou rede. Até 16 registros rotativos.',statusFields=fields,
  tools={{id='ota_diagnostic',label='OTA',icon='ui_info'}},footer={{id='start',label='Monitorar • 30 s',icon='ui_play'},{id='start_fast',label='Monitorar • 5 s',icon='ui_play'},{id='stop',label='Parar registro',icon='ui_stop'},{id='sd_test',label='Verificar SD',icon='files'},{id='ota_review',label='Revisar OTA',icon='updates'}}})
end
