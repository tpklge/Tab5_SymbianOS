-- SPDX-License-Identifier: MIT
local chrome=sys.module('chrome')
local page = 'home'
function event(action, value)
  if action == 'menu' or action == 'options' then page = 'menu'
  elseif action == 'switcher' then page = 'switcher'
  elseif action == 'back' or action == 'home' then page = 'home'
  elseif action == 'open:notes' then next_app = 'notes'
  elseif action == 'open:calculator' then next_app = 'calculator' end
  if page == 'home' then
    return chrome.view{title='Pessoal', body=sys.time() .. '\n\nBem-vindo ao Tab5\nSistema carregado do cartão SD.',
      buttons={{id='open:notes',label='Notas'},{id='open:calculator',label='Calculadora'},{id='menu',label='Menu'}}}
  end
  if page=='switcher' then
    local buttons={}
    local labels={notes='Notas',calculator='Calculadora'}
    for _,id in ipairs(sys.apps()) do
      buttons[#buttons+1]={id='open:'..id,label=labels[id] or id}
    end
    local body=#buttons==0 and 'Nenhum aplicativo aberto.' or 'Retome os aplicativos sem perder o estado.'
    buttons[#buttons+1]={id='home',label='Tela inicial'}
    return chrome.view{title='Aplicativos abertos',body=body,buttons=buttons}
  end
  return chrome.view{title='Menu',body='Selecione e toque novamente para abrir.',classic=true,
    buttons={{id='open:notes',label='Notas'},{id='open:calculator',label='Calculadora'},{id='home',label='Tela inicial'}}}
end
