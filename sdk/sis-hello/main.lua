-- SPDX-License-Identifier: MIT
function event(action)
 if action=='home' or action=='back' then next_app='shell' end
 return {title='Olá SIS',body='Aplicativo original instalado de um pacote SIS sem assinatura.',buttons={{id='back',label='Voltar'}}}
end
