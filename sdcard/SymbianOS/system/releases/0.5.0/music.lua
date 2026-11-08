-- SPDX-License-Identifier: MIT
local chrome, store = sys.module("chrome"), sys.module("store")
local page = 0
local status = "WAV PCM16 e MP3 • reprodução em segundo plano"
local session = store.resume("music")
local text = session and session[2] or "/home/Music"
local playlist = store.read("/data/music/playlist.tsv") or {}
function event(action, value)
	if action ~= "focus" then
		text = value
	end
	if action == "next" then
		page = page + 1
	elseif action == "prev" then
		page = math.max(0, page - 1)
	elseif action:sub(1, 5) == "play:" then
		local ok, why = sys.media("play", action:sub(6))
		status = why
	elseif action == "pause" or action == "stop" then
		local ok, why = sys.media(action)
		status = why
	elseif action == "save_list" then
		playlist = {}
		local files = sys.fs("list", text, "", page * 12) or {}
		for _, f in ipairs(files) do
			if not f.directory and (f.path:lower():match("%.wav$") or f.path:lower():match("%.mp3$")) then
				playlist[#playlist + 1] = { f.path }
			end
		end
		local ok, why = store.write("/data/music/playlist.tsv", playlist)
		status = store.saveStatus(ok, why)
	elseif action == "play_list" then
		local paths = {}
		for _, r in ipairs(playlist) do
			paths[#paths + 1] = r[1]
		end
		local ok, why = sys.media("queue", table.concat(paths, "\n"))
		status = why
	elseif action == "suspend" or action == "checkpoint" then
		store.session("music", "browser", text)
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	local buttons = {}
	local files = sys.fs("list", text, "", page * 12) or {}
	for _, f in ipairs(files) do
		if not f.directory and (f.path:lower():match("%.wav$") or f.path:lower():match("%.mp3$")) then
			buttons[#buttons + 1] = {
				id = "play:" .. f.path,
				label = f.path:match("[^/]+$"),
				subtitle = tostring(f.size) .. " bytes",
				icon = "music",
			}
		end
	end
	for _, b in ipairs({
		{ "pause", "Pausar / retomar" },
		{ "stop", "Parar" },
		{ "save_list", "Salvar playlist da página" },
		{ "play_list", "Reproduzir playlist" },
		{ "prev", "Anterior" },
		{ "next", "Próxima" },
	}) do
		buttons[#buttons + 1] = { id = b[1], label = b[2] }
	end
	return chrome.view({
		title = "Música",
		mediaStatus = true,
		layout = "files",
		field = true,
		text = text,
		body = sys.media("status"),
		buttons = buttons,
	})
end
