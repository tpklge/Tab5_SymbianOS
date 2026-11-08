-- SPDX-License-Identifier: MIT
local chrome=sys.module('chrome')
local initial, reason = sys.read('/data/notes/document.txt')
local text = initial or ''
local blocked = not initial and reason~='not found'
local status = blocked and ('Documento não aberto: '..tostring(reason)) or 'Documento UTF-8 • Ctrl+S para salvar'
function event(action, value)
  if action ~= 'focus' then text = value end
  if blocked then
    if action=='home' or action=='back' or action=='suspend' then next_app='shell' end
    return chrome.view{title='Notas',body=status,buttons={{id='home',label='Início'}}}
  end
  if action == 'save' or action == 'options' or action == 'home' or action == 'back' or action == 'suspend' then
    local ok, reason = sys.write('/data/notes/document.txt', text)
    if not ok then navigation_blocked=true;status='Não foi possível salvar: '..tostring(reason)
    else status='Documento salvo no cartão SD.' end
  end
  if (action == 'back' or action == 'home') and status=='Documento salvo no cartão SD.' then next_app='shell' end
  return chrome.view{title='Notas',body=status,field=true,text=text,
    buttons={{id='save',label='Salvar'},{id='home',label='Tela inicial'}}}
end
