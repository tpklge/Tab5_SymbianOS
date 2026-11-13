-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local path = "/data/organizer/contacts.tsv"
local page, text, selected = "list", "", ""
local session = store.resume("contacts")
if session then
	page, text, selected = session[1], session[2], session[3]
end
local status = ""
local query = ""
local offset = 0
local favoritesOnly=false
local favoritePath="/data/organizer/contact-favorites.tsv"
local function alphabet(s)
	local accents = {
        ["Ñ"]="n",["ñ"]="n",
		["Á"] = "a",
		["À"] = "a",
		["Â"] = "a",
		["Ã"] = "a",
		["Ä"] = "a",
		["á"] = "a",
		["à"] = "a",
		["â"] = "a",
		["ã"] = "a",
		["ä"] = "a",
		["É"] = "e",
		["Ê"] = "e",
		["é"] = "e",
		["ê"] = "e",
		["Í"] = "i",
		["í"] = "i",
		["Ó"] = "o",
		["Ô"] = "o",
		["Õ"] = "o",
		["ó"] = "o",
		["ô"] = "o",
		["õ"] = "o",
		["Ú"] = "u",
		["Ü"] = "u",
		["ú"] = "u",
		["ü"] = "u",
		["Ç"] = "c",
		["ç"] = "c",
	}
	return (s:gsub("[\194-\244][\128-\191]+", accents):lower())
end
local function vescape(s)
	return s:gsub("\\", "\\\\"):gsub("\n", "\\n"):gsub(";", "\\;"):gsub(",", "\\,")
end
local function vunescape(s)
	return (s:gsub("\\[nN]", "\n"):gsub("\\([\\;,])", "%1"))
