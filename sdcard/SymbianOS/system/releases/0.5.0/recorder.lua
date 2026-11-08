-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local page = 0
local status = "Gravação mono WAV, 16 kHz • máximo 16 MiB por arquivo"
function event(action, value)
	if action == "next" then
		page = page + 1
	elseif action == "prev" then
		page = math.max(0, page - 1)
	elseif action == "record" then
		local path = "/home/Recordings/voz-" .. tostring(sys.epoch()) .. "-" .. tostring(sys.new_id()) .. ".wav"
		local ok, why = sys.media("record", path)
		status = why
	elseif action == "stop" then
		local ok, why = sys.media("stop")
		status = why
	elseif action:sub(1, 5) == "play:" then
		local ok, why = sys.media("play", action:sub(6))
		status = why
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	local buttons =
		{ { id = "record", label = "Iniciar gravação" }, { id = "stop", label = "Parar / salvar gravação" } }
	local files = sys.fs("list", "/home/Recordings", "", page * 12) or {}
	for _, f in ipairs(files) do
		if f.path:match("%.wav$") then
			buttons[#buttons + 1] = { id = "play:" .. f.path, label = f.path:match("[^/]+$"), icon = "music" }
		end
	end
	buttons[#buttons + 1] = { id = "prev", label = "Anterior" }
	buttons[#buttons + 1] = { id = "next", label = "Próxima" }
	return chrome.view({ title = "Gravador", mediaStatus = true, body = sys.media("status"), buttons = buttons })
end
