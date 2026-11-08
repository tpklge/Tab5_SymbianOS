-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local page, selected, kind = 0, "", ""
local status = "Fotos e vídeos • toque para abrir"
function event(action, value)
	if action == "next" then
		page = page + 1
	elseif action == "prev" then
		page = math.max(0, page - 1)
	elseif action:sub(1, 5) == "view:" then
		selected = action:sub(6)
		kind = selected:lower():match("%.avi$") and "video" or "photo"
		local ok, why = sys.media(kind == "video" and "video_play" or "image", selected)
		if not ok then
			status = tostring(why)
			selected = ""
		end
	elseif action == "back" then
		if selected ~= "" then
			if kind == "video" then
				sys.media("video_stop")
			end
			selected = ""
		else
			next_app = "shell"
		end
	elseif action == "home" or action == "suspend" then
		if kind == "video" then
			sys.media("video_stop")
		end
		if action == "home" then
			next_app = "shell"
		end
	end
	local buttons, tools = {}, {}
	if selected == "" then
		local files, total = sys.fs("list", "/home/Pictures", "", page * 8, "visual")
		files = files or {}
		for i = 1, math.min(8, #files) do
			local f = files[i]
			local lower = f.path:lower()
			if lower:match("%.jpe?g$") or lower:match("%.png$") or lower:match("%.avi$") then
				buttons[#buttons + 1] = {
					id = "view:" .. f.path,
					label = f.path:match("[^/]+$"),
					icon = lower:match("%.avi$") and "ui_video" or "gallery",
					thumbnail = f.path,
				}
			end
		end
		tools = {
			{ id = "prev", label = "Anterior", icon = "ui_prev" },
			{ id = "next", label = "Próxima", icon = "ui_next" },
		}
	end
	return chrome.view({
		title = "Galeria",
		layout = selected == "" and "gallery" or kind == "video" and "video_view" or "photo",
		body = selected == "" and status or selected,
		buttons = buttons,
		tools = tools,
	})
end
