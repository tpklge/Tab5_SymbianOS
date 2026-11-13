# Transporte P4/C6 — 0.15.11

A foto 0.15.10 confirma Disconnect=ESP_FAIL, Stop=ESP_FAIL, Deinit=ESP_FAIL e ausência de STA_STOP. A OTA também havia parado. Isso indica indisponibilidade do caminho remoto de controle/dados, sem demonstrar a causa exata (C6, barramento, filas ou software).

A dependência instalada ESP-Hosted 1.4.0 possui `transport_deinit_internal()` SDIO com corpo TODO. `esp_hosted_reinit()` chama novamente a inicialização, mas esse caminho não desmonta as tarefas/filas SDIO anteriores; não foi adotado como recuperação segura. `esp_hosted_slave_reset()` só reconfigura quando o estado interno não diz que o transporte está ativo. Resetar GPIO do C6 com tarefas e contadores SDIO antigos também não constitui uma recuperação completa.

A 0.15.11 não promete recuperar o barramento em funcionamento. Após a sequência de três ESP_FAIL sem STA_STOP, marca necessidade de reinício e bloqueia novos scans, conexões, consultas RSSI e tentativas de recuperação. Evita repetir longas esperas RPC sem progresso. O status limpa IP/token/conexão ativos, e as telas de rede/e-mail oferecem Reiniciar… que abre Atualizações e recuperação. O usuário ainda precisa tocar em Reiniciar e Confirmar; não há reboot automático. Salve os documentos antes. Dependendo do launcher, selecione o Symbian novamente após reiniciar.

O aviso Recebendo/Enviando em segundo plano desaparece quando o worker termina. A nova informação de rede no status de e-mail usa a capacidade mail existente, sem conceder acesso network adicional.

Um buffer circular fixo retém até 12 avisos/erros dos tags SDIO/sdmmc/transport, sem gravar no cartão dentro do callback de logs. Após a tentativa de recuperação, exporta `/SymbianOS/logs/wifi-sdio.txt`. Guarde também `wifi-driver-recovery.txt` e `network-recovery.txt`. Ausência de avisos capturados não comprova saúde do transporte. O arquivo só contém avisos ocorridos desde esta inicialização; não recupera logs da versão anterior.

Há relatos upstream de falhas SDIO com RPCs esp_wifi indisponíveis em outras versões e placas: [Espressif EHM-188](https://github.com/espressif/esp-hosted-mcu/issues/167). São contexto de investigação, não prova da mesma causa no Tab5.

Atualize BIN e SD juntos. Após reiniciar, confira primeiro o Wi-Fi e a página OTA, depois tente Receber uma vez. Se travar novamente, os três arquivos acima permitem distinguir erros SDIO observáveis de falha sem logs do barramento. Corrigir a causa da perda de tráfego permanece pendente de evidência física; sem flash, serial ou atualização do C6 nesta entrega.
