-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local listPage = 0
local page, path, target, status = "list", "", "", "Pacotes locais .t5pkg em /update/packages"
function event(action, value)
	if action == "next_list" then
		listPage = listPage + 1
	elseif action == "prev_list" then
		listPage = math.max(0, listPage - 1)
	elseif action:sub(1, 8) == "inspect:" then
		path = action:sub(9)
		local info, why = sys.package("inspect", path)
		if info then
			page = "review"
			status = info.title
				.. " • "
				.. info.version
				.. " • "
				.. info.author
				.. "\nPermissões: "
				.. (info.permissions ~= "" and info.permissions or "somente dados próprios")
				.. "\n"
				.. info.bytes
				.. " bytes extraídos. Pacote local sem assinatura."
		else
			status = tostring(why)
		end
	elseif action == "install" and page == "review" then
		local ok, why = sys.package("install", path)
		status = ok and "Instalado. Disponível no Menu." or tostring(why)
		page = "list"
	elseif action:sub(1, 7) == "remove:" then
		target = action:sub(8)
		page = "remove"
		status = "Remover " .. target .. "? Preservar dados é a opção padrão."
	elseif action == "confirm_remove" or action == "confirm_erase" then
		local ok, why = sys.package(action == "confirm_erase" and "remove_data" or "remove", target)
		status = ok and "Aplicativo removido." or tostring(why)
		page = "list"
	elseif action:sub(1, 6) == "theme:" then
		local ok, why = sys.package("theme", action:sub(7))
		status = ok and "Tema aplicado." or tostring(why)
	elseif action == "default_theme" then
		local ok, why = sys.package("theme_default", "")
		status = ok and "Tema original restaurado." or tostring(why)
	elseif action == "back" then
		if page == "list" then
			next_app = "shell"
		else
			page = "list"
		end
	elseif action == "home" then
		next_app = "shell"
	end
	local buttons = {}
	if page == "review" then
		buttons = { { id = "install", label = "Aceitar permissões e instalar" }, { id = "back", label = "Cancelar" } }
	elseif page == "remove" then
		buttons = {
			{ id = "confirm_remove", label = "Remover e preservar dados" },
			{ id = "confirm_erase", label = "Remover também os dados do app" },
			{ id = "back", label = "Cancelar" },
		}
	else
		local files = sys.fs("list", "/update/packages", "", listPage * 6) or {}
		for i = 1, math.min(6, #files) do
			local f = files[i]
			if not f.directory and f.path:match("%.t5pkg$") then
				buttons[#buttons + 1] =
					{ id = "inspect:" .. f.path, label = f.path:match("[^/]+$"), icon = "installer" }
			end
		end
		local apps = sys.catalog() or {}
		for i = listPage * 6 + 1, math.min(listPage * 6 + 6, #apps) do
			local a = apps[i]
			if a.kind == "theme" then
				buttons[#buttons + 1] = { id = "theme:" .. a.id, label = "Aplicar tema: " .. a.title }
			end
			buttons[#buttons + 1] = { id = "remove:" .. a.id, label = "Remover " .. a.title .. " " .. a.version }
		end
		buttons[#buttons + 1] = { id = "prev_list", label = "Anterior" }
		buttons[#buttons + 1] = { id = "next_list", label = "Próxima" }
		buttons[#buttons + 1] = { id = "default_theme", label = "Restaurar tema original" }
	end
	return chrome.view({
		title = "Instalador",
		body = status,
		buttons = buttons,
		field = page == "review",
		readonly = true,
		text = status,
	})
end
