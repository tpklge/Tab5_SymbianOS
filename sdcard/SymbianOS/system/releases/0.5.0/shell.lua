-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local registry = {
	{ "notes", "Notas" },
	{ "calculator", "Calculadora" },
	{ "contacts", "Contatos" },
	{ "agenda", "Agenda" },
	{ "clock", "Relógio / alarmes" },
	{ "converter", "Conversor" },
	{ "files", "Arquivos" },
	{ "backup", "Backup" },
	{ "profiles", "Perfis" },
	{ "music", "Música" },
	{ "recorder", "Gravador" },
	{ "gallery", "Galeria" },
	{ "camera", "Câmera" },
	{ "installer", "Instalador" },
}
local names = {}
for _, a in ipairs(registry) do
	names[a[1]] = a[2]
end
local menuPage = 0
local page = "home"
local status = ""
local noticePage = 0
function event(action, value)
	local registryNow = {}
	for _, a in ipairs(registry) do
		registryNow[#registryNow + 1] = a
	end
	for _, a in ipairs(sys.catalog() or {}) do
		if a.kind == "app" then
			registryNow[#registryNow + 1] = { a.id, a.title }
			names[a.id] = a.title
		end
	end
	if action == "menu_next" then
		menuPage = menuPage + 1
	elseif action == "menu_prev" then
		menuPage = math.max(0, menuPage - 1)
	end
	if action == "menu" or action == "options" then
		page = "menu"
	elseif action == "switcher" then
		page = "switcher"
	elseif action == "notices" then
		page = "notices"
		noticePage = math.max(0, #sys.notices() - 4)
	elseif action == "notice_next" then
		noticePage = noticePage + 4
	elseif action == "notice_prev" then
		noticePage = math.max(0, noticePage - 4)
	elseif action:sub(1, 4) == "ack:" then
		sys.send("services", "ack", action:sub(5))
		status = "Notificação dispensada."
	elseif action == "back" or action == "home" then
		page = "home"
	elseif action:sub(1, 5) == "open:" then
		next_app = action:sub(6)
	elseif action:sub(1, 6) == "close:" then
		local ok, why = sys.close(action:sub(7))
		status = ok and (why or "Aplicativo encerrado; estado salvo.") or tostring(why)
	elseif action:sub(1, 7) == "snooze:" then
		sys.send("services", "snooze", action:sub(8))
		status = "Soneca solicitada: 5 minutos."
	end
	if page == "home" then
		local profile = sys.read("/config/public/profile") or "normal"
		return chrome.view({
			title = "Pessoal • " .. profile,
			layout = "home",
			body = status ~= "" and status or "Seu espaço pessoal • aplicativos e documentos no cartão.",
			buttons = {
				{ id = "menu", label = "Menu", icon = "menu" },
				{ id = "open:notes", label = "Notas", icon = "notes" },
				{ id = "open:calculator", label = "Calculadora", icon = "calculator" },
				{ id = "notices", label = "Notificações", icon = "clock" },
			},
		})
	elseif page == "menu" then
		local b = {}
		for i = menuPage * 7 + 1, math.min(menuPage * 7 + 7, #registryNow) do
			local a = registryNow[i]
			b[#b + 1] = { id = "open:" .. a[1], label = a[2], icon = a[1] }
		end
		if menuPage > 0 then
			b[#b + 1] = { id = "menu_prev", label = "Anterior" }
		end
		if (menuPage + 1) * 7 < #registryNow then
			b[#b + 1] = { id = "menu_next", label = "Próxima página" }
		end
		return chrome.view({
			title = "Aplicativos",
			layout = "grid",
			body = status ~= "" and status or "Selecione e toque novamente para abrir.",
			classic = true,
			buttons = b,
		})
	elseif page == "switcher" then
		local b = {}
		for _, id in ipairs(sys.apps()) do
			local info = sys.app_info(id)
			local state = info.faulted and "falhou" or "suspenso"
			b[#b + 1] = {
				id = "open:" .. id,
				label = names[id] or id,
				subtitle = state .. " • " .. tostring(math.floor(info.memory / 1024)) .. " KiB",
				icon = id,
			}
			b[#b + 1] = { id = "close:" .. id, label = "Fechar " .. (names[id] or id) }
		end
		return chrome.view({
			title = "Aplicativos abertos",
			layout = "list",
			body = status ~= "" and status or "Três apps residentes; demais retomam o estado do SD.",
			buttons = b,
		})
	else
		local b = {}
		local lines = {}
		local list = sys.notices()
		noticePage = math.min(noticePage, math.max(0, #list - 4))
		for i = noticePage + 1, math.min(noticePage + 4, #list) do
			local n = list[i]
			lines[#lines + 1] = n.text
			b[#b + 1] = { id = "ack:" .. n.id, label = "Dispensar " .. store.clip(n.text, 24) }
			if n.alarm then
				b[#b + 1] = { id = "snooze:" .. n.id, label = "Soneca 5 min: " .. store.clip(n.text, 20) }
			end
		end
		b[#b + 1] = { id = "notice_prev", label = "Anterior" }
		b[#b + 1] = { id = "notice_next", label = "Próxima" }
		return chrome.view({
			title = "Notificações",
			body = status,
			field = true,
			readonly = true,
			text = #lines > 0 and table.concat(lines, "\n\n") or "Nenhuma notificação.",
			buttons = b,
		})
	end
end
