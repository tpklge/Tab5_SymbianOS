-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local text = "/backups/installation.t5bak"
local status = ""
local confirm = false
local session = store.resume("backup")
if session then
	text = session[2]
end
function event(action, value)
	if action ~= "focus" then
		text = value
	end
	if action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("backup", "editor", text)
		if not ok then
			error(why)
		end
	elseif action == "create" then
		local ok, why = sys.fs("backup", text)
		status = ok and "Backup verificado criado. Sistema, apps, configurações e documentos incluídos."
			or tostring(why)
	elseif action == "restore" then
		confirm = true
		status =
			"Restaurar este backup? Arquivos de mesmo nome serão substituídos; outros documentos serão preservados."
	elseif action == "confirm_restore" then
		local ok, why = sys.fs("restore", text)
		status = ok
				and "Restauração concluída. Reinicie para recarregar serviços, preferências e sistema restaurados."
			or tostring(why)
		confirm = false
	elseif action == "cancel" then
		confirm = false
	elseif action == "home" or action == "back" then
		store.session("backup", "editor", text)
		next_app = "shell"
	end
	local usage = sys.usage()
	local info = usage
			and string.format(
				"Instalação %.2f MB • backups %.2f MB • limite 4000 MB",
				usage.used / 1e6,
				usage.backups / 1e6
			)
		or "Quota indisponível"
	return chrome.view({
		title = "Backup / restauração",
		body = info
			.. "\n"
			.. (
				status ~= "" and status
				or "Informe um novo caminho em /backups. Backups, cache, logs, tmp e updates não entram no snapshot. Esc cancela antes do commit."
			),
		field = true,
		text = text,
		buttons = confirm and {
			{ id = "confirm_restore", label = "Confirmar restauração" },
			{ id = "cancel", label = "Cancelar" },
		} or { { id = "create", label = "Criar backup" }, { id = "restore", label = "Restaurar backup" } },
	})
end
