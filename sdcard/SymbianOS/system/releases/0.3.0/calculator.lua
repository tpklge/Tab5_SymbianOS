-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local store = sys.module("store")
local session = store.resume("calculator")
local expression, answer = session and session[2] or "", "Operadores: + − * / e parênteses"
-- Recursive descent over numeric expressions; no load/eval or native execution.
local function calculate(s)
	local pos, depth = 1, 0
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
			error("expressão muito profunda")
		end
		local n
		if accept("-") then
			n = -atom()
		elseif accept("+") then
			n = atom()
		elseif accept("(") then
			n = expr()
			if not accept(")") then
				error("falta )")
			end
		else
			spaces()
			local token = s:sub(pos):match("^%d*%.?%d+")
			if not token then
				error("número esperado")
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
					error("divisão por zero")
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
		error("caractere inválido")
	end
	if n ~= n or n == math.huge or n == -math.huge then
		error("resultado fora do limite")
	end
	return string.format("%.12g", n)
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
	if action == "clear" then
		expression = ""
		answer = "Pronto"
	elseif action == "calculate" then
		answer = calculate(expression)
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	return chrome.view({
		title = "Calculadora",
		body = answer,
		field = true,
		text = expression,
		buttons = { { id = "calculate", label = "Calcular" }, { id = "clear", label = "Limpar" } },
	})
end
