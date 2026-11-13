-- SPDX-License-Identifier: MIT
local chrome,store=sys.module("chrome"),sys.module("store")
local section,selected,article,page="sources","","",0
local items,message,pending={},{},nil
local plain,imagesPending=false,false
local errorDetail=""
message=""
local function clean(s)
 s=s:gsub("<!%[CDATA%[(.-)%]%]>","%1"):gsub("&lt;","<"):gsub("&gt;",">")
 s=s:gsub("<script[^>]*>.-</script>",""):gsub("<style[^>]*>.-</style>",""):gsub("<br%s*/?>","\n"):gsub("</p>","\n\n"):gsub("<[^>]*>","")
 s=s:gsub("&amp;","&"):gsub("&quot;",'"'):gsub("&apos;","'"):gsub("&nbsp;"," ")
 s=s:gsub("&#(%d+);",function(n)local v=tonumber(n);return v and v>0 and v<=1114111 and not(v>=55296 and v<=57343) and utf8.char(v) or "" end)
 return s:gsub("[ \t]+"," "):gsub("\n\n\n+","\n\n"):match("^%s*(.-)%s*$") or ""
end
local function markup(s)
 local c=s:match("^%s*<!%[CDATA%[(.*)%]%]>%s*$")
 if c then return c end
 return s:gsub("&lt;","<"):gsub("&gt;",">"):gsub("&quot;",'"'):gsub("&apos;","'"):gsub("&amp;","&")
