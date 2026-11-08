# 0.7.1 / API 7 — câmera e redes Wi-Fi

## Câmera: erro 22 em Configurar RGB565

O usuário confirmou a etapa do erro. No ESP Video instalado, `esp_video_get_format` copia a estrutura inteira do stream, mas a inicialização CSI preenche dimensões/pixel format sem necessariamente preencher `type`. Assim, `G_FMT` pode sobrescrever o seletor `V4L2_BUF_TYPE_VIDEO_CAPTURE` com zero. Reutilizar essa estrutura em `S_FMT` faz o roteamento de stream falhar com EINVAL (22).

O residente agora cria uma requisição de captura nova com tipo explícito e dimensões nativas. Se o CSI/ISP já entrega RGB565, mantém esse formato e não faz uma reconfiguração redundante. Metadados de outro formato não são reaproveitados. O restante da captura, áudio, carregamento e controles permanece compatível.

O teste de protocolo da câmera usa os headers V4L2 da dependência e reproduz exatamente `G_FMT` devolvendo tipo zero. Verifica o formato RGB565 padrão, negociação a partir de RAW, dimensões, limpeza de metadados e propagação de erros/etapa. Esse teste valida a correção do protocolo; não comprova imagem física do sensor.

## Wi-Fi

O log recebido da unidade mostra runtime 0.7.0, ESP-Hosted 1.4.0, Wi-Fi Remote 0.8.5 e resposta C6=1.4.1. Isso comprova a consulta ao coprocessador naquele momento, sem comprovar associação, autenticação ou DHCP. Nenhuma atualização de firmware do C6 foi executada ou embutida.

Rede → Buscar redes Wi-Fi inicia busca assíncrona na banda de 2,4 GHz. A lista mostra SSID, segurança, sinal em dBm e canal; SSIDs repetidos usam o resultado mais forte. Há até 32 SSIDs, seis por página, seleção para preencher o formulário, busca novamente, atualização de resultados, cancelamento e entrada manual para redes ocultas. Resultados da captura de tela são sintéticos: [prévia](preview-wifi-scan-0.7.1.png).

A conexão passa a configurar explicitamente SAE Hunting-and-Pecking/H2E (`WPA3_SAE_PWE_BOTH`) e PMF capaz/opcional, além de limiar WPA2 para redes com senha. A configuração anterior zerava esses campos; isso podia impedir autenticação em redes que exigem métodos SAE mais recentes. São configurações suportadas pelo transporte fixado e pela [documentação de segurança da Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c6/api-guides/wifi-security.html). A confirmação no roteador do usuário continua pendente. O Tab5 precisa de um SSID disponível em 2,4 GHz, independentemente de o roteador ser anunciado como Wi-Fi 7.

A mensagem genérica de C6/credenciais foi substituída pelo erro real do backend. O novo `logs/network.txt` registra etapa e código do SDK, ou motivo da desconexão/handshake e distinção de timeout DHCP. Não grava senha nem SSID. Falhas de scan, cancelamento e timeout também são diferenciados.

## Apps Lua e memória

Apps e módulos do SD são carregados como texto (`luaL_loadbufferx` em modo `t`). O Lua compila esse texto para bytecode em RAM, executado pela VM; não gera ELF/código de máquina nem grava um binário de aplicativo. Cada app tem seu estado Lua, e VMs inativas podem ser descarregadas conforme a política LRU.

O valor mostrado em Abertos vem do alocador da VM: inclui objetos/bytecode Lua, mas não atribui ao app todos os buffers de GUI, fontes, imagens, codecs, pilhas e drivers nativos compartilhados. Por isso alguns apps aparecem com poucos KiB, enquanto o sistema usa mais memória global. O áudio é lido por streaming: o arquivo de 133 MB não precisa caber inteiro na RAM.

## Evidência física relatada pelo usuário

Em 8 de novembro de 2026, o usuário relatou duas músicas reproduzidas corretamente, funcionamento de um set de DJ de 133 MB, volume alto a 100% nos fones e negociação QC chegando a 12 W. O gravador já havia sido confirmado. São relatos físicos do usuário, sem medição instrumentada independente pelo agente. Câmera ainda apresentava EINVAL e a conexão Wi-Fi falhou; suas correções nesta versão precisam de novo ensaio físico.

## Atualização

Atualize BIN e SD 0.7.1 juntos, preservando os documentos. A API 7 impede que o novo aplicativo de busca seja carregado por um residente sem suporte à operação. Testes de host/sanitizadores e build ESP32-P4 são separados em [BUILD](BUILD.md). Nenhum flash, monitor ou serial foi executado.
