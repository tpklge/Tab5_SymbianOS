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
	return chrome.view({
		title = "Perfis",
		body = "Ativo: "
			.. profile
			.. "\n"
			.. (
				status ~= "" and status
				or "Normal: avisos sonoros. Silencioso: avisos visuais. Alarmes continuam audíveis. Offline: sem rede; avisos visuais."
			),
		buttons = {
			{ id = "normal", label = "Normal" },
			{ id = "silent", label = "Silencioso" },
			{
				id = "offline",
				label = "Offline",
			},
		},
	})
end