end
local function parse(xml)
 local result={};local pattern=xml:find("<item[%s>]") and "<item[^>]*>(.-)</item>" or "<entry[^>]*>(.-)</entry>"
 for body in xml:gmatch(pattern) do
  local title=clean(body:match("<title[^>]*>(.-)</title>") or "Sem título")
  local text=body:match("<content:encoded[^>]*>(.-)</content:encoded>") or body:match("<content[^>]*>(.-)</content>") or body:match("<description[^>]*>(.-)</description>") or body:match("<summary[^>]*>(.-)</summary>") or "Este feed fornece apenas o título."
  local link=body:match("<link[^>]*>(.-)</link>") or body:match([=[<link[^>]-href=["'](.-)["']]=]) or ""
  local html=markup(text)
  if not html:lower():find("<img",1,true) then
   local image=body:match([=[<media:content[^>]-url=["'](.-)["']]=]) or body:match([=[<media:thumbnail[^>]-url=["'](.-)["']]=])
   if image then html='<img src="'..image:gsub('"','&quot;')..'">'..html end
  end
  result[#result+1]={title=store.clip(title,160),text=store.clip(clean(text),6000),html=store.clip(html,24000),link=clean(link)}
  if #result>=30 then break end
 end
 return result
end
local function cachePath() return "/data/feeds/cache-"..selected..".xml" end
function event(action,value)
 local network=sys.net()
 if imagesPending and not network.busy then imagesPending=false;message=network.message end
 if action=="toggle_html" then plain=not plain end
 if action=="load_images" and network.html_supported then
  local html,base="",""
  if section=="reader" and items[article] then html,base=items[article].html,items[article].link
  elseif section=="saved_reader" then for _,r in ipairs(store.read("/data/feeds/saved.tsv") or {}) do if r[1]==article then html=r[4] and sys.read(r[4]) or "";base=r[5] or "" end end end
  imagesPending=sys.net("images",html,base);message=imagesPending and "Carregando imagens…" or "Sem imagens externas, rede ocupada ou desconectada."
 end
 local sources,why=store.read("/data/feeds/sources.tsv")
 if not sources then return chrome.view({title="Feeds",body=tostring(why).." • arquivo preservado",buttons={}}) end
 if #sources==0 and not sys.read("/data/feeds/sources.tsv") then sources={{"gizmodo","Gizmodo","https://www.gizmodo.com.br/feed"}} end
 if pending and not sys.net().busy then
  local xml=sys.read("/data/feeds/latest.xml")
  if xml and #parse(xml)>0 and sys.net().message=="Feed atualizado" then
   sys.write("/data/feeds/cache-"..pending..".xml",xml)
  end
  local result=sys.net().message or ""
  message=result
  if result~="Feed atualizado" and result~="Cancelado" then errorDetail=result;message="Não foi possível atualizar. Abra Detalhes." else errorDetail="" end
  pending=nil
 end
 if action=="error_details" then section="error_details"
 elseif action=="configure" or action=="new" then section="configure"
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
   if sys.write(path,item.title.."\n\n"..item.text.."\n\n"..item.link) and sys.write(path..".html",item.html or "") then saved[#saved+1]={id,item.title,path,path..".html",item.link};local ok,why=store.write("/data/feeds/saved.tsv",saved);message=store.saveStatus(ok,why) end
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
 if section=="error_details" then return chrome.view({title="Detalhes da conexão",layout="reader",field=true,readonly=true,text=errorDetail~="" and errorDetail or sys.net().message,body="Resposta completa do serviço de rede",buttons={}}) end
 if section=="configure" then return chrome.view({title="Adicionar feed",layout="form",field=true,text="\nhttps://",body=message,
  controls={{label="Nome",type="text"},{label="URL RSS / Atom",type="text"}},buttons={{id="save_source",label="Salvar feed"}}}) end
 if section=="saved" then local b={};for _,r in ipairs(store.read("/data/feeds/saved.tsv") or {}) do b[#b+1]={id="saved:"..r[1],label=r[2],icon="feeds"} end;return chrome.view({title="Artigos offline",layout="list",body="Artigos salvos no cartão",buttons=b}) end
 if section=="saved_reader" then
  local text,html,base="Artigo não encontrado.","",""
  for _,r in ipairs(store.read("/data/feeds/saved.tsv") or {}) do if r[1]==article then text=sys.read(r[3]) or text;html=r[4] and sys.read(r[4]) or "";base=r[5] or "" end end
  local rich=network.html_supported and html~="" and not plain
  return chrome.view({title="Leitura offline",layout=rich and "html" or "reader",field=not rich,readonly=true,text=text,html=html,baseUrl=base,body=message,refreshMs=imagesPending and 1000 or 0,footer=network.html_supported and {{id="toggle_html",label=plain and "Formatado" or "Texto",icon="ui_info"},{id="load_images",label="Carregar imagens",icon="gallery"}} or {}})
 end
 if section=="confirm_remove" then return chrome.view({title="Remover assinatura?",layout="list",body="Artigos salvos serão preservados.",buttons={{id="confirm_remove",label="Remover assinatura"},{id="back",label="Cancelar"}}}) end
 if selected~="" then
  local xml=sys.read(cachePath()) or "";items=parse(xml)
  if pending==selected then local latest=sys.read("/data/feeds/latest.xml") or "";if sys.net().message=="Feed atualizado" then items=parse(latest);sys.write(cachePath(),latest) end end
 end
 if section=="reader" then
  local item=items[article];local rich=item and network.html_supported and not plain
  return chrome.view({title=item and item.title or "Artigo",layout=rich and "html" or "reader",field=not rich,readonly=true,text=item and item.text.."\n\n"..item.link or "Artigo indisponível.",html=item and item.html or "",baseUrl=item and item.link or "",body=message,refreshMs=imagesPending and 1000 or 0,buttons={},footer=network.html_supported and {{id="save_article",label="Salvar offline",icon="ui_save"},{id="toggle_html",label=plain and "Formatado" or "Texto",icon="ui_info"},{id="load_images",label="Carregar imagens",icon="gallery"}} or {{id="save_article",label="Salvar offline",icon="ui_save"}}})
 end
 local buttons={}
 if errorDetail~="" then buttons[#buttons+1]={id="error_details",label="Detalhes da falha",icon="ui_info"} end
 if section=="articles" then
  page=math.min(page,math.max(0,math.floor((#items-1)/6)))
  for i=page*6+1,math.min(page*6+6,#items) do buttons[#buttons+1]={id="article:"..i,label=items[i].title,subtitle=store.clip(items[i].text,80),icon="feeds"} end
  return chrome.view({title="Artigos • "..#items,layout="list",refreshMs=1000,body=sys.net().busy and ("Baixando • "..tostring(sys.net().bytes).." bytes") or (message~="" and message or "Artigos disponíveis no cartão"),buttons=buttons,
   tools={{id=sys.net().busy and "cancel" or "update",label=sys.net().busy and "Cancelar" or "Baixar",icon=sys.net().busy and "ui_cancel" or "network"},{id="prev",label="Anterior",icon="ui_prev"},{id="next",label="Próxima",icon="ui_next"},{id="remove_source",label="Remover",icon="ui_delete"}}})
 end
 for _,source in ipairs(sources) do buttons[#buttons+1]={id="source:"..source[1],label=source[2],subtitle=source[3],icon="feeds"} end
 return chrome.view({title="Feeds e leitura",layout="list",body="Assinaturas e artigos disponíveis offline",buttons=buttons,tools={{id="new",label="Adicionar",icon="ui_new"},{id="saved",label="Salvos",icon="ui_save"}}})
end
