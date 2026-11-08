-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local dir = "/home/Documents"
local text = dir
local status = ""
local page = 0
local confirm = nil
local preview = false
local operation = nil
local session = store.resume("files")
if session then
	dir = session[1]
	text = session[2]
end
local function arguments()
	local f = store.lines(text)
	return f[1] or "", f[2] or ""
end
function event(action, value)
	if action ~= "focus" and not preview then
		text = value
	end
	if action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("files", dir, text)
		if not ok then
			error(why)
		end
	elseif action == "up" then
		dir = dir:match("^(.*)/[^/]+$") or "/"
		if dir == "" then
			dir = "/"
		end
		text = dir
		page = 0
		preview = false
	elseif action == "open" then
		dir = arguments()
		page = 0
		preview = false
	elseif action:sub(1, 4) == "dir:" then
		dir = action:sub(5)
		text = dir
		page = 0
		preview = false
	elseif action:sub(1, 5) == "file:" then
		text = action:sub(6)
		preview = false
	elseif action == "info" then
		local a = arguments()
		local info, why = sys.fs("stat", a)
		status = info and (info.path .. " • " .. (info.directory and "pasta" or (info.size .. " bytes")))
			or tostring(why)
	elseif action == "space" then
		local u = sys.usage()
		if u then
			preview = true
			local lines = {
				string.format("Usado: %.2f MB / 4000 MB", u.used / 1e6),
				string.format("Saldo lógico comum: %.2f MB", math.max(0, u.limit - u.reserve - u.used) / 1e6),
				string.format("Reserva: %.2f MiB", u.reserve / 1048576),
				string.format("Livre físico: %.2f MB", u.physical_free / 1e6),
			}
			for _, key in ipairs({ "system", "apps", "data", "home", "config", "backups", "cache", "logs", "tmp" }) do
				lines[#lines + 1] = string.format("%s: %.2f MB", key, u[key] / 1e6)
			end
			previewText = table.concat(lines, "\n")
		end
	elseif action == "read" then
		local a = arguments()
		local content, why = sys.read(a)
		status = content and ("Leitura: " .. a) or tostring(why)
		if content then
			preview = true
			previewText = content
		end
	elseif action == "apply_operation" and operation then
		local a, b = arguments()
		local ok, why = sys.fs(operation, a, b)
		status = ok and "Operação concluída." or tostring(why)
		if ok then
			operation = nil
			text = dir
		end
	elseif action == "new_folder" then
		operation = "mkdir"
		text = dir .. "/Nova pasta"
	elseif action == "copy" or action == "move" then
		local a, b = arguments()
		if b == "" then
			operation = action
			text = a .. "\n" .. dir .. "/" .. (a:match("[^/]+$") or "arquivo") .. "-copia"
		else
			local ok, why = sys.fs(action, a, b)
			status = ok and "Operação concluída." or tostring(why)
		end
	elseif action == "mkdir" then
		local a = arguments()
		local ok, why = sys.fs("mkdir", a)
		status = ok and "Pasta criada." or tostring(why)
	elseif action == "delete" then
		confirm = arguments()
		status = "Excluir " .. confirm .. "? Diretórios precisam estar vazios."
	elseif action == "confirm_delete" and confirm then
		local ok, why = sys.fs("remove", confirm)
		status = ok and "Excluído." or tostring(why)
		confirm = nil
	elseif action == "cancel" then
		confirm = nil
		preview = false
	elseif action == "next" then
		page = page + 1
	elseif action == "prev" then
		page = math.max(0, page - 1)
	elseif action == "back" then
		if operation or preview or confirm then
			operation = nil
			preview = false
			confirm = nil
			text = dir
		elseif dir ~= "/" then
			dir = dir:match("^(.*)/[^/]+$") or "/"
			if dir == "" then
				dir = "/"
			end
			text = dir
			page = 0
		else
			next_app = "shell"
		end
	elseif action == "home" then
		store.session("files", dir, text)
		next_app = "shell"
	end
	local entries, total = sys.fs("list", dir, "", page * 8)
	local usage = sys.usage()
	local buttons = {}
	if not entries then
		status = tostring(total)
	else
		for i = 1, math.min(8, #entries) do
			local e = entries[i]
			if e.path:match("[^/]+$") ~= ".gitkeep" then
				buttons[#buttons + 1] = {
					id = (e.directory and "dir:" or "file:") .. e.path,
					label = e.path:match("[^/]+$") or e.path,
					subtitle = e.directory and "Pasta" or tostring(e.size) .. " bytes",
					icon = e.directory and "files" or "notes",
				}
			end
		end
	end
	if operation then
		buttons = { { id = "apply_operation", label = "Confirmar" }, { id = "back", label = "Cancelar" } }
	elseif confirm then
		buttons = { { id = "confirm_delete", label = "Confirmar exclusão" }, { id = "cancel", label = "Cancelar" } }
	elseif preview then
		buttons = { { id = "back", label = "Fechar leitura" } }
	else
		for _, b in ipairs({
			{ "up", "Pasta acima" },
			{ "open", "Ir" },
			{ "read", "Abrir" },
			{ "info", "Info" },
			{ "space", "Espaço" },
			{ "copy", "Copiar" },
			{ "move", "Mover" },
			{ "new_folder", "Nova pasta" },
			{ "delete", "Excluir" },
			{ "next", "Próxima" },
			{ "prev", "Anterior" },
		}) do
			buttons[#buttons + 1] = { id = b[1], label = b[2] }
		end
	end
	local occupancy = usage
			and string.format(
				"Uso: %.2f / 4000 MB • reserva %.1f MB • livre físico %.1f MB",
				usage.used / 1e6,
				usage.reserve / 1e6,
				usage.physical_free / 1e6
			)
		or "Contabilização indisponível"
	return chrome.view({
		title = "Arquivos • " .. dir,
		layout = operation and "form" or preview and "editor" or confirm and "list" or "files",
		fields = operation == "mkdir" and "Nova pasta" or "Origem\nDestino",
		body = occupancy
			.. "\n"
			.. (status ~= "" and status or "Selecione um arquivo ou pasta. Caminho acima da lista."),
		field = true,
		readonly = preview,
		text = preview and previewText or text,
		buttons = buttons,
	})
end
