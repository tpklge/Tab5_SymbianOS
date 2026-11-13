-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local status = ""
function event(action, value)
	if action == "normal" or action == "silent" or action == "offline" then
		local old = sys.read("/config/public/profile") or "normal"
		local applied = sys.offline(action == "offline")
		if applied then
			local ok, why = sys.write("/config/public/profile", action)
			if not ok then
				sys.offline(old == "offline")
			end
			status = store.saveStatus(ok, why)
		else
			status = "Falha ao aplicar controle de rádio; perfil anterior preservado."
		end
	elseif action == "home" or action == "back" then
		next_app = "shell"
	end
	local profile = sys.read("/config/public/profile") or "normal"
    local names={normal="Normal",silent="Silencioso",offline="Offline"}
    return chrome.view({title="Perfis",layout="options",body=status~="" and status or "Alarmes permanecem audíveis no perfil silencioso.",
      statusFields={{label="Perfil aplicado",value=names[profile] or profile}},
      buttons={{id="normal",label="Normal",icon="ui_normal",subtitle=profile=="normal" and "selected" or ""},
       {id="silent",label="Silencioso",icon="ui_silent",subtitle=profile=="silent" and "selected" or ""},
       {id="offline",label="Offline",icon="ui_offline",subtitle=profile=="offline" and "selected" or ""}}})
end
