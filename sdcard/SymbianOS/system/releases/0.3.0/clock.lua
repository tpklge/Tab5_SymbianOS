-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local path = "/data/organizer/alarms.tsv"
local page, text, selected = "list", "", ""
local status = ""
local offset = 0
local session = store.resume("clock")
if session then
	page, text, selected = session[1], session[2], session[3]
end
function event(action, value)
	local rows, why = store.read(path)
	if not rows then
		return chrome.view({ title = "Relógio", body = tostring(why), buttons = {} })
	end
	if action ~= "focus" then
		text = value
	end
	if action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("clock", page, text, selected)
		if not ok then
			error(why)
		end
	elseif action == "new" then
		page = "edit"
		selected = ""
		text = "Alarme\n" .. sys.date(sys.epoch() + 60) .. "\núnico"
		status = "Nome, data AAAA-MM-DD HH:MM, repetição: diário ou único."
	elseif action:sub(1, 5) == "edit:" then
		selected = action:sub(6)
		for _, r in ipairs(rows) do
			if r[1] == selected then
				text = table.concat({ r[2], sys.date(tonumber(r[3])), r[4] == "86400" and "diário" or "único" }, "\n")
				page = "edit"
			end
		end
	elseif action == "save" then
		local f = store.lines(text)
		local epoch = f[2] and sys.epoch(f[2])
		if not epoch or not f[1] or f[1] == "" or (utf8.len(f[1]) or 9999) > 256 then
			status = "Nome e data válidos são obrigatórios."
		else
			local id = selected ~= "" and selected or store.id(rows)
			local row = { id, f[1], tostring(epoch), f[3] == "diário" and "86400" or "0", "1" }
			local found = false
			for i, r in ipairs(rows) do
				if r[1] == id then
					rows[i] = row
					found = true
				end
			end
			if not found then
				rows[#rows + 1] = row
			end
			local ok, why = store.write(path, rows)
			status = store.saveStatus(ok, why)
			if ok then
				page = "list"
				text = ""
				selected = ""
			end
		end
	elseif action == "toggle" then
		for _, r in ipairs(rows) do
			if r[1] == selected then
				r[5] = r[5] == "1" and "0" or "1"
			end
		end
		local ok, why = store.write(path, rows)
		status = store.saveStatus(ok, why)
	elseif action == "delete" then
		page = "confirm"
	elseif action == "confirm_delete" then
		for i = #rows, 1, -1 do
			if rows[i][1] == selected then
				table.remove(rows, i)
			end
		end
		local ok, why = store.write(path, rows)
		status = store.saveStatus(ok, why)
		if ok then
			page = "list"
			selected = ""
			text = ""
		end
	elseif action == "adjust" then
		page = "adjust"
		text = sys.date(sys.epoch())
		status = "Ajuste: AAAA-MM-DD HH:MM. RTC é gravado se disponível."
	elseif action == "set_time" then
		local epoch = sys.epoch(text)
		if epoch then
			local ok, persistent = sys.set_clock(epoch)
			if ok and persistent then
				local saved, why = sys.write("/config/public/rtc_format", "utc")
				status = saved and "Relógio ajustado e salvo no RTC (UTC)."
					or ("Hora ajustada; falha no marcador RTC: " .. tostring(why))
			else
				status = "Hora ajustada nesta sessão; RTC indisponível."
			end
		else
			status = "Data inválida."
		end
	elseif action == "zone_cuiaba" or action == "zone_utc" or action == "zone_sp" then
		local tz = action == "zone_utc" and "UTC0" or action == "zone_sp" and "<-03>3" or "<-04>4"
		local ok, why = sys.write("/config/public/timezone", tz)
		if ok then
			sys.zone(tz)
		end
		status = store.saveStatus(ok, why)
	elseif action == "next" then
		offset = offset + 8
	elseif action == "prev" then
		offset = math.max(0, offset - 8)
	elseif action == "back" then
		page = "list"
		text = ""
		selected = ""
	elseif action == "home" then
		store.session("clock", page, text, selected)
		next_app = "shell"
	end
	local buttons = {}
	if page == "edit" then
		buttons = {
			{ id = "save", label = "Salvar alarme" },
			{ id = "toggle", label = "Ativar / desativar" },
			{ id = "delete", label = "Excluir alarme" },
		}
	elseif page == "confirm" then
		status = "Excluir este alarme?"
		buttons = { { id = "confirm_delete", label = "Confirmar exclusão" }, { id = "back", label = "Cancelar" } }
	elseif page == "adjust" then
		buttons = {
			{ id = "set_time", label = "Aplicar data/hora" },
			{ id = "zone_cuiaba", label = "Fuso: Cuiabá" },
			{ id = "zone_sp", label = "Fuso: Brasília" },
			{ id = "zone_utc", label = "Fuso: UTC" },
		}
	else
		offset = math.min(offset, math.max(0, math.floor((#rows - 1) / 8) * 8))
		for i = offset + 1, math.min(offset + 8, #rows) do
			local r = rows[i]
			buttons[#buttons + 1] =
				{ id = "edit:" .. r[1], label = (r[5] == "1" and "● " or "○ ") .. store.clip(r[2], 48) }
		end
		buttons[#buttons + 1] = { id = "new", label = "Novo alarme" }
		buttons[#buttons + 1] = { id = "adjust", label = "Ajustar relógio / fuso" }
		buttons[#buttons + 1] = { id = "prev", label = "Anterior" }
		buttons[#buttons + 1] = { id = "next", label = "Próxima" }
	end
	return chrome.view({
		title = "Relógio / alarmes",
		body = status ~= "" and status or "Alarmes ativos mesmo com outro app aberto. Soneca nas notificações.",
		field = page ~= "confirm",
		text = text,
		buttons = buttons,
	})
end
