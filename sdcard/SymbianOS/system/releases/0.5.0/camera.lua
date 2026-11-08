-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local status = "Atualize a prévia antes de fotografar."
local visible = false
function event(action, value)
	if action == "preview" then
		local ok, why = sys.media("preview")
		visible = ok
		status = why
	elseif action == "capture" then
		local path = "/home/Pictures/foto-" .. tostring(sys.epoch()) .. "-" .. tostring(sys.new_id()) .. ".jpg"
		local ok, why = sys.media("capture", path)
		visible = ok
		status = ok and path or why
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	return chrome.view({
		title = "Câmera",
		layout = visible and "image" or "list",
		body = status,
		buttons = {
			{ id = "preview", label = "Atualizar prévia" },
			{ id = "capture", label = "Fotografar e salvar JPEG" },
		},
	})
end
