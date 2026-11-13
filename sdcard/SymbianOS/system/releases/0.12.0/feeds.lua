-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local section,selected,article,page="sources","","",0
local items,message,pending={},{},nil
message=""
local function clean(s)
 s=s:gsub("<!%[CDATA%[(.-)%]%]>","%1"):gsub("&lt;","<"):gsub("&gt;",">")
 s=s:gsub("<script[^>]*>.-</script>",""):gsub("<style[^>]*>.-</style>",""):gsub("<br%s*/?>","\n"):gsub("</p>","\n\n"):gsub("<[^>]*>","")
 s=s:gsub("&amp;","&"):gsub("&quot;",'"'):gsub("&apos;","'"):gsub("&nbsp;"," ")
 s=s:gsub("&#(%d+);",function(n)local v=tonumber(n);return v and v>0 and v<=1114111 and not(v>=55296 and v<=57343) and utf8.char(v) or "" end)
 return s:gsub("[ \t]+"," "):gsub("\n\n\n+","\n\n"):match("^%s*(.-)%s*$") or ""
end
local function parse(xml)
 local result={};local pattern=xml:find("<item[%s>]") and "<item[^>]*>(.-)</item>" or "<entry[^>]*>(.-)</entry>"
 for body in xml:gmatch(pattern) do
  local title=clean(body:match("<title[^>]*>(.-)</title>") or "Sem título")
  local text=body:match("<content:encoded[^>]*>(.-)</content:encoded>") or body:match("<content[^>]*>(.-)</content>") or body:match("<description[^>]*>(.-)</description>") or body:match("<summary[^>]*>(.-)</summary>") or "Este feed fornece apenas o título."
  local link=body:match("<link[^>]*>(.-)</link>") or body:match([=[<link[^>]-href=["'](.-)["']]=]) or ""
  result[#result+1]={title=store.clip(title,160),text=store.clip(clean(text),6000),link=clean(link)}
  if #result>=30 then break end
 end
 return result
end
local function cachePath() return "/data/feeds/cache-"..selected..".xml" end
function event(action,value)
 local sources,why=store.read("/data/feeds/sources.tsv")
 if not sources then return chrome.view({title="Feeds",body=tostring(why).." • arquivo preservado",buttons={}}) end
 if #sources==0 and not sys.read("/data/feeds/sources.tsv") then sources={{"gizmodo","Gizmodo","https://www.gizmodo.com.br/feed"}} end
 if pending and not sys.net().busy then
  local xml=sys.read("/data/feeds/latest.xml")
  if xml and #parse(xml)>0 and sys.net().message=="Feed atualizado" then
   sys.write("/data/feeds/cache-"..pending..".xml",xml)
  end
  pending=nil
 end
 if action=="configure" or action=="new" then section="configure"
 elseif action=="save_source" then
  local f=store.lines(value or "")
  if f[1] and f[1]~="" and #f[1]<=80 and f[2] and (f[2]:match("^https://") or f[2]:match("^http://")) and #sources<16 then
   selected=store.id(sources);sources[#sources+1]={selected,f[1],f[2]};local ok,why=store.write("/data/feeds/sources.tsv",sources);message=store.saveStatus(ok,why);section="sources"
  else message="Nome e URL HTTP(S) válidos; máximo 16 feeds." end
 elseif action:sub(1,7)=="source:" then selected=action:sub(8);section="articles";page=0
 elseif action=="update" then
  if selected=="" and sources[1] then selected=sources[1][1] end
  for _,source in ipairs(sources) do if source[1]==selected then
   local ok=sys.net("feed",source[3],"/data/feeds/latest.xml");message=ok and "Baixando… a lista será atualizada automaticamente." or "Rede ocupada, offline ou URL inválida.";if ok then pending=selected end;break end end
  section="articles"
 elseif action=="cancel" then sys.net("cancel")
 elseif action:sub(1,8)=="article:" then article=tonumber(action:sub(9)) or 1;section="reader"
 elseif action=="save_article" and items[article] then
  local saved=store.read("/data/feeds/saved.tsv") or {};if #saved<32 then
   local id=store.id(saved);local item=items[article];local path="/data/feeds/article-"..id..".txt"
   if sys.write(path,item.title.."\n\n"..item.text.."\n\n"..item.link) then saved[#saved+1]={id,item.title,path};local ok,why=store.write("/data/feeds/saved.tsv",saved);message=store.saveStatus(ok,why) end
  else message="Limite de 32 artigos salvos." end
 elseif action=="saved" then section="saved"
 elseif action:sub(1,6)=="saved:" then section="saved_reader";article=action:sub(7)
 elseif action=="remove_source" then section="confirm_remove"
 elseif action=="confirm_remove" then
  for i=#sources,1,-1 do if sources[i][1]==selected then table.remove(sources,i) end end
  store.write("/data/feeds/sources.tsv",sources);selected="";section="sources"
 elseif action=="next" then page=page+1 elseif action=="prev" then page=math.max(0,page-1)
 elseif action=="back" then if section=="sources" then next_app="shell" elseif section=="reader" then section="articles" else section="sources" end
 elseif action=="home" then next_app="shell" end
 if section=="configure" then return chrome.view({title="Adicionar feed",layout="form",field=true,text="\nhttps://",body=message,
  controls={{label="Nome",type="text"},{label="URL RSS / Atom",type="text"}},buttons={{id="save_source",label="Salvar feed"}}}) end
 if section=="saved" then local b={};for _,r in ipairs(store.read("/data/feeds/saved.tsv") or {}) do b[#b+1]={id="saved:"..r[1],label=r[2],icon="feeds"} end;return chrome.view({title="Artigos offline",layout="list",body="Artigos salvos no cartão",buttons=b}) end
 if section=="saved_reader" then local text="Artigo não encontrado.";for _,r in ipairs(store.read("/data/feeds/saved.tsv") or {}) do if r[1]==article then text=sys.read(r[3]) or text end end;return chrome.view({title="Leitura offline",layout="reader",field=true,readonly=true,text=text,buttons={}}) end
 if section=="confirm_remove" then return chrome.view({title="Remover assinatura?",layout="list",body="Artigos salvos serão preservados.",buttons={{id="confirm_remove",label="Remover assinatura"},{id="back",label="Cancelar"}}}) end
 if selected~="" then
  local xml=sys.read(cachePath()) or "";items=parse(xml)
  if pending==selected then local latest=sys.read("/data/feeds/latest.xml") or "";if sys.net().message=="Feed atualizado" then items=parse(latest);sys.write(cachePath(),latest) end end
 end
 if section=="reader" then local item=items[article];return chrome.view({title=item and item.title or "Artigo",layout="reader",field=true,readonly=true,text=item and item.text.."\n\n"..item.link or "Artigo indisponível.",body=message,buttons={},tools={{id="save_article",label="Salvar offline",icon="ui_save"},{id="back",label="Artigos",icon="ui_prev"}}}) end
 local buttons={}
 if section=="articles" then
  page=math.min(page,math.max(0,math.floor((#items-1)/6)))
  for i=page*6+1,math.min(page*6+6,#items) do buttons[#buttons+1]={id="article:"..i,label=items[i].title,subtitle=store.clip(items[i].text,80),icon="feeds"} end
  return chrome.view({title="Artigos • "..#items,layout="list",refreshMs=1000,body=sys.net().message.." • "..message,buttons=buttons,
   tools={{id="update",label="Baixar",icon="network"},{id="prev",label="Anterior",icon="ui_prev"},{id="next",label="Próxima",icon="ui_next"},{id="remove_source",label="Remover",icon="ui_delete"}}})
 end
 for _,source in ipairs(sources) do buttons[#buttons+1]={id="source:"..source[1],label=source[2],subtitle=source[3],icon="feeds"} end
 return chrome.view({title="Feeds e leitura",layout="list",body="Assinaturas e artigos disponíveis offline",buttons=buttons,tools={{id="new",label="Adicionar",icon="ui_new"},{id="saved",label="Salvos",icon="ui_save"}}})
end
