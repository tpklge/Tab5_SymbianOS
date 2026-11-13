# Inicialização e sincronização — 0.13.1 / API 13

## Tela de carregamento

Após inicializar o display, a luz é mantida apagada enquanto a primeira tela cinza escura é desenhada. Então aparece Carregando… no centro, barra ciano e etapa atual: preparação, montagem/auditoria do cartão, verificação dos arquivos e preparação da interface. As verificações de integridade reportam arquivos efetivamente concluídos; a barra representa etapas, não previsão de tempo. Não há animação falsa de conclusão.

Essa tela usa apenas LVGL e sua fonte mínima residente; não depende de SD, Lua, wallpaper ou fontes externas. Continua em paisagem. Erros de SD/integridade seguem para recuperação; não ficam presos em Carregando. A tela é liberada depois que a interface normal assume o display. Não é uma alteração da tela do launcher antes de o runtime controlar o display.

## RAK: distinguir porta aberta de rádio pronto

O usuário informou TX=27, RX=27 e três quadros sem sincronizar. A coincidência é compatível com eco de três pedidos, mas não prova a origem elétrica: respostas com outro ID também precisam ser diferenciadas. O cliente Tab5-Meshtastic consultado usa UART2 (não UART1), TX53/RX54, 115200 8N1, configuração de pinos antes da instalação e TX com buffer. Esta versão alinha esse caminho, força modo UART normal e desliga explicitamente o loopback interno, com erros por etapa. Há descarte inicial de 200 ms de ruído de arranque. Alimentação, GPIOs, região e firmware do módulo não são alterados.

Ao abrir RAK, uma tentativa é iniciada se nenhum transporte estiver ativo; um BLE ativo não é derrubado automaticamente. Conectar continua disponível como ação explícita. A sincronização usa IDs incrementais pequenos, como o cliente de referência; isso não altera o formato protobuf. A saída offline só é liberada pelo config_complete_id correspondente e diferente de zero.

Eco exato do want_config é contado separadamente, sem ser promovido a resposta nem prolongar a espera. O diagnóstico inclui UART2/modo/loopback, pedidos TX/RX, ecos, respostas e os IDs esperado/recebido. Um timeout só com eco recebe mensagem específica. Nenhum conteúdo de mensagens, chave ou PIN é registrado nesses contadores. [Referência de interoperabilidade](https://github.com/tpklge/Tab5-Meshtastic/tree/edafd39750a9a8bc3037d77bd11f26c097b0e11b/main/transport).

## Validação

16 suites host e ASan/UBSan, incluindo teste de eco sem liberar a caixa de saída e progresso de integridade monotônico até 100%. Jornada gráfica verifica fundo cinza antes do boot Lua, PIN e navegação; prévia da tela de carga usa uma etapa sintética. Build ESP32-P4 separado. O tempo de transição/backlight e a sincronização real do RAK precisam de confirmação no Tab5; não houve flash, monitor ou abertura de serial pelo agente. Atualizar BIN e SD 0.13.1 juntos; API permanece 13. Dist mantém as três versões mais recentes.
