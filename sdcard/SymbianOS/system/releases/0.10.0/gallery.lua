-- SPDX-License-Identifier: MIT
local chrome=sys.module("chrome")
local page,selected,kind,zoom=0,"","",1
local status,confirm="Fotos e vídeos • toque para abrir",false
local function neighbors()
 local images={};for offset=0,504,12 do
  local files,total=sys.fs("list","/home/Pictures","",offset,"visual");if not files then break end
  for _,f in ipairs(files) do if not f.directory then images[#images+1]=f.path end end
  if offset+12>=total then break end
 end;return images
end
local function open(path)
 kind=path:lower():match("%.avi$") and "video" or "photo"
 local ok,why=sys.media(kind=="video" and "video_play" or "image",path,2)
 if ok then selected=path;zoom=1;status=path:match("[^/]+$") else status=tostring(why) end
end
function event(action)
 if action=="next" then page=page+1 elseif action=="prev" then page=math.max(0,page-1)
 elseif action:sub(1,5)=="view:" then open(action:sub(6))
 elseif action=="photo_next" or action=="photo_prev" then
  local all=neighbors();for i,p in ipairs(all) do if p==selected then if kind=="video" then sys.media("video_stop") end;open(all[(i-1+(action=="photo_next" and 1 or -1))%#all+1]);break end end
 elseif action=="zoom_in" then zoom=math.min(4,zoom*2)
 elseif action=="zoom_out" then zoom=math.max(1,math.floor(zoom/2))
 elseif action=="fit" then zoom=1
 elseif action=="delete" then confirm=true
 elseif action=="cancel" then confirm=false
 elseif action=="confirm_delete" then
  if kind=="video" then sys.media("video_stop") end
  local ok,why=sys.fs("remove",selected);status=ok and "Arquivo excluído." or tostring(why);if ok then selected="" end;confirm=false
 elseif action=="back" then
  if confirm then confirm=false elseif selected~="" then if kind=="video" then sys.media("video_stop") end;selected="" else next_app="shell" end
 elseif action=="home" or action=="suspend" then if kind=="video" then sys.media("video_stop") end;if action=="home" then next_app="shell" end end
 if confirm then return chrome.view({title="Excluir mídia?",layout="list",body=selected,buttons={{id="confirm_delete",label="Excluir arquivo"},{id="cancel",label="Cancelar"}}}) end
 local buttons,tools={},{}
 if selected=="" then
  local files,total=sys.fs("list","/home/Pictures","",page*8,"visual");files=files or {}
  if total and page*8>=total and page>0 then page=math.max(0,math.floor((total-1)/8));files=sys.fs("list","/home/Pictures","",page*8,"visual") or {} end
  for i=1,math.min(8,#files) do local f=files[i];local lower=f.path:lower();if not f.directory and (lower:match("%.jpe?g$") or lower:match("%.png$") or lower:match("%.avi$")) then buttons[#buttons+1]={id="view:"..f.path,label=f.path:match("[^/]+$"),icon=lower:match("%.avi$") and "ui_video" or "gallery",thumbnail=f.path} end end
  tools={{id="prev",label="Anterior",icon="ui_prev"},{id="next",label="Próxima",icon="ui_next"}}
 else
  tools={{id="photo_prev",label="Anterior",icon="ui_prev"},{id="photo_next",label="Próxima",icon="ui_next"}}
  if kind=="photo" then tools[#tools+1]={id="zoom_out",label="Zoom -",icon="ui_prev"};tools[#tools+1]={id="fit",label="Ajustar",icon="gallery"};tools[#tools+1]={id="zoom_in",label="Zoom +",icon="ui_next"} end
  tools[#tools+1]={id="delete",label="Excluir",icon="ui_delete"}
  local f=sys.fs("stat",selected);status=selected:match("[^/]+$")..(f and " • "..math.floor(f.size/1024).." KiB" or "")..(kind=="photo" and " • "..zoom.."× • arraste para deslocar" or "")
 end
 return chrome.view({title="Galeria",layout=selected=="" and "gallery" or kind=="video" and "video_view" or "photo",body=status,result=kind=="photo" and zoom>1 and ("zoom"..zoom) or "",buttons=buttons,tools=tools})
end
