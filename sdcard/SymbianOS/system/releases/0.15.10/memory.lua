-- SPDX-License-Identifier: MIT
local chrome=sys.module("chrome")
local function kib(n)return string.format("%.1f KiB",n/1024) end
function event(action)
 if action=="back" or action=="home" then next_app="shell" end
 local m=sys.memory();local fields={}
 local function metric(label,used,total)
  fields[#fields+1]={label=label,value=kib(used).." / "..kib(total),percent=total>0 and math.floor(used*100/total) or 0}
 end
 if m.available then
  metric("Heap interna usada",m.internal_total-m.internal_free,m.internal_total)
  metric("PSRAM usada",m.psram_total-m.psram_free,m.psram_total)
  fields[#fields+1]={label="Heap livre / maior bloco",value=kib(m.internal_free).." / "..kib(m.internal_largest)}
  fields[#fields+1]={label="PSRAM livre / maior bloco",value=kib(m.psram_free).." / "..kib(m.psram_largest)}
  fields[#fields+1]={label="Mínimo histórico de heap livre",value=kib(m.internal_minimum)}
 else fields[#fields+1]={label="Hardware",value="Medição física indisponível no simulador"} end
 metric("Memória Lua / limite",m.lua_used,m.lua_limit)
 return chrome.view({title="Memória e sistema",layout="status",refreshMs=1000,body="Uso real do runtime • atualização automática",statusFields=fields,buttons={}})
end
