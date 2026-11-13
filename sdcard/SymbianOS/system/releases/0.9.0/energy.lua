-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local message = ""
local pendingSleep
function event(action, value)
	if action:sub(1, 6) == "sleep:" then
        pendingSleep = tonumber(action:sub(7))
    elseif action == "cancel" then pendingSleep = nil
    elseif action == "confirm" and pendingSleep then
        local ok, why = sys.power("sleep", pendingSleep)
        message = why or "Suspensão indisponível."
        pendingSleep = nil
    elseif action:sub(1, 8) == "timeout:" then
		message = sys.power("screen_timeout", tonumber(action:sub(9))) and "Tempo de tela salvo." or "Falha ao salvar."
	elseif action == "screen_off" then
		sys.power("screen_off")
	elseif action == "fast" or action == "normal" or action == "off" then
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
    if pendingSleep then
        return chrome.view({title="Suspender",layout="editor",field=true,readonly=true,
            text="Suspender profundamente por " .. pendingSleep .. " min? Música e rede param. Alarmes serão verificados na retomada. Toque e teclado não despertam este modo; use o temporizador ou reinicie.",
            buttons={{id="confirm",label="Confirmar suspensão"},{id="cancel",label="Cancelar"}}})
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
		text = "Tela: " .. (sys.power("screen_timeout") == 0 and "sempre ligada" or tostring(sys.power("screen_timeout")) .. " min") .. " • Toque ou teclado para despertar.\n" .. body .. "\nVolume: " .. sys.media("volume") .. "% • " .. message,
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
			{ id = "timeout:0", label = "Tela sempre ligada" },
            { id = "timeout:1", label = "Apagar após 1 min" },
            { id = "timeout:2", label = "Apagar após 2 min" },
            { id = "timeout:5", label = "Apagar após 5 min" },
            { id = "timeout:10", label = "Apagar após 10 min" },
            { id = "timeout:30", label = "Apagar após 30 min" },
            { id = "screen_off", label = "Apagar tela agora" },
            { id = "sleep:1", label = "Suspender por 1 min" },
            { id = "sleep:5", label = "Suspender por 5 min" },
            { id = "sleep:15", label = "Suspender por 15 min" },
		},
	})
end
