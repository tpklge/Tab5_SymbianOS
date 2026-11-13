-- SPDX-License-Identifier: MIT
local chrome = sys.module("chrome")
local status = "Prepare os arquivos pelo SD. Atualizações não apagam documentos."
local reviewed, confirm = false, ""
function event(action, value)
    if action == "home" or action == "back" then next_app = "shell"
    elseif action == "inspect" then
        local ok, why = sys.update("inspect")
        reviewed, status = ok, why
    elseif action == "install" and reviewed then
        confirm = "install"
        status = "Instalar o runtime verificado no outro slot? Mantenha alimentação estável."
    elseif action == "stage" then
        confirm = "stage"
        status = "Ativar /update/manifest.txt no próximo boot? Os arquivos devem estar em /system/releases/."
    elseif action == "rollback" then
        confirm = "rollback"
        status = "Voltar à release anterior do cartão? Documentos serão preservados."
    elseif action == "runtime_previous" then
        confirm = action
        status = "Selecionar runtime anterior confirmado? Use uma release SD compatível."
    elseif action == "reboot" then
        confirm = "reboot"
        status = "Reiniciar agora? Salve os documentos dos outros aplicativos antes."
    elseif action == "confirm" and confirm ~= "" then
        local ok, why = sys.update(confirm)
        status = why
        confirm, reviewed = "", false
    elseif action == "cancel" then confirm = ""
    end
    local buttons
    if confirm ~= "" then
        buttons = {{id="confirm",label="Confirmar"},{id="cancel",label="Cancelar"}}
    else
        buttons = {
            {id="inspect",label="Revisar runtime",subtitle="/update/runtime.bin e runtime.sha256"},
            {id="stage",label="Preparar release do SD",subtitle="Verifica hashes e compatibilidade antes de ativar."},
            {id="rollback",label="Restaurar release anterior"},
            {id="runtime_previous",label="Runtime anterior"},
            {id="reboot",label="Reiniciar"},
        }
        if reviewed then table.insert(buttons,2,{id="install",label="Instalar runtime revisado"}) end
    end
    return chrome.view({title="Atualizações e recuperação",layout="editor",field=true,readonly=true,
        text=status,buttons=buttons})
end
