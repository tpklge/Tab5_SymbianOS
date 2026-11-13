-- SPDX-License-Identifier: MIT
local icons={
 create_backup="ui_backup",apply="ui_ok",save="ui_save",save_options="ui_ok",save_source="ui_save",save_list="ui_save",menu_save="ui_ok",set_time="ui_ok",install="ui_install",connect="network",connect_once="network",fetch="ui_download",confirm="ui_ok",confirm_delete="ui_delete",confirm_restore="ui_restore",confirm_remove="ui_delete",confirm_erase="ui_delete",apply_operation="ui_ok",cancel="ui_cancel",back="ui_back",
 favorites="profiles",star="profiles",new="ui_new",new_list="ui_new",new_folder="ui_folder",search="ui_search",search_form="ui_search",import="ui_import",export="ui_export",prev="ui_back",next="ui_forward",up="ui_up",open="ui_forward",read="ui_open",info="ui_info",space="ui_memory",copy="ui_copy",move="ui_move",delete="ui_delete",create="ui_backup",restore="ui_restore",adjust="settings",test_sound="ui_normal",today="clock",month="agenda",week="agenda",day="agenda",done="ui_ok",toggle="ui_ok",default_theme="profiles",prev_list="ui_back",next_list="ui_forward",remove_pin="ui_unlock",lock="lock",appearance="profiles",security="lock",refresh="ui_refresh",
 normal="ui_normal",silent="ui_silent",offline="ui_offline",volume_up="ui_volume_up",volume_down="ui_volume_down",scan="ui_search",saved="ui_save",wifi="network",share="ui_share",share_off="ui_stop",download="ui_download",disconnect="ui_offline",forget="ui_delete",known_connect="network",auto_on="ui_ok",auto_off="ui_cancel",known="network",inspect="installer",remove="ui_delete",theme="profiles",next_list="ui_forward",prev_list="ui_back",day_prev="ui_back",day_next="ui_forward",saved_next="ui_forward",saved_prev="ui_back",scan_next="ui_forward",scan_prev="ui_back",
 screen="ui_brightness",charge="ui_charge",sound="ui_normal",sleep="ui_sleep",screen_off="lock",fast="ui_charge",off="ui_stop",zoom_in="ui_zoom_in",zoom_out="ui_zoom_out",
}
local commits={create_backup=true,apply=true,save=true,save_options=true,save_source=true,save_list=true,menu_save=true,set_time=true,install=true,connect=true,connect_once=true,fetch=true,confirm=true,confirm_delete=true,confirm_restore=true,confirm_remove=true,confirm_erase=true,apply_operation=true,cancel=true,back=true}
local toolApps={agenda=true,clock=true,backup=true,installer=true}
local M={}
local i18n=sys.module("locale")
local themeCatalog=sys.module("themes")
function M.view(v)
 v.left="";v.right=v.right or "Voltar"
 v.home,v.menu,v.switcher,v.keyboard="Início","Menu","Abertos","Teclado"
 local preferences=sys.theme();local themes=themeCatalog;local theme=themes[preferences.builtin or "aurora"] or themes.aurora
 for key,value in pairs(theme) do if key~="name" then v[key]=value end end
 for key,value in pairs(preferences) do if key~="builtin" and key~="app_id" then v[key]=value end end
 if not v.layout then v.layout=v.list and "list" or v.field and "editor" or "list" end
 if v.layout~="home" then v.wallpaper=nil end
 v.buttons=v.buttons or {};v.tools=v.tools or {};v.footer=v.footer or {}
 local id=preferences.app_id;local buttons={}
 local function decorate(b)
  local prefix=b.id:match("^([^:]+)")
  b.icon=b.icon or icons[b.id] or icons[prefix]
  return b
 end
 for _,b in ipairs(v.buttons) do
  decorate(b)
  if commits[b.id] then
   if b.id=="back" then v.right=b.label~="Voltar" and b.label or v.right
   else v.footer[#v.footer+1]=b end
  elseif toolApps[id] and v.layout~="status" and v.layout~="options" and v.layout~="choices" and icons[b.id] and b.id~="toggle" then v.tools[#v.tools+1]=b
  else buttons[#buttons+1]=b end
 end
 v.buttons=buttons
 local tools={};for _,b in ipairs(v.tools) do
  decorate(b)
  if commits[b.id] then if b.id~="back" then v.footer[#v.footer+1]=b end
  elseif b.id~="volume_up" and b.id~="volume_down" then tools[#tools+1]=b end
 end;v.tools=tools
 if v.layout~="form" and id=="clock" then v.option="adjust" elseif v.layout~="form" and id=="music" then v.option="options" elseif v.layout~="form" and id=="feeds" then v.option="configure" end
 if v.option then
  local filtered={};for _,b in ipairs(v.tools) do if b.id~=v.option then filtered[#filtered+1]=b end end;v.tools=filtered
  filtered={};for _,b in ipairs(v.buttons) do if b.id~=v.option then filtered[#filtered+1]=b end end;v.buttons=filtered
 end
 v.profile=preferences.profile
 return i18n.apply(v,preferences.language,preferences.app_id)
end
return M
