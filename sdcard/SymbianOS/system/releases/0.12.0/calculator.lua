-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local session = store.resume("calculator")
local expression, answer = session and session[2] or "", "Toque nas teclas ou digite a expressão"
local fresh = false
-- Recursive descent over numeric expressions; no load/eval or native execution.
local function calculate(s)
	local pos, depth = 1, 0
	local problem
	if #s > 512 then
		return "Expressão muito longa"
	end
	local expr, term, atom
	local function spaces()
		while s:sub(pos, pos):match("%s") do
			pos = pos + 1
		end
	end
	local function accept(c)
		spaces()
		if s:sub(pos, pos) == c then
			pos = pos + 1
			return true
		end
	end
	atom = function()
		depth = depth + 1
		if depth > 24 then
			problem = "Expressão muito profunda"
			return 0
		end
		local n
		if accept("-") then
			n = -atom()
		elseif accept("+") then
			n = atom()
		elseif accept("(") then
			n = expr()
			if not accept(")") then
				problem = "Falta fechar parênteses"
			end
		else
			spaces()
			local token = s:sub(pos):match("^%d*%.?%d+")
			if not token then
				problem = "Número esperado"
				depth = depth - 1
				return 0
			end
			n = tonumber(token)
			pos = pos + #token
		end
		depth = depth - 1
		return n
	end
	term = function()
		local n = atom()
		while true do
			if accept("*") then
				n = n * atom()
			elseif accept("/") then
				local d = atom()
				if d == 0 then
					problem = "Divisão por zero"
					d = 1
				end
				n = n / d
			else
				break
			end
		end
		return n
	end
	expr = function()
		local n = term()
		while true do
			if accept("+") then
				n = n + term()
			elseif accept("-") then
				n = n - term()
			else
				break
			end
		end
		return n
	end
	local n = expr()
	spaces()
	if pos <= #s then
		problem = "Caractere inválido"
	end
	if n ~= n or n == math.huge or n == -math.huge then
		problem = "Resultado fora do limite"
	end
	return problem or string.format("%.12g", n)
end
-- The runtime exposes a protected calculator callback so malformed input is a
-- recoverable app error, without exposing pcall to untrusted infinite loops.
function event(action, value)
	if action ~= "focus" then
		expression = value
	end
	if action == "suspend" or action == "checkpoint" then
		local ok, why = store.session("calculator", "editor", expression)
		if not ok then
			navigation_blocked = true
			answer = tostring(why)
		end
	end
	if action:sub(1, 4) == "key:" then
		local key = action:sub(5)
		if fresh and key:match("^[%d%.]$") then
			expression = ""
		end
		fresh = false
		expression = expression .. key
	elseif action == "erase" then
		fresh = false
		expression = expression:sub(1, -2)
	elseif action == "clear" then
		expression = ""
		fresh = false
		answer = "Pronto"
	elseif action == "calculate" or action == "options" then
		answer = calculate(expression)
		fresh = tonumber(answer) ~= nil
		if fresh then
			expression = answer
		end
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	local keys = {
		{ "clear", "C" },
		{ "key:(", "(" },
		{ "key:)", ")" },
		{ "key:/", "÷" },
		{ "key:7", "7" },
		{ "key:8", "8" },
		{ "key:9", "9" },
		{ "key:*", "×" },
		{ "key:4", "4" },
		{ "key:5", "5" },
		{ "key:6", "6" },
		{ "key:-", "-" },
		{ "key:1", "1" },
		{ "key:2", "2" },
		{ "key:3", "3" },
		{ "key:+", "+" },
		{ "erase", "Apagar" },
		{ "key:0", "0" },
		{ "key:.", "," },
		{ "calculate", "=" },
	}
	local buttons = {}
	for _, k in ipairs(keys) do
		buttons[#buttons + 1] = { id = k[1], label = k[2] }
	end
	return chrome.view({
		title = "Calculadora",
		layout = "calculator",
		body = answer,
		field = true,
		text = expression,
		left = "Calcular",
		buttons = buttons,
	})
end
