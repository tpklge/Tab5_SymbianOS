# Recuperação do driver Wi-Fi — 0.15.10

O erro “Modo de busca STA: ESP_FAIL (-1)” ocorre na preparação do Wi-Fi, antes da autenticação IMAP. É um erro genérico do driver; não identifica sozinho a causa da perda de tráfego.

A recuperação agora desconecta, solicita parada, observa o evento STA_STOP e tenta desinicializar o driver Wi-Fi. Só recria o driver quando a limpeza foi aceita. Mantém a infraestrutura de netif, eventos e ESP-Hosted, evitando handlers duplicados. Não força reset do C6 nem reinício do P4: se o transporte RPC estiver travado, a recuperação ainda pode falhar.

Após falha, as buscas automáticas ficam suspensas para preservar o erro e liberar uma nova tentativa manual. A intenção de manter a OTA ativa sobrevive a uma tentativa malsucedida; uma recuperação posterior tenta reabrir o serviço. Confira o token exibido novamente.

O painel de diagnóstico tem fundo opaco e invalidação completa ao atualizar o mesmo objeto de texto, para limpar caracteres anteriores.

Atualize BIN e SD juntos. Toque em Reconectar Wi-Fi e teste a página OTA antes de Receber. Se falhar, envie `/SymbianOS/logs/wifi-driver-recovery.txt` e `/SymbianOS/logs/network-recovery.txt`. Eles registram etapas/resultados sem senha.

Validação: 18 suítes host, jornada LVGL, cinco suítes ASan/UBSan e build ESP32-P4 passaram. Os testes simulados cobrem falha de limpeza, ausência de nova conexão nessa situação, suspensão de buscas e restauração da OTA na tentativa manual seguinte. Recuperação física e conexão IMAP permanecem pendentes.
