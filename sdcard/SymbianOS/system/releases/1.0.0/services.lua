-- SPDX-License-Identifier: MIT
local store = sys.module("store")
local bench=sys.module("bench_core")
local ledger = "/data/services/delivered.tsv"
local history = "/data/services/history.tsv"
local snoozes = "/data/services/snoozes.tsv"
local cached={}
local warning=''
local function warn(path,why)
 local message='Serviços: '..tostring(why)..' · '..path..'. Arquivo preservado.'
 if warning~=message then sys.notify('services:storage',store.clip(message,60),false,false);warning=message end
end
local function load(path) return cached[path] end
local function put(path,rows)
 local ok,why=store.write(path,rows)
 if not ok then warn(path,why) end
 return ok
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
local mesh=sys.module("mesh_core")
local function run(action, value)
	seen = load(ledger)
	historyRows = load(history)
	lookup = {}
	pending = {}
	active = {}
	for _, r in ipairs(seen) do
		lookup[r[1]] = r
	end
	if action == "start" then
		for _, r in ipairs(load(history)) do
			sys.notify(r[1], r[2], r[3] == "1", false)
		end
	elseif action == "ack" then
		for i = #historyRows, 1, -1 do
			if historyRows[i][1] == value then
				table.remove(historyRows, i)
			end
		end
		if not put(history, historyRows) then return end
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
		if not put(snoozes, rows) then return end
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
		if not put(history, historyRows) then return end
		if not put(ledger, seen) then return end
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
			if not put(ledger, kept) then return end
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
			if not put(snoozes, remaining) then return end
		end
		sys.schedule("commit")
	end
end

-- Snapshot every required file before changing data. A damaged ledger is
-- never treated as empty, and a storage error does not fault the UI refresh.
function event(action,value)
 bench.event(action,value)
 if action=="mesh" then mesh.command(value);return end
 if action=="tick" then mesh.tick() end
	if action == "tick" then
		sys.net("autoconnect")
	end
 if action=="start" then
		local profile = sys.read("/config/public/profile") or "normal"
		if not sys.offline(profile == "offline") then
			sys.notify("profile", "Não foi possível aplicar o perfil de rádio", false, false)
		end
		local zone = sys.read("/config/public/timezone")
		if zone then
			sys.zone(zone)
		end
 end
 cached={}
 local function stamp(value) local n=tonumber(value);return n and n>=0 and n<=4102444799 and n==math.floor(n) end
 local paths={ledger,history}
 if action=='tick' or action=='snooze' then paths[#paths+1]=snoozes end
 if action=='tick' then paths[#paths+1]='/data/organizer/events.tsv';paths[#paths+1]='/data/organizer/alarms.tsv' end
 for _,path in ipairs(paths) do
  local rows,why=store.read(path)
  if not rows then warn(path,why);return end
  for _,row in ipairs(rows) do
   local valid=row[1] and row[1]~='' and #row[1]<=100
   if path==ledger then valid=valid and stamp(row[2])
   elseif path==history then valid=valid and row[2] and #row[2]<=240 and utf8.len(row[2])~=nil and (row[3]=='0' or row[3]=='1') and stamp(row[4])
   else valid=valid and row[2] and #row[2]<=240 and utf8.len(row[2])~=nil and stamp(row[3]) end
   if path=='/data/organizer/alarms.tsv' then local repeatTime=tonumber(row[4]);valid=valid and #row[1]<=92 and (repeatTime==0 or repeatTime==86400) and (row[5]=='0' or row[5]=='1')
   elseif path=='/data/organizer/events.tsv' then valid=valid and #row[1]<=92 end
   if not valid then warn(path,'Registro inválido');return end
  end
  cached[path]=rows
 end
 if warning~='' then sys.dismiss('services:storage');warning='' end
 run(action,value)
end
