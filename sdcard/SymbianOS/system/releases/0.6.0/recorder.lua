-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local page = 0
function event(action, value)
	if action == "record" then
		sys.media("record", "/home/Recordings/voz-" .. tostring(sys.epoch()) .. "-" .. tostring(sys.new_id()) .. ".au")
	elseif action == "stop" or action == "pause" then
		sys.media(action)
	elseif action:sub(1, 5) == "play:" then
		sys.media("play", action:sub(6))
	elseif action == "next" then
		page = page + 1
	elseif action == "prev" then
		page = math.max(0, page - 1)
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	local buttons = {}
	local files = sys.fs("list", "/home/Recordings", "", page * 8, "recordings") or {}
	for i = 1, math.min(8, #files) do
		local f = files[i]
		if f.path:match("%.au$") or f.path:match("%.wav$") then
			buttons[#buttons + 1] = {
				id = "play:" .. f.path,
				label = f.path:match("[^/]+$"),
				subtitle = tostring(f.size) .. " bytes",
				icon = "recorder",
			}
		end
	end
	return chrome.view({
		title = "Gravador • voz compacta AU",
		layout = "recorder",
		body = sys.media("status"),
		mediaStatus = true,
		buttons = buttons,
		tools = {
			{ id = "record", label = "Gravar", icon = "ui_record" },
			{ id = "pause", label = "Pausar", icon = "ui_pause" },
			{ id = "stop", label = "Salvar", icon = "ui_stop" },
			{ id = "prev", label = "Página −", icon = "ui_prev" },
			{ id = "next", label = "Página +", icon = "ui_next" },
		},
	})
end
