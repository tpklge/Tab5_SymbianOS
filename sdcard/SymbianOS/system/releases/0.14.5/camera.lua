-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local mode, filming, status = "photo", false, "Foto • toque no obturador"
function event(action, value)
	filming = sys.media("video_active")
	if action == "restart" then
		sys.media("camera_on")
	elseif action == "focus" then
		sys.media("camera_on")
	elseif action == "preview" then
		sys.media("preview")
	elseif action == "switch_mode" and not filming then
		mode = mode == "photo" and "video" or "photo"
	elseif action == "capture" then
		if mode == "photo" then
			local path = "/home/Pictures/foto-" .. tostring(sys.epoch()) .. "-" .. tostring(sys.new_id()) .. ".jpg"
			local ok, why = sys.media("capture", path)
			status = ok and "Foto salva" or why
			if ok then
				sys.media("camera_on")
			end
		elseif filming then
			sys.media("video_stop")
			filming = false
		else
			local ok, why = sys.media(
				"video_record",
				"/home/Pictures/video-" .. tostring(sys.epoch()) .. "-" .. tostring(sys.new_id()) .. ".avi"
			)
			filming = ok
			status = why
		end
	elseif action == "back" or action == "home" or action == "suspend" or action == "checkpoint" then
		if filming then
			sys.media("video_stop")
			filming = false
		end
		sys.media("camera_off")
		if action == "back" or action == "home" then
			next_app = "shell"
		end
	end
	return chrome.view({
		title = "Câmera",
		layout = "camera",
		body = mode == "video" and "Vídeo AVI/MJPEG • até 5 fps • sem áudio" or status,
		tools = {
			{ id = "restart", label = "Reiniciar", icon = "ui_play" },
			{
				id = "capture",
				label = filming and "Parar" or mode == "video" and "Gravar" or "Foto",
				icon = filming and "ui_stop" or mode == "video" and "ui_record" or "ui_photo",
			},
			{
				id = "switch_mode",
				label = mode == "photo" and "Vídeo" or "Foto",
				icon = mode == "photo" and "ui_video" or "camera",
			},
		},
	})
end
