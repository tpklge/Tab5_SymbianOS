# Home, processamento e Meshtastic — 0.12.0 / API 12

## Interface

Home com relógio/data atualizados, próximos dois compromissos e duas notificações recentes. **Início → Atalhos** configura três aplicativos e salva em /data/shell/home.tsv. Agenda, notificações e Ajustes têm acesso fixo. Abertos separa nome e estado/memória Lua em linhas de 76 px. Processamento usa spinner piscante junto à bateria: tocar nele ou Esc pede cancelamento antes do commit; o aviso textual e a troca do botão do rodapé por Cancelar foram removidos.

## Meshtastic

Dois aplicativos no Menu: **Meshtastic BLE** e **Meshtastic RAK**, com um transporte ativo por vez. Ambos compartilham histórico, nós, canais e caixa de saída em /data/mesh/. Até 32 mensagens pendentes, 80 registros de histórico, 128 nós, oito canais e 228 bytes UTF-8 por texto. Não modifica região, frequência ou chaves do rádio. Não é SMS. Não instala firmware no RAK ou C6.

RAK: Conectar mostra a ligação antes de abrir UART1 a 115200/8N1: TX GPIO53 para RX PB7, RX GPIO54 para TX PB6 e GND comum. Requer o firmware Meshtastic PROTO utilizado pelo projeto de referência, não firmware AT/LoRaWAN comum. Nenhum comando é enviado a uma porta serial durante o desenvolvimento. O runtime só abre a UART quando solicitado pelo app ou para reconectar uma conexão salva. Não habilita alimentação externa: 5 V só na entrada de uma placa adaptadora compatível; nunca no módulo RAK nu, de lógica 3,3 V.

BLE: Buscar rádios filtra o serviço Meshtastic. Selecione um rádio e forneça seu PIN de seis dígitos. O endereço também pode ser informado como MAC,tipo (0 público / 1 aleatório). Usa NimBLE central sobre o C6 e leitura periódica FromRadio, sem depender da passagem de notificações pelo túnel. Bonds ficam no NVS; o PIN informado não é salvo no SD. Reconexão usa o endereço salvo e o bond; se o bond não existir mais, pareie novamente. Exige firmware C6 com transporte HCI compatível; a compilação P4 não valida essa capacidade no firmware atualmente instalado.

Mensagens podem ser compostas offline. O destino 4294967295 é broadcast; nós individuais aceitam número decimal ou !hex. Sincronização precisa terminar antes de liberar a saída: até 20 segundos sem progresso, 300 segundos totais e três tentativas. Receber nós mantém a janela viva durante bases maiores. Escritas são espaçadas por três segundos; falhas locais têm até três tentativas. O histórico distingue aceitação pelo transporte, resposta de fila do rádio, eco de envio e confirmação de roteamento. Broadcast não garante ACK. Ausência de confirmação não é convertida em entrega. IDs recebidos são deduplicados por remetente/ID. A notificação respeita o perfil silencioso; offline desconecta o rádio. Suspender encerra o transporte e permite reconexão após retorno. Histórico e fila continuam no cartão.

## RSS

A foto corresponde a EAGAIN (-0x7007), com 1.363 bytes e resposta chunked. A janela sem progresso passou de 12 para 30 segundos; leituras menores (1 KiB) evitam esperar preencher 4 KiB antes de devolver dados, com timeout de socket de três segundos. Continua exigindo corpo/chunks completos e fechamento RSS/Atom. Isso trata a espera observada, mas ainda requer reprodução física para confirmar a causa no C6/P4. No computador, https://gizmodo.com.br/feed redirecionou para www e retornou HTTP 200/RSS completo de 25.143 bytes. A raiz do site sem /feed é uma página HTML, não uma assinatura RSS válida. O conteúdo externo coletado não é distribuído no SD.

## Implementação e referências

Protocolo e regras escritos originalmente em Lua no SD; firmware contém transportes de bytes limitados a 512 bytes e toolkit genérico. Nenhum código do repositório GPL de referência foi incorporado. Consulta de pinagem/protocolo: [Tab5-Meshtastic, commit edafd397](https://github.com/tpklge/Tab5-Meshtastic/tree/edafd39750a9a8bc3037d77bd11f26c097b0e11b), [Client API oficial](https://meshtastic.org/docs/development/device/client-api/), [definições públicas de mensagens](https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto). NimBLE permanece sob as licenças próprias do ESP-IDF, inventariadas na distribuição.

Para manter o slot original de 2 MiB, parsers simples de linhas deixaram de depender de streams C++/locale e main/platform/Lua/LVGL usam LTO com ISA explícita. Partições, bundle de CAs e TLS cliente foram preservados. Mudança do otimizador precisa de aceitação física geral, além dos testes de host.

## Aceitação no Tab5

Testar dois rádios reais, PIN errado/correto, perda de alcance, reconexão, UART com ruído, mensagem broadcast e direta, confirmação/recusa, Wi-Fi simultâneo, suspensão e fluxo contínuo de recepção. Ainda não houve medição física. OTA própria continua pendente; BIN e SD API 12 precisam ser atualizados juntos. E-mail offline e SIS/SISX/JAR permanecem planejados.
