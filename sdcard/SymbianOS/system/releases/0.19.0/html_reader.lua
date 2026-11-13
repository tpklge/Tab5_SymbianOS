-- SPDX-License-Identifier: MIT
-- Navigation, resource loading and page state remain on the SD.
local M={};local key='';local page=0;local revision=0;local pending=false;local authorized=false;local discover=false;local rounds=0
function M.event(action,busy)
 if not sys.html then return false end
 if pending and not busy then pending=false;revision=revision+1;discover=true
 elseif discover and not busy then discover=false;if authorized and rounds<4 and (sys.html().pending or 0)>0 then pending=sys.html('resources');rounds=rounds+1 else authorized=false end end
 if action=='html_prev' then page=math.max(0,page-1);return true
 elseif action=='html_next' then local status=sys.html();page=math.min(page+1,math.max(0,(status.pages or 1)-1));return true
 elseif action=='cancel_resources' then sys.html('cancel');pending=false;authorized=false;discover=false;return true
 elseif action=='load_images' then rounds=1;pending=sys.html('resources');authorized=pending;return true end
 return false
end
function M.close()
 if pending and sys.html then sys.html('cancel') end
 pending=false;authorized=false;discover=false
end
function M.view(v,id)
 if not sys.html then return v end
 if id~=key then key=id;page=0;revision=revision+1;pending=false;authorized=false;discover=false end
 v.layout='web_document';v.htmlPage=page;v.htmlRevision=revision;v.field=false;v.readonly=true;v.centerTools=true
 v.tools={{id='html_prev',label='Anterior',icon='ui_prev'},{id='html_next',label='Próxima',icon='ui_next'}}
 if pending or authorized then v.refreshMs=1000 end
 for _,button in ipairs(v.footer or {}) do if button.id=='load_images' and pending then button.id='cancel_resources';button.label='Cancelar';button.icon='ui_cancel' end end
 return v
end
return M
