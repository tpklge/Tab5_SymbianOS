-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local configure = false
local message, items, url = "", {}, "https://"
local function clean(s)
	return (s:gsub("<!%[CDATA%[(.-)%]%]>", "%1")
		:gsub("<[^>]*>", "")
		:gsub("&amp;", "&")
		:gsub("&lt;", "<")
		:gsub("&gt;", ">")
		:gsub("&quot;", '"')
		:gsub("%s+", " ")):sub(1, 200)
end
function event(action, value)
	if action == "configure" then
		configure = true
	elseif action == "update" then
		configure = false
		url = value or url
		message = sys.net("feed", url, "/data/feeds/latest.xml")
				and "Atualizando… toque Ler para carregar os títulos."
			or "URL inválida, operação ocupada ou perfil Offline."
	elseif action == "cancel" then
		sys.net("cancel")
	elseif action == "home" or action == "back" then
		next_app = "shell"
	end
	local text = sys.read("/data/feeds/latest.xml") or ""
	items = {}
	for title in text:gmatch("<title[^>]*>(.-)</title>") do
		items[#items + 1] = clean(title)
		if #items >= 20 then
			break
		end
	end
	if configure then
		return chrome.view({
			title = "Endereço do feed",
			layout = "editor",
			field = true,
			text = url,
			body = "URL HTTP(S) final • RSS/Atom de até 64 KiB",
			buttons = { { id = "update", label = "Baixar feed" } },
		})
	end
	local status = sys.net()
	return chrome.view({
		title = "Feeds RSS / Atom",
		layout = "editor",
		field = true,
		readonly = true,
		text = table.concat(items, "\n\n"),
		body = status.message .. " • " .. message,
		buttons = {
			{ id = "configure", label = "Endereço / atualizar" },
			{ id = "read", label = "Ler títulos salvos" },
			{ id = "cancel", label = "Cancelar" },
		},
	})
end
