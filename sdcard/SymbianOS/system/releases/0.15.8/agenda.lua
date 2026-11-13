-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local datetime=sys.module("datetime")
local path = "/data/organizer/events.tsv"
local page, text, selected = "day", "", ""
local status = ""
local query = ""
local offset = 0
local today = sys.date(sys.epoch(), "day")
local session = store.resume("agenda")
if session then
	page, text, selected = session[1], session[2], session[3]
end
local function monthView(day)
	local year, month = day:match("^(%d%d%d%d)%-(%d%d)")
	if not year then
		return "Ajuste o relógio para exibir o calendário."
	end
	local first = sys.epoch(year .. "-" .. month .. "-01 12:00")
	if not first then
		return "Data inválida"
	end
	local weekday = tonumber(sys.date(first, "weekday"))
	local result = { year .. "-" .. month, " D   S   T   Q   Q   S   S" }
	local line = string.rep("    ", weekday)
	for d = 1, 31 do
		local date = string.format("%s-%s-%02d 12:00", year, month, d)
		if not sys.epoch(date) then
			break
		end
		line = line .. string.format("%2d  ", d)
		weekday = weekday + 1
		if weekday == 7 then
			result[#result + 1] = line
			line = ""
			weekday = 0
		end
	end
	if line ~= "" then
		result[#result + 1] = line
	end
	return table.concat(result, "\n")
end
function event(action, value)
	local rows, why = store.read(path)
	if not rows then
		return chrome.view({ title = "Agenda", body = tostring(why), buttons = {} })
	end
	if action ~= "focus" and page ~= "month" then
		text = value
	end
	if action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("agenda", page, text, selected)
		if not ok then
			error(why)
		end
	elseif action == "new" then
		page = "edit"
		selected = ""
		text = "\n" .. datetime.fields(sys.date(sys.epoch())) .. "\n\ncompromisso"
		status = "Título, data brasileira, hora, descrição e tipo."
	elseif action:sub(1, 5) == "edit:" then
		selected = action:sub(6)
		for _, r in ipairs(rows) do
			if r[1] == selected then
				text = table.concat({ r[2], datetime.fields(sys.date(tonumber(r[3]))), r[4], r[5] }, "\n")
				page = "edit"
			end
		end
	elseif action == "save" then
		local f = store.lines(text)
		local legacy=f[2] and f[2]:match("^%d%d%d%d%-")
        local epoch=f[2] and datetime.epoch(f[2],f[3])
        local description=f[legacy and 3 or 4] or ""
        local kind=f[legacy and 4 or 5]
		if
			not epoch
			or not f[1]
			or f[1] == ""
			or (utf8.len(f[1]) or 9999) > 256
			or (utf8.len(description) or 9999) > 2048
		then
			status = "Título até 256 caracteres, descrição até 2048 e data válida são obrigatórios."
		else
			local id = selected ~= "" and selected or store.id(rows)
			local entry = { id, f[1], tostring(epoch), description, kind == "tarefa" and "tarefa" or "compromisso", "0" }
			local found = false
			for i, r in ipairs(rows) do
				if r[1] == id then
					entry[6] = r[6]
					rows[i] = entry
					found = true
				end
			end
			if not found then
				rows[#rows + 1] = entry
			end
			local ok, why = store.write(path, rows)
			status = store.saveStatus(ok, why)
			if ok then
				page = "day"
				text = ""
				selected = ""
			end
		end
	elseif action == "done" then
		for _, r in ipairs(rows) do
			if r[1] == selected then
				r[6] = r[6] == "1" and "0" or "1"
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
			page = "day"
			text = ""
			selected = ""
		end
    elseif action:sub(1,5)=="date:" then today=action:sub(6);page="day";text="";query="";offset=0
    elseif action=="week" then page="week"
    elseif action=="day_prev" or action=="day_next" then
        local epoch=sys.epoch(today.." 12:00");if epoch then today=sys.date(epoch+(action=="day_next" and 86400 or -86400),"day") end
        page="day";offset=0
	elseif action == "month" then
		page = "month"
	elseif action == "day" or action == "back" then
		page = "day"
		text = ""
		selected = ""
	elseif action == "today" then
		today = sys.date(sys.epoch(), "day")
		page = "day"
		query = ""
		offset = 0
	elseif action == "search" then
		query = text
		page = "day"
		offset = 0
	elseif action == "next" then
		if page == "month" then
			local y, m = today:match("^(%d+)%-(%d+)")
			if y then
				y = tonumber(y)
				m = tonumber(m) + 1
				if m == 13 then
					y = y + 1
					m = 1
				end
				today = string.format("%04d-%02d-01", y, m)
			end
		else
			offset = offset + 6
		end
	elseif action == "prev" then
		if page == "month" then
			local y, m = today:match("^(%d+)%-(%d+)")
			if y then
				y = tonumber(y)
				m = tonumber(m) - 1
				if m == 0 then
					y = y - 1
					m = 12
				end
				today = string.format("%04d-%02d-01", y, m)
			end
		else
			offset = math.max(0, offset - 6)
		end
	elseif action == "export" then
		local ok, why = store.write("/home/Documents/agenda.tsv", rows)
		status = ok and "Exportado: /home/Documents/agenda.tsv" or tostring(why)
	elseif action == "import" then
		local incoming, why = store.read("/home/Documents/agenda.tsv")
		if not incoming then
			status = tostring(why)
		else
			local valid = true
			for _, r in ipairs(incoming) do
				if #r ~= 6 or not tonumber(r[3]) or r[2] == "" then
					valid = false
				end
			end
			if valid then
				for _, r in ipairs(incoming) do
					r[1] = store.id(rows)
					rows[#rows + 1] = r
				end
				local ok, why = store.write(path, rows)
				status = store.saveStatus(ok, why)
			else
				status = "Arquivo inválido; agenda preservada."
			end
		end
	elseif action == "home" then
		store.session("agenda", page, text, selected)
		next_app = "shell"
	end
	local buttons = {}
	local shown = text
    local layout="list"
    local controls,result,tools=nil,"",{}
    if page=="month" then
        layout="calendar";shown=monthView(today)
        local y,m=today:match("^(%d%d%d%d)%-(%d%d)")
        local first=y and sys.epoch(y.."-"..m.."-01 12:00")
        if first then
            result=sys.date(first,"weekday")
            for d=1,31 do
                local date=string.format("%s-%s-%02d",y,m,d)
                if not sys.epoch(date.." 12:00") then break end
                local count=0;for _,r in ipairs(rows) do if sys.date(tonumber(r[3]),"day")==date then count=count+1 end end
                buttons[#buttons+1]={id="date:"..date,label=tostring(d)..(count>0 and " •" or "")}
            end
        end
        tools={{id="prev",label="Mês -",icon="ui_prev"},{id="next",label="Mês +",icon="ui_next"},{id="today",label="Hoje",icon="clock"},{id="day",label="Dia",icon="agenda"},{id="week",label="Semana",icon="agenda"},{id="new",label="Novo",icon="ui_new"}}
    elseif page=="week" then
        local epoch=sys.epoch(today.." 12:00")
        if epoch then
            local weekday=tonumber(sys.date(epoch,"weekday"));local start=epoch-((weekday+6)%7)*86400
            local names={"Segunda","Terça","Quarta","Quinta","Sexta","Sábado","Domingo"}
            for d=0,6 do local date=sys.date(start+d*86400,"day");local count=0;local first=""
                for _,r in ipairs(rows) do if sys.date(tonumber(r[3]),"day")==date then count=count+1;if first=="" then first=r[2] end end end
                buttons[#buttons+1]={id="date:"..date,label=names[d+1].." • "..date:sub(9,10).."/"..date:sub(6,7),subtitle=count.." compromissos"..(first~="" and " • "..store.clip(first,45) or ""),icon="agenda"}
            end
        end
        tools={{id="month",label="Mês",icon="agenda"},{id="new",label="Novo",icon="ui_new"}}
    elseif page=="edit" then
        layout="form"
        controls={{label="Título",type="text"},{label="Data",type="date"},{label="Hora",type="time"},{label="Descrição",type="text"},{label="Tipo",type="select",options="Compromisso\nTarefa",keys="compromisso\ntarefa"}}
        buttons = {
			{ id = "save", label = "Salvar" },
			{ id = "done", label = "Concluir / reabrir" },
			{
				id = "delete",
				label = "Excluir",
			},
		}
	elseif page == "confirm" then
		status = "Excluir este compromisso/tarefa?"
		buttons = { { id = "confirm_delete", label = "Confirmar exclusão" }, { id = "back", label = "Cancelar" } }
	else
		local matches = {}
		for _, r in ipairs(rows) do
			if query ~= "" and store.match(r, query) or query == "" and sys.date(tonumber(r[3]), "day") == today then
				matches[#matches + 1] = r
			end
		end
		table.sort(matches, function(a, b)
			return tonumber(a[3]) < tonumber(b[3])
		end)
		for i = offset + 1, math.min(offset + 6, #matches) do
			local r = matches[i]
			buttons[#buttons + 1] = {
				id = "edit:" .. r[1],
				label = (r[6] == "1" and "✓ " or "") .. sys.date(tonumber(r[3]), "clock") .. " " .. store.clip(
					r[2],
					60
				),
			}
		end
		for _, b in ipairs({
			{ "new", "Novo" },
			{ "today", "Hoje" },
			{ "month", "Mês" },
            { "week", "Semana" },
            { "day_prev", "Dia anterior" },
            { "day_next", "Próximo dia" },
			{ "search", "Buscar texto" },
			{ "prev", "Anterior" },
			{ "next", "Seguinte" },
			{ "import", "Importar" },
			{ "export", "Exportar" },
		}) do
			buttons[#buttons + 1] = { id = b[1], label = b[2] }
		end
	end
	return chrome.view({
		title = "Agenda • " .. today,
        layout=layout,controls=controls,result=result,tools=tools,
		body = status ~= "" and status or "Compromissos e tarefas; avisos continuam fora da Agenda.",
		field = page == "edit" or page=="day" or page=="month",
		readonly = page == "month",
		text = shown,
		buttons = buttons,
	})
end