end
function event(action, value)
	local favorites=store.read(favoritePath) or {};local starred={};for _,f in ipairs(favorites) do starred[f[1]]=true end
	local rows, why = store.read(path)
	if not rows then
		return chrome.view({ title = "Contatos", body = tostring(why), buttons = {} })
	end
	if action ~= "focus" then
		text = value
	end
	if action=="favorites" then favoritesOnly=not favoritesOnly;page="list";offset=0
	elseif action=="star" and selected~="" then
	 if starred[selected] then for i=#favorites,1,-1 do if favorites[i][1]==selected then table.remove(favorites,i) end end;starred[selected]=nil
	 else favorites[#favorites+1]={selected};starred[selected]=true end
	 local ok,why=store.write(favoritePath,favorites);status=store.saveStatus(ok,why)
	elseif action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("contacts", page, text, selected)
		if not ok then
			error(why)
		end
	elseif action == "new" then
		page = "edit"
		selected = ""
		text = "\n\n"
		status = "Nome, telefone e e-mail, uma linha por campo."
	elseif action:sub(1, 5) == "edit:" then
		selected = action:sub(6)
		for _, r in ipairs(rows) do
			if r[1] == selected then
				text = table.concat({ r[2], r[3], r[4] }, "\n")
				page = "edit"
			end
		end
	elseif action == "search" then
		query = text
		page = "list"
		offset = 0
	elseif action == "next" then
		offset = offset + 8
	elseif action == "prev" then
		offset = math.max(0, offset - 8)
	elseif action == "save" then
		local f = store.lines(text)
		if
			not f[1]
			or f[1] == ""
			or (utf8.len(f[1]) or 9999) > 256
			or (utf8.len(f[2] or "") or 9999) > 128
			or (utf8.len(f[3] or "") or 9999) > 254
		then
			status = "Nome até 256 caracteres; telefone até 128; e-mail até 254."
		else
			local id = selected ~= "" and selected or store.id(rows)
			local found = false
			for i, r in ipairs(rows) do
				if r[1] == id then
					rows[i] = { id, f[1], f[2] or "", f[3] or "" }
					found = true
				end
			end
			if not found then
				rows[#rows + 1] = { id, f[1], f[2] or "", f[3] or "" }
			end
			local ok, why = store.write(path, rows)
			status = store.saveStatus(ok, why)
			if ok then
				page = "list"
				text = ""
				selected = ""
			end
		end
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
            for i=#favorites,1,-1 do if favorites[i][1]==selected then table.remove(favorites,i) end end
            store.write(favoritePath,favorites)
			page = "list"
			selected = ""
			text = ""
		end
	elseif action == "export" then
		local lines = {}
		for _, r in ipairs(rows) do
			for _, line in ipairs({
				"BEGIN:VCARD",
				"VERSION:3.0",
				"FN:" .. vescape(r[2]),
				"N:;" .. vescape(r[2]) .. ";;;",
				"TEL:" .. vescape(r[3]),
				"EMAIL:" .. vescape(r[4]),
				"END:VCARD",
			}) do
				while #line > 75 do
					local finish = 75
					while line:byte(finish + 1) >= 128 and line:byte(finish + 1) < 192 do
						finish = finish - 1
					end
					lines[#lines + 1] = line:sub(1, finish)
					line = " " .. line:sub(finish + 1)
				end
				lines[#lines + 1] = line
			end
		end
		local ok, why = sys.write("/home/Documents/contacts.vcf", table.concat(lines, "\r\n") .. "\r\n")
		status = ok and "Exportado: /home/Documents/contacts.vcf" or tostring(why)
	elseif action == "import" then
		local source, why = sys.read("/home/Documents/contacts.vcf")
		if not source then
			status = tostring(why)
		else
			source = source:gsub("\r\n", "\n"):gsub("\n[ \t]", "")
			local imported = {}
			local card = nil
			local version = nil
			for line in source:gmatch("[^\n]+") do
				if line == "BEGIN:VCARD" then
					if card then
						imported = nil
						status = "vCard aninhado inválido."
						break
					end
					card = { "", "", "", "" }
					version = nil
				elseif line == "END:VCARD" and card then
					if
						card[2] == ""
						or (utf8.len(card[2]) or 9999) > 256
						or (utf8.len(card[3]) or 9999) > 128
						or (utf8.len(card[4]) or 9999) > 254
						or (version ~= "3.0" and version ~= "4.0")
					then
						status = "vCard sem FN; importação recusada."
						card = nil
						imported = nil
						break
					end
					imported[#imported + 1] = card

					card = nil
				elseif card then
					local key, v = line:match("^([^:]+):(.*)$")
					if key and v then
						key = key:match("^[^;]+"):gsub("^.-%.", ""):upper()
						if key == "VERSION" then
							version = v
						elseif key == "FN" then
							card[2] = vunescape(v)
						elseif key == "TEL" then
							card[3] = vunescape(v)
						elseif key == "EMAIL" then
							card[4] = vunescape(v)
						end
					end
				end
			end
			if imported and not card and #imported > 0 then
				for _, r in ipairs(imported) do
					r[1] = store.id(rows)
					rows[#rows + 1] = r
				end
				local ok, why = store.write(path, rows)
				status = store.saveStatus(ok, why)
			elseif imported then
				status = "vCard incompleto ou vazio; nenhum dado alterado."
			end
		end
	elseif action == "back" then
		if page == "list" then
			next_app = "shell"
		end
		page = "list"
		text = ""
		selected = ""
	elseif action == "home" then
		store.session("contacts", page, text, selected)
		next_app = "shell"
	end
	local buttons = {}
	if page == "edit" then
		buttons = { { id = "save", label = "Salvar contato" }, { id = "delete", label = "Excluir contato" } };if selected~="" then buttons[#buttons+1]={id="star",label=starred[selected] and "Remover favorito" or "Adicionar favorito",icon="profiles"} end
	elseif page == "confirm" then
		status = "Excluir este contato?"
		buttons = { { id = "confirm_delete", label = "Confirmar exclusão" }, { id = "back", label = "Cancelar" } }
	else
		local matches = {}
		for _, r in ipairs(rows) do
			if store.match(r, query) and (not favoritesOnly or starred[r[1]]) then
				matches[#matches + 1] = r
			end
		end
		table.sort(matches, function(a, b)
			local x, y = alphabet(a[2]), alphabet(b[2])
			if x == y then
				return a[1] < b[1]
			end
			return x < y
		end)
		if offset >= #matches then
			offset = math.max(0, #matches - 8)
		end
		for i = offset + 1, math.min(offset + 8, #matches) do
			local r = matches[i]
			buttons[#buttons + 1] = {
				id = "edit:" .. r[1],
				label = (starred[r[1]] and "★ " or "")..store.clip(r[2], 36),
				subtitle = r[3] ~= "" and r[3] or r[4],
				icon = "contacts",
			}
		end
		for _, b in ipairs({
			{ "new", "Novo" },
            { "favorites", favoritesOnly and "Todos os contatos" or "Favoritos" },
			{ "search", "Buscar texto" },
			{ "import", "Importar vCard" },
			{ "export", "Exportar vCard" },
			{ "prev", "Anterior" },
			{ "next", "Próxima" },
		}) do
			buttons[#buttons + 1] = { id = b[1], label = b[2] }
		end
	end
	return chrome.view({
		title = "Contatos",
		layout = page == "list" and "contacts" or page == "edit" and "form" or "list",
		fields = page == "edit" and "Nome\nTelefone\nE-mail" or "Buscar contato",
		body = status ~= "" and status or "Contatos • A–Z",
		field = page ~= "confirm",
		text = text,
		buttons = buttons,
	})
end
