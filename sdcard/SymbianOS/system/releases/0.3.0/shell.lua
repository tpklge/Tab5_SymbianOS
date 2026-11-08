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
}
local names = {}
for _, a in ipairs(registry) do
	names[a[1]] = a[2]
end
local page = "home"
local status = ""
local noticePage = 0
function event(action, value)
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
			body = status ~= "" and status or "Seu espaço pessoal • aplicativos e documentos no cartão.",
			buttons = {
				{ id = "menu", label = "Menu" },
				{ id = "open:notes", label = "Notas" },
				{ id = "open:calculator", label = "Calculadora" },
				{ id = "notices", label = "Notificações" },
			},
		})
	elseif page == "menu" then
		local b = {}
		for _, a in ipairs(registry) do
			b[#b + 1] = { id = "open:" .. a[1], label = a[2] }
		end
		return chrome.view({
			title = "Aplicativos",
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
				label = (names[id] or id) .. string.format(" • %s • %.0f KiB", state, info.memory / 1024),
			}
			b[#b + 1] = { id = "close:" .. id, label = "Fechar " .. (names[id] or id) }
		end
		return chrome.view({
			title = "Aplicativos abertos",
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
