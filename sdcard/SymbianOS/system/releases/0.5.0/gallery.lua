-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local page = 0
local selected, zoom, status = "", 1, "JPEG e PNG • prévia decodificada por demanda"
function event(action, value)
	if action == "next" then
		page = page + 1
	elseif action == "prev" then
		page = math.max(0, page - 1)
	elseif action:sub(1, 5) == "view:" then
		selected = action:sub(6)
		zoom = 1
		local ok, why = sys.media("image", selected, zoom)
		status = why
		if not ok then
			selected = ""
		end
	elseif action == "zoom" and selected ~= "" then
		zoom = zoom == 1 and 2 or 1
		local ok, why = sys.media("image", selected, zoom)
		status = why
	elseif action == "back" then
		if selected ~= "" then
			selected = ""
		else
			next_app = "shell"
		end
	elseif action == "home" then
		next_app = "shell"
	end
	local buttons = {}
	if selected ~= "" then
		buttons = { { id = "zoom", label = "Ampliar / ajustar" }, { id = "back", label = "Voltar à galeria" } }
	else
		local files = sys.fs("list", "/home/Pictures", "", page * 12) or {}
		for _, f in ipairs(files) do
			if not f.directory and (f.path:lower():match("%.jpe?g$") or f.path:lower():match("%.png$")) then
				buttons[#buttons + 1] = {
					id = "view:" .. f.path,
					label = f.path:match("[^/]+$"),
					subtitle = tostring(f.size) .. " bytes",
					icon = "gallery",
				}
			end
		end
	end
	if selected == "" then
		buttons[#buttons + 1] = { id = "prev", label = "Anterior" }
		buttons[#buttons + 1] = { id = "next", label = "Próxima" }
	end
	return chrome.view({
		title = "Galeria",
		layout = selected ~= "" and (zoom == 2 and "image_zoom" or "image") or "list",
		body = selected ~= "" and selected or status,
		buttons = buttons,
	})
end
