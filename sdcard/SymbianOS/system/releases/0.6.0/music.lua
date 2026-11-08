-- SPDX-License-Identifier: MIT
local chrome, store = sys.module("chrome"), sys.module("store")
local page, selected = 0, ""
local library = {}
local function paths()
	local result = {}
	for offset = 0, 60, 12 do
		local files, total = sys.fs("list", "/home/Music", "", offset, "audio")
		if not files then
			break
		end
		for _, f in ipairs(files) do
			if
				not f.directory
				and (f.path:lower():match("%.mp3$") or f.path:lower():match("%.wav$") or f.path:lower():match("%.au$"))
			then
				result[#result + 1] = f.path
				if #result == 64 then
					return result
				end
			end
		end
		if offset + 12 >= total then
			break
		end
	end
	return result
end
function event(action, value)
	if action == "focus" or action == "refresh" then
		library = paths()
	end
	local all = library
	if action:sub(1, 5) == "play:" then
		selected = action:sub(6)
		local index = 1
		for i, path in ipairs(all) do
			if path == selected then
				index = i
			end
		end
		sys.media("queue", table.concat(all, "\n"), index)
	elseif action == "toggle" then
		local status = sys.media("status")
		if status:match("^playing") or status:match("^paused") then
			sys.media("pause")
		else
			sys.media("queue", table.concat(all, "\n"))
		end
	elseif action == "stop" or action == "next_track" or action == "prev_track" then
		sys.media(action)
	elseif action == "page_next" then
		page = page + 1
	elseif action == "page_prev" then
		page = math.max(0, page - 1)
	elseif action == "back" or action == "home" then
		next_app = "shell"
	elseif action == "suspend" or action == "checkpoint" then
		store.session("music", "browser", selected)
	end
	page = math.min(page, math.max(0, math.floor((#all - 1) / 6)))
	local buttons = {}
	for i = page * 6 + 1, math.min(page * 6 + 6, #all) do
		buttons[#buttons + 1] = { id = "play:" .. all[i], label = all[i]:match("[^/]+$"), icon = "music" }
	end
	return chrome.view({
		title = "Música",
		layout = "player",
		mediaStatus = true,
		body = sys.media("status"),
		buttons = buttons,
		tools = {
			{ id = "prev_track", label = "Anterior", icon = "ui_prev" },
			{ id = "toggle", label = "Tocar", icon = "ui_play" },
			{ id = "stop", label = "Parar", icon = "ui_stop" },
			{ id = "next_track", label = "Próxima", icon = "ui_next" },
			{ id = "page_prev", label = "Página −", icon = "ui_prev" },
			{ id = "page_next", label = "Página +", icon = "ui_next" },
		},
	})
end
