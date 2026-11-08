-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local session = store.resume("converter")
local text = session and session[2] or "1\nkm\nm"
local status = "Escolha as unidades nos menus."
local choices =
	"Metro (m)\nQuilômetro (km)\nCentímetro (cm)\nMilímetro (mm)\nPolegada (inches)\nPé (ft)\nQuilograma (kg)\nGrama (g)\nLibra (lb)\nLitro (l)\nMililitro (ml)\nSegundo (s)\nMinuto (min)\nHora (h)\nCelsius (c)\nFahrenheit (f)\nKelvin (k)\nQuilômetros por hora (kmh)\nMetros por segundo (ms)"
local keys = "m\nkm\ncm\nmm\ninches\nft\nkg\ng\nlb\nl\nml\ns\nmin\nh\nc\nf\nk\nkmh\nms"
local units = {
	m = { "comprimento", 1 },
	km = { "comprimento", 1000 },
	cm = { "comprimento", 0.01 },
	mm = { "comprimento", 0.001 },
	inches = { "comprimento", 0.0254 },
	ft = { "comprimento", 0.3048 },
	kg = { "massa", 1 },
	g = { "massa", 0.001 },
	lb = { "massa", 0.45359237 },
	l = { "volume", 1 },
	ml = { "volume", 0.001 },
	s = { "tempo", 1 },
	min = { "tempo", 60 },
	h = { "tempo", 3600 },
	kmh = { "velocidade", 1 / 3.6 },
	ms = { "velocidade", 1 },
}
function event(action, value)
	if action ~= "focus" then
		text = value
	end
	if action == "swap" then
		local f = store.lines(text)
		text = (f[1] or "") .. "\n" .. (f[3] or "m") .. "\n" .. (f[2] or "km")
	elseif action == "convert" or action == "options" then
		local f = store.lines(text)
		local n = tonumber(((f[1] or ""):gsub(",", ".")))
		local a, b = (f[2] or ""):lower(), (f[3] or ""):lower()
		local result = nil
		if not n or n ~= n or math.abs(n) == math.huge then
			status = "Valor numérico inválido."
		elseif (a == "c" or a == "f" or a == "k") and (b == "c" or b == "f" or b == "k") then
			local c = a == "f" and (n - 32) * 5 / 9 or a == "k" and n - 273.15 or n
			if c < -273.15 then
				status = "Temperatura abaixo do zero absoluto."
			else
				result = b == "f" and c * 9 / 5 + 32 or b == "k" and c + 273.15 or c
			end
		elseif units[a] and units[b] and units[a][1] == units[b][1] then
			result = n * units[a][2] / units[b][2]
		else
			status = "Unidades desconhecidas ou incompatíveis."
		end
		if result then
			status = string.format("%.12g %s = %.12g %s", n, a, result, b)
		end
	elseif action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("converter", "editor", text)
		if not ok then
			error(why)
		end
	elseif action == "home" or action == "back" then
		store.session("converter", "editor", text)
		next_app = "shell"
	end
	return chrome.view({
		title = "Conversor",
		left = "Converter",
		layout = "form",
		controls = {
			{ label = "Valor", type = "text" },
			{ label = "De", type = "select", options = choices, keys = keys },
			{ label = "Para", type = "select", options = choices, keys = keys },
		},
		body = status,
		field = true,
		text = text,
		buttons = {
			{ id = "convert", label = "Converter" },
			{ id = "swap", label = "Inverter unidades" },
		},
	})
end
