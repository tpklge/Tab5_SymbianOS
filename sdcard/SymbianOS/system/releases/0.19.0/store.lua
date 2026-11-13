-- SPDX-License-Identifier: MIT
local M = {}
local function encode(s)
	return tostring(s or ""):gsub("[%%\t\r\n]", function(c)
		return string.format("%%%02X", string.byte(c))
	end)
end
local function decode(s)
	return s:gsub("%%(%x%x)", function(h)
		return string.char(tonumber(h, 16))
	end)
end
function M.clip(text, count)
	local at = utf8.offset(text, count + 1)
	return at and text:sub(1, at - 1) or text
end
function M.lines(text)
	local a = {}
	for line in (text .. "\n"):gmatch("(.-)\n") do
		a[#a + 1] = line:gsub("\r$", "")
	end
	return a
end
-- Bounded per-row append: the caller creates the T5DATA1 header once.
function M.append(path,row)
 local fields={};for _,value in ipairs(row) do fields[#fields+1]=encode(value) end
 return sys.fs('append',path,table.concat(fields,'\t')..'\n')
end
function M.read(path)
	local data, reason = sys.read(path)
	if not data then
		if reason == "not found" then
			return {}
		end
		return nil, reason
	end
	if data:sub(1, 8) ~= "T5DATA1\n" then
		return nil, "Formato desconhecido; arquivo preservado."
	end
	local rows = {}
	for line in data:sub(9):gmatch("([^\n]+)") do
		local row = {}
		for value in (line .. "\t"):gmatch("(.-)\t") do
			local decoded = decode(value)
			if decoded:find("\0", 1, true) or not utf8.len(decoded) then
				return nil, "Texto UTF-8 inválido; arquivo preservado."
			end
			row[#row + 1] = decoded
		end
		rows[#rows + 1] = row
		if #rows > 1024 then
			return nil, "Limite de 1024 registros"
		end
	end
	return rows
end
function M.write(path, rows)
	local cap = path:find("/organizer/events.tsv", 1, true) and 128
		or path:find("/organizer/alarms.tsv", 1, true) and 128
		or path:find("/organizer/contacts.tsv", 1, true) and 256
		or path:find("/services/snoozes.tsv", 1, true) and 128
		or 1024
	if #rows > cap then
		return false, "Limite de " .. cap .. " registros"
	end
	if #rows > 1024 then
		return false, "Limite de 1024 registros"
	end
	local lines = { "T5DATA1" }
	for _, r in ipairs(rows) do
		local fields = {}
		for _, value in ipairs(r) do
			local text = tostring(value)
			if text:find("\0", 1, true) or not utf8.len(text) then
				return false, "Texto UTF-8 inválido; arquivo preservado."
			end
			fields[#fields + 1] = encode(value)
		end
		lines[#lines + 1] = table.concat(fields, "\t")
	end
	local data = table.concat(lines, "\n") .. "\n"
	if #data > 65536 then
		return false, "Arquivo acima de 64 KiB"
	end
	return sys.write(path, data)
end
function M.session(id, page, text, selected)
	return M.write("/data/" .. id .. "/session.tsv", { { page, text, selected or "" } })
end
function M.resume(id)
	local rows = M.read("/data/" .. id .. "/session.tsv")
	return rows and rows[1] or nil
end
function M.id(rows)
	local used = {}
	for _, r in ipairs(rows) do
		used[r[1]] = true
	end
	for _ = 1, 257 do
		local id, why = sys.new_id()
		if not id then
			error(why)
		end
		if not used[id] then
			return id
		end
	end
	error("Sequência de IDs inválida")
end
function M.match(row, query)
	return query == "" or table.concat(row, " "):lower():find(query:lower(), 1, true) ~= nil
end
function M.saveStatus(ok, why)
	return ok and "Salvo no cartão." or ("Não foi possível salvar: " .. tostring(why))
end
return M
