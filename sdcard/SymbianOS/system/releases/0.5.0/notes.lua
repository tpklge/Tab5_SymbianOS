-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local initial, reason = sys.read("/data/notes/document.txt")
local text = initial or ""
local blocked = not initial and reason ~= "not found"
if initial and (initial:find("\0", 1, true) or not utf8.len(initial) or (utf8.len(initial) or 0) > 16000) then
	blocked = true
	reason = "Documento inválido ou acima de 16.000 caracteres; arquivo preservado."
end
local status = blocked and tostring(reason) or "Documento UTF-8 • Ctrl+S salva"
local session = store.resume("notes")
if session then
	text = session[2]
end
function event(action, value)
	if action ~= "focus" then
		text = value
	end
	if action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("notes", "editor", text)
		if not ok then
			navigation_blocked = true
			status = tostring(why)
		end
	end
	if action == "save" or action == "options" or action == "back" or action == "home" then
		if not blocked then
			local ok, why = sys.write("/data/notes/document.txt", text)
			status = store.saveStatus(ok, why)
			if not ok then
				navigation_blocked = true
			else
				store.session("notes", "editor", text)
				if action == "back" or action == "home" then
					next_app = "shell"
				end
			end
		else
			status = "Documento não aberto; arquivo preservado."
		end
	end
	return chrome.view({
		title = "Notas",
		body = status,
		field = not blocked,
		text = text,
		buttons = { { id = "save", label = "Salvar documento" }, { id = "home", label = "Início" } },
	})
end
