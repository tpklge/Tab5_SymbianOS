-- SPDX-License-Identifier: MIT
local store = sys.module("store")
local ledger = "/data/services/delivered.tsv"
local history = "/data/services/history.tsv"
local snoozes = "/data/services/snoozes.tsv"
local function load(path)
	local rows, why = store.read(path)
	if not rows then
		error("Serviço preservou arquivo: " .. tostring(why))
	end
	return rows
end
local function put(path, rows)
	local ok, why = store.write(path, rows)
	if not ok then
		error(why)
	end
end
local seen, historyRows, lookup, pending, active
local function deliver(id, title, due, alarm)
	title = store.clip(title, 48)
	if lookup[id] and tonumber(lookup[id][2]) >= due then
		return
	end
	if lookup[id] then
		lookup[id][2] = tostring(due)
	else
		local row = { id, tostring(due) }
		seen[#seen + 1] = row
		lookup[id] = row
	end
	local found = false
	for _, r in ipairs(historyRows) do
		if r[1] == id then
			r[2] = title
			r[3] = alarm and "1" or "0"
			r[4] = tostring(due)
			found = true
		end
	end
	if not found then
		historyRows[#historyRows + 1] = { id, title, alarm and "1" or "0", tostring(due) }
	end
	while #historyRows > 32 do
		table.remove(historyRows, 1)
	end
	pending[#pending + 1] = { id, title, alarm, due }
end
function event(action, value)
	if action == "tick" then
		sys.net("autoconnect")
	end
	seen = load(ledger)
	historyRows = load(history)
	lookup = {}
	pending = {}
	active = {}
	for _, r in ipairs(seen) do
		lookup[r[1]] = r
	end
	if action == "start" then
		local profile = sys.read("/config/public/profile") or "normal"
		if not sys.offline(profile == "offline") then
			sys.notify("profile", "Não foi possível aplicar o perfil de rádio", false, false)
		end
		local zone = sys.read("/config/public/timezone")
		if zone then
			sys.zone(zone)
		end
		for _, r in ipairs(load(history)) do
			sys.notify(r[1], r[2], r[3] == "1", false)
		end
	elseif action == "ack" then
		for i = #historyRows, 1, -1 do
			if historyRows[i][1] == value then
				table.remove(historyRows, i)
			end
		end
		put(history, historyRows)
		sys.dismiss(value)
	elseif action == "snooze" then
		local rows = load(snoozes)
		local id = value:gsub(":snooze.*$", "") .. ":snooze"
		local title = "Soneca"
		for _, r in ipairs(load(history)) do
			if r[1] == value then
				title = r[2]
			end
		end
		local found = false
		for _, r in ipairs(rows) do
			if r[1] == id then
				r[2] = title
				r[3] = tostring(sys.epoch() + 300)
				found = true
			end
		end
		if not found then
			rows[#rows + 1] = { id, title, tostring(sys.epoch() + 300) }
		end
		put(snoozes, rows)
	elseif action == "post" then
		deliver("message:" .. tostring(sys.epoch()), value, sys.epoch(), false)
	elseif action == "tick" then
		sys.schedule("begin")
		local profile = sys.read("/config/public/profile") or "normal"
		local function register(id, title, due, repeatTime, alarm)
			active[id] = true
			if due and due >= 946684800 and due <= 4102444799 then
				sys.schedule(
					"add",
					id,
					store.clip(title, 48),
					due,
					repeatTime,
					alarm,
					alarm or profile == "normal",
					lookup[id] and tonumber(lookup[id][2]) or 0
				)
			end
		end

		local now = sys.epoch()
		if now < 946684800 then
			return
		end
		for _, r in ipairs(load("/data/organizer/events.tsv")) do
			local due = tonumber(r[3])
			if due and r[6] ~= "1" then
				register("agenda:" .. r[1], r[2], due, 0, false)
			end
			if due and due <= now and r[6] ~= "1" then
				deliver("agenda:" .. r[1], r[2], due, false)
			end
		end
		for _, r in ipairs(load("/data/organizer/alarms.tsv")) do
			local due = tonumber(r[3])
			local repeatTime = tonumber(r[4]) or 0
			if due and r[5] == "1" then
				register("alarm:" .. r[1], r[2], due, repeatTime, true)
			end
			if due and r[5] == "1" and due <= now then
				if repeatTime > 0 then
					due = due + math.floor((now - due) / repeatTime) * repeatTime
				end
				deliver("alarm:" .. r[1], r[2], due, true)
			end
		end
		for _, r in ipairs(load(snoozes)) do
			local due = tonumber(r[3])
			if due then
				register(r[1], r[2], due, 0, true)
			end
			if due and due <= now then
				deliver(r[1], r[2], due, true)
			end
		end
	end
	if #pending > 0 then
		put(history, historyRows)
		put(ledger, seen)
		local profile = sys.read("/config/public/profile") or "normal"
		local rang = false
		local hasAlarm = false
		for _, p in ipairs(pending) do
			hasAlarm = hasAlarm or p[3]
		end
		for _, p in ipairs(pending) do
			local sound = not rang and (p[3] or (not hasAlarm and profile == "normal"))
			sys.notify(p[1], p[2], p[3], sound, p[4])
			rang = rang or sound
		end
	end
	if action == "tick" then
		local retain = {}
		for _, n in ipairs(historyRows) do
			retain[n[1]] = true
		end
		local kept = {}
		for _, row in ipairs(seen) do
			if active[row[1]] or retain[row[1]] then
				kept[#kept + 1] = row
			end
		end
		if #kept < #seen then
			put(ledger, kept)
		end
		local rows = load(snoozes)
		local remaining = {}
		for _, row in ipairs(rows) do
			local due = tonumber(row[3])
			if not due or due > sys.epoch() or not lookup[row[1]] or tonumber(lookup[row[1]][2]) < due then
				remaining[#remaining + 1] = row
			end
		end
		if #remaining < #rows then
			put(snoozes, remaining)
		end
		sys.schedule("commit")
	end
end
