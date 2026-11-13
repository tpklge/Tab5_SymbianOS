-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local page = 0
function event(action, value)
 if action=="volume_set" then local level=sys.media("volume","",tonumber(value) or 60);sys.write("/config/public/volume",tostring(level)) end
	if action == "volume_up" or action == "volume_down" then
		local level = sys.media("volume")
		level = sys.media("volume", "", level + (action == "volume_up" and 10 or -10))
		sys.write("/config/public/volume", tostring(level))
	end
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
			{ id = "volume_down", label = "Vol -", icon = "ui_volume_down" },
			{ id = "volume_up", label = "Vol +", icon = "ui_volume_up" },
			{ id = "record", label = "Gravar", icon = "ui_record" },
			{ id = "pause", label = "Pausar", icon = "ui_pause" },
			{ id = "stop", label = "Salvar", icon = "ui_stop" },
			{ id = "prev", label = "Página -", icon = "ui_prev" },
			{ id = "next", label = "Página +", icon = "ui_next" },
		},
	})
end
