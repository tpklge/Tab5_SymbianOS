-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local message = ""
function event(action, value)
	if action == "fast" or action == "normal" or action == "off" then
		if sys.power(action) then
			local ok, why = sys.write("/config/public/charge", action)
			message = ok and "Modo aplicado. A corrente medida confirma se a bateria está carregando." or tostring(why)
		else
			message = "Não foi possível aplicar o modo de carga."
		end
	elseif action == "volume_up" or action == "volume_down" then
		local v = sys.media("volume")
		v = sys.media("volume", "", v + (action == "volume_up" and 10 or -10))
		sys.write("/config/public/volume", tostring(v))
	elseif action == "home" or action == "back" then
		next_app = "shell"
	end
	local p = sys.power()
	local body = p.available
			and ("Bateria estimada: " .. p.percent .. "% • " .. p.millivolts .. " mV\nCorrente: " .. p.milliamps .. " mA • " .. (p.charging and "Carregando" or "Sem corrente de carga detectada"))
		or "Monitor de bateria indisponível"
	return chrome.view({
		title = "Energia e volume",
		layout = "editor",
		field = true,
		readonly = true,
		text = body .. "\nVolume: " .. sys.media("volume") .. "% • " .. message,
		buttons = {
			{
				id = "fast",
				label = "Carga rápida / QC",
				subtitle = "Solicita negociação do carregador; depende de fonte e cabo compatíveis.",
			},
			{ id = "normal", label = "Carga normal", subtitle = "Habilita carga sem solicitar QC." },
			{ id = "off", label = "Pausar carga", subtitle = "Mantém o uso da alimentação disponível." },
			{ id = "volume_down", label = "Diminuir volume -" },
			{ id = "volume_up", label = "Aumentar volume +" },
			{ id = "refresh", label = "Atualizar medição" },
		},
	})
end
