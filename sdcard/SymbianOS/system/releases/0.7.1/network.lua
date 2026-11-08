-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local scanPage = 0
local page, ssid, password, message = "main", "", "", ""
local function lines(value)
	local a, b = (value or ""):match("^([^\n]*)\n?(.*)$")
	return a or "", b or ""
end
function event(action, value)
	if action == "scan" then
		message = sys.net("scan") and "Buscando redes… toque Atualizar para ver o resultado."
			or "Busca não iniciada; confira o perfil Offline."
		page = "scan"
		scanPage = 0
	elseif action == "scan_next" then
		scanPage = scanPage + 1
	elseif action == "scan_prev" then
		scanPage = math.max(0, scanPage - 1)
	elseif action:sub(1, 5) == "ssid:" then
		local list = sys.net().networks or {}
		local ap = list[tonumber(action:sub(6)) or 0]
		if ap then
			ssid = ap.ssid
			password = ""
			page = "wifi"
		end
	elseif action == "wifi" then
		page = "wifi"
	elseif action == "connect" then
		ssid, password = lines(value)
		message = sys.net("connect", ssid, password) and "Conectando… use Atualizar para acompanhar."
			or "Não foi possível iniciar; confira o perfil Offline e os campos."
		password = ""
		page = "main"
	elseif action == "share" then
		message = sys.net("share", "on") and "Iniciando transferência local…" or "Operação ocupada"
		page = "main"
	elseif action == "share_off" then
		sys.net("share", "off")
	elseif action == "disconnect" then
		sys.net("disconnect")
	elseif action == "download" then
		page = "download"
	elseif action == "fetch" then
		local url, name = lines(value)
		message = sys.net("download", url, "/home/Transfer/" .. name) and "Baixando… use Atualizar ou Cancelar."
			or "URL/nome inválido, perfil Offline ou outra operação ativa."
		page = "main"
	elseif action == "cancel" then
		sys.net("cancel")
	elseif action == "back" and page ~= "main" then
		page = "main"
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	if page == "scan" then
		local status = sys.net()
		local list = status.busy and {} or (status.networks or {})
		scanPage = math.min(scanPage, math.max(0, math.floor((#list - 1) / 6)))
		local buttons = {}
		for i = scanPage * 6 + 1, math.min(scanPage * 6 + 6, #list) do
			local ap = list[i]
			local name = utf8.len(ap.ssid) and ap.ssid or "Nome não UTF-8"
			buttons[#buttons + 1] = {
				id = "ssid:" .. i,
				label = name,
				subtitle = ap.security .. " • " .. ap.signal .. " dBm • canal " .. ap.channel,
				icon = "network",
			}
		end
		buttons[#buttons + 1] = { id = "refresh", label = "Atualizar resultados" }
		buttons[#buttons + 1] = { id = "scan", label = "Buscar novamente" }
		buttons[#buttons + 1] = { id = "wifi", label = "Digitar SSID manualmente" }
		buttons[#buttons + 1] = { id = "cancel", label = "Cancelar busca" }
		if scanPage > 0 then
			buttons[#buttons + 1] = { id = "scan_prev", label = "Página anterior" }
		end
		if #list > (scanPage + 1) * 6 then
			buttons[#buttons + 1] = { id = "scan_next", label = "Próxima página" }
		end
		return chrome.view({ title = "Redes Wi-Fi • 2,4 GHz", layout = "list", body = status.message, buttons = buttons })
	end
	if page == "wifi" then
		return chrome.view({
			title = "Conectar Wi-Fi",
			layout = "form",
			field = true,
			fields = "Nome da rede (SSID)\nSenha",
			controls = { { label = "Nome da rede (SSID)", type = "text" }, { label = "Senha", type = "password" } },
			text = ssid .. "\n",
			body = "Rede de 2,4 GHz • senha usada apenas para esta conexão, sem salvar no SD",
			buttons = { { id = "connect", label = "Conectar" }, { id = "scan", label = "Buscar redes" } },
		})
	end
	if page == "download" then
		return chrome.view({
			title = "Download",
			layout = "form",
			field = true,
			fields = "URL HTTP(S) final\nNome do arquivo",
			text = "https://\narquivo.bin",
			body = "Destino: home/Transfer • até 64 MiB • arquivos existentes são preservados",
			buttons = { { id = "fetch", label = "Baixar" } },
		})
	end
	local s = sys.net()
	return chrome.view({
		title = "Rede e transferências",
		layout = "editor",
		field = true,
		readonly = true,
		text = s.message
			.. " • "
			.. s.bytes
			.. " bytes\n"
			.. (s.sharing and ("http://" .. s.address .. " • token: " .. s.token) or s.address)
			.. "\n"
			.. message,
		buttons = {
			{ id = "scan", label = "Buscar redes Wi-Fi" },
			{ id = "wifi", label = "Digitar SSID / conectar" },
			{
				id = "share",
				label = "Ativar transferência local",
				subtitle = "Acesso por token somente à pasta home/Transfer",
			},
			{ id = "share_off", label = "Encerrar transferência" },
			{ id = "download", label = "Baixar arquivo" },
			{ id = "cancel", label = "Cancelar operação" },
			{ id = "disconnect", label = "Desconectar Wi-Fi" },
			{ id = "refresh", label = "Atualizar estado" },
		},
	})
end
