-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local scanPage, savedPage, knownSSID = 0, 0, ""
local page, ssid, password, message = "main", "", "", ""
local function lines(value)
	local a, b = (value or ""):match("^([^\n]*)\n?(.*)$")
	return a or "", b or ""
end
function event(action, value)
	if action == "restart_help" then
        next_app="energy"
    elseif action == "saved" then
		page = "saved"
	elseif action == "saved_next" then
		savedPage = savedPage + 1
	elseif action == "saved_prev" then
		savedPage = math.max(0, savedPage - 1)
	elseif action:sub(1, 6) == "known:" then
		local item = (sys.net("saved") or {})[tonumber(action:sub(7)) or 0]
		if item then
			knownSSID = item.ssid
			page = "known"
		end
	elseif action == "known_connect" then
		message = sys.net("connect_saved", knownSSID) and "Conectando à rede salva…"
			or "Não foi possível iniciar a conexão."
		page = "main"
	elseif action == "auto_on" or action == "auto_off" then
		message = sys.net("automatic", knownSSID, action == "auto_on" and "on" or "off") and "Preferência salva."
			or "Não foi possível salvar a preferência."
	elseif action == "forget" then
		if sys.net().ssid == knownSSID then
			sys.net("disconnect")
		end
		message = sys.net("forget", knownSSID) and "Rede esquecida." or "Aguarde a operação atual ou confira o SD."
		page = "saved"
	elseif action == "scan" then
		message = sys.net("scan") and "Buscando redes…"
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
	elseif action == "connect" or action == "connect_once" then
		ssid, password = lines(value)
		message = sys.net("connect", ssid, password, action == "connect")
				and "Conectando…"
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
		message = sys.net("download", url, "/home/Transfer/" .. name) and "Baixando…"
			or "URL/nome inválido, perfil Offline ou outra operação ativa."
		page = "main"
	elseif action == "recover" then
    sys.net("recover")
  elseif action == "cancel" then
		sys.net("cancel")
	elseif action == "back" and page ~= "main" then
		page = "main"
	elseif action == "back" or action == "home" then
		next_app = "shell"
	end
	if page == "saved" then
		local list = sys.net("saved") or {}
		savedPage = math.min(savedPage, math.max(0, math.floor((#list - 1) / 6)))
		local buttons = {}
		for i = savedPage * 6 + 1, math.min(savedPage * 6 + 6, #list) do
			local p = list[i]
			buttons[#buttons + 1] = {
				id = "known:" .. i,
				label = p.ssid,
				subtitle = p.automatic and "Conexão automática ativada" or "Conexão automática desativada",
				icon = "network",
			}
		end
		if savedPage > 0 then
			buttons[#buttons + 1] = { id = "saved_prev", label = "Página anterior" }
		end
		if #list > (savedPage + 1) * 6 then
			buttons[#buttons + 1] = { id = "saved_next", label = "Próxima página" }
		end
		buttons[#buttons + 1] = { id = "scan", label = "Buscar outra rede" }
		buttons[#buttons + 1] = { id = "wifi", label = "Adicionar rede manualmente" }
		return chrome.view({
			title = "Redes salvas",
			layout = "list",
			body = #list == 0 and "Nenhuma rede salva. Conecte e salve uma rede para ativar a conexão automática."
				or ("Selecione uma rede • " .. message),
			buttons = buttons,
		})
	end
	if page == "known" then
		local automatic = false
		for _, p in ipairs(sys.net("saved") or {}) do
			if p.ssid == knownSSID then
				automatic = p.automatic
			end
		end
		return chrome.view({
			title = "Rede conhecida",
            layout="status",refreshMs=1000,body=message,
            statusFields={{label="Rede",value=knownSSID},{label="Conexão automática",value=automatic and "Ativada" or "Desativada"}},
			buttons = {
				{ id = "known_connect", label = "Conectar" },
				{
					id = automatic and "auto_off" or "auto_on",
					label = automatic and "Desativar conexão automática" or "Ativar conexão automática",
				},
				{ id = "forget", label = "Esquecer rede" },
				{ id = "saved", label = "Todas as redes salvas" },
			},
		})
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
		buttons[#buttons + 1] = { id = "scan", label = "Buscar novamente" }
		buttons[#buttons + 1] = { id = "wifi", label = "Digitar SSID manualmente" }
		buttons[#buttons + 1] = { id = "cancel", label = "Cancelar busca" }
		if scanPage > 0 then
			buttons[#buttons + 1] = { id = "scan_prev", label = "Página anterior" }
		end
		if #list > (scanPage + 1) * 6 then
			buttons[#buttons + 1] = { id = "scan_next", label = "Próxima página" }
		end
		return chrome.view({
			title = "Redes Wi-Fi • 2,4 GHz",
			layout = "list",
            refreshMs=1000,
			body = status.message,
			buttons = buttons,
		})
	end
	if page == "wifi" then
		return chrome.view({
			title = "Conectar Wi-Fi",
			layout = "form",
			field = true,
			fields = "Nome da rede (SSID)\nSenha",
			controls = { { label = "Nome da rede (SSID)", type = "text" }, { label = "Senha", type = "password" } },
			text = ssid .. "\n",
			body = "2,4 GHz • Conectar e salvar guarda a senha no SD e ativa a conexão automática.",
			buttons = {
				{ id = "connect", label = "Conectar e salvar" },
				{ id = "connect_once", label = "Conectar sem salvar" },
				{ id = "scan", label = "Buscar redes" },
			},
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
    local s=sys.net()
    local buttons={{id="recover",label="Reconectar Wi-Fi",icon="ui_refresh"},{id="scan",label="Buscar redes",icon="ui_search"},{id="saved",label="Redes salvas",icon="ui_save"},{id="wifi",label="Conectar",icon="network"},
      {id=s.sharing and "share_off" or "share",label=s.sharing and "Encerrar envio web" or "Enviar arquivos",icon=s.sharing and "ui_stop" or "ui_share"},
      {id="download",label="Download",icon="ui_download"},{id="disconnect",label="Desconectar",icon="ui_offline"}}
    if s.restart_required then
      buttons={{id="restart_help",label="Reiniciar…",icon="ui_refresh"},{id="saved",label="Redes salvas",icon="ui_save"}}
      message=""
    end
    return chrome.view({title="Wi-Fi e transferências",layout="status",refreshMs=1000,body=(message:match("^Não") or message:match("^Falha") or message:match("^URL/")) and message or s.message,
      statusFields={{label="Rede",value=s.ssid~="" and s.ssid or "Nenhuma"},{label="Conexão",value=s.busy and "Em andamento" or s.connected and "Conectada" or "Desconectada"},
       {label="Endereço IP",value=s.connected and s.address~="" and s.address or "Indisponível"},{label=s.sharing and "Envio web / token" or "Transferência",value=s.sharing and ("http://"..(s.transfer_address and s.transfer_address~="" and s.transfer_address or s.address).." • "..s.token) or (s.bytes.." bytes")}},
      buttons=buttons,footer=s.busy and {{id="cancel",label="Cancelar",icon="ui_cancel"}} or {}})
end
