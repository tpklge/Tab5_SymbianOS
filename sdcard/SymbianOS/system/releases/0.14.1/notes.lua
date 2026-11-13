-- SPDX-License-Identifier: MIT
local chrome, store = sys.module("chrome"), sys.module("store")
local notePage = 0
local path, mode = "/data/notes/document.txt", "editor"
local initial, reason = sys.read(path)
local text = initial or ""
local blocked = not initial and reason ~= "not found"
if initial and (initial:find("\0", 1, true) or not utf8.len(initial) or (utf8.len(initial) or 0) > 16000) then
	blocked = true
	reason = "Documento inválido ou acima de 16.000 caracteres; arquivo preservado."
end
local status = blocked and tostring(reason) or "Ctrl+S salva • notas independentes no cartão"
local session = store.resume("notes")
if session then
	text = session[2]
	if session[3] and session[3]:match("^/data/notes/") then
		path = session[3]
		local content, why = sys.read(path)
		blocked = (not content and why ~= "not found")
			or (content and (content:find("\0", 1, true) or not utf8.len(content) or (utf8.len(content) or 0) > 16000))
			or false
	end
end
local buffer = text
local function entries()
	local rows = store.read("/data/notes/index.tsv") or {}
	if #rows == 0 then
		rows = { { "legacy", "Minha nota", "/data/notes/document.txt" } }
	end
	return rows
end
local function save()
	if blocked then
		status = "Documento não aberto; arquivo preservado."
		navigation_blocked = true
		return false
	end
	local ok, why = sys.write(path, text)
	status = store.saveStatus(ok, why)
	if not ok then
		navigation_blocked = true
		return false
	end
	buffer = text
	store.session("notes", "editor", text, path)
	return true
end
function event(action, value)
	if action ~= "focus" then
		text = value
	end
	if action == "page_next" then
		notePage = notePage + 1
	elseif action == "page_prev" then
		notePage = math.max(0, notePage - 1)
	elseif action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("notes", "editor", mode == "editor" and text or buffer, path)
		if not ok then
			navigation_blocked = true
			status = tostring(why)
		end
	elseif action == "new" then
		if mode ~= "editor" or blocked or save() then
			buffer = mode == "editor" and text or buffer
			mode = "new"
			text = "Nova nota"
		end
	elseif action == "list" then
		if mode ~= "editor" or blocked or save() then
			mode = "list"
			text = ""
		end
	elseif action:sub(1, 5) == "open:" then
		local id = action:sub(6)
		for _, row in ipairs(entries()) do
			if row[1] == id then
				local content, why = sys.read(row[3])
				if content and not content:find("\0", 1, true) and utf8.len(content) and utf8.len(content) <= 16000 then
					path = row[3]
					text = content
					buffer = content
					blocked = false
					mode = "editor"
				else
					status = tostring(why or "Nota inválida; arquivo preservado.")
				end
			end
		end
	elseif action == "rename" then
		if save() then
			buffer = text
			mode = "rename"
			for _, row in ipairs(entries()) do
				if row[3] == path then
					text = row[2]
				end
			end
		end
	elseif action == "save" or action == "options" then
		if mode == "new" or mode == "rename" then
			local name = text
			local rows = entries()
			if name == "" or (utf8.len(name) or 999) > 80 then
				status = "Título deve ter entre 1 e 80 caracteres."
			elseif mode == "new" then
				local id = sys.new_id()
				if id then
					local newpath = "/data/notes/note-" .. id .. ".txt"
					local ok, why = sys.write(newpath, "")
					if ok then
						rows[#rows + 1] = { id, name, newpath }
						ok, why = store.write("/data/notes/index.tsv", rows)
					end
					if ok then
						path = newpath
						mode = "editor"
						text = ""
						buffer = ""
						blocked = false
						status = "Nova nota criada."
					else
						status = tostring(why)
					end
				end
			else
				for _, row in ipairs(rows) do
					if row[3] == path then
						row[2] = name
					end
				end
				local ok, why = store.write("/data/notes/index.tsv", rows)
				if ok then
					mode = "editor"
					text = buffer
					status = "Título atualizado."
				else
					status = tostring(why)
				end
			end
		elseif mode == "editor" then
			save()
		end
	elseif action == "back" or action == "home" then
		if action == "back" and mode ~= "editor" then
			mode = "editor"
			text = buffer
		elseif mode ~= "editor" or blocked or save() then
			next_app = "shell"
		end
	end
	local buttons, tools = {}, {}
	if mode == "list" then
		local rows = entries()
		notePage = math.min(notePage, math.max(0, math.floor((#rows - 1) / 5)))
		for i = notePage * 5 + 1, math.min(notePage * 5 + 5, #rows) do
			local row = rows[i]
			buttons[#buttons + 1] = { id = "open:" .. row[1], label = row[2], icon = "notes" }
		end
		tools = {
			{ id = "new", label = "Nova", icon = "ui_new" },
			{ id = "page_prev", label = "Anterior", icon = "ui_prev" },
			{ id = "page_next", label = "Próxima", icon = "ui_next" },
		}
	elseif mode == "editor" then
		tools = {
			{ id = "save", label = "Salvar", icon = "ui_save" },
			{ id = "new", label = "Nova", icon = "ui_new" },
			{ id = "list", label = "Minhas notas", icon = "ui_list" },
			{ id = "rename", label = "Renomear", icon = "ui_edit" },
		}
	else
		tools = {
			{ id = "save", label = "Confirmar", icon = "ui_save" },
			{ id = "back", label = "Cancelar", icon = "ui_back" },
		}
	end
	return chrome.view({
		title = mode == "list" and "Minhas notas"
			or mode == "new" and "Nova nota"
			or mode == "rename" and "Renomear nota"
			or "Notas",
		layout = (mode == "new" or mode == "rename") and "form" or mode == "list" and "notebook" or "editor",
		fields = "Título",
		body = status,
		field = mode ~= "list" and not blocked,
		text = text,
		buttons = buttons,
		tools = tools,
	})
end
