-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local text = sys.read("/data/hello/document.txt") or (sys.asset("message.txt") or "Olá, Tab5!")
local status = "Aplicativo externo de exemplo • dados no SD"
function event(action, value)
	if action ~= "focus" then
		text = value
	end
	if action == "save" or action == "suspend" or action == "checkpoint" then
		local ok, why = sys.write("/data/hello/document.txt", text)
		if not ok then
			navigation_blocked = true
			status = tostring(why)
		else
			status = "Salvo."
		end
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	return chrome.view({ title = "Olá SDK", body = status, field = true, text = text, buttons = {
		{ id = "save", label = "Salvar no SD" },
	} })
end
