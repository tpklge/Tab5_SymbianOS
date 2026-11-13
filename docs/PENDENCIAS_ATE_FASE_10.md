# Pendências até a fase 10 — revisão após 0.17.0

Atualização: favoritos, tons, EN/ES, anexos e exclusões persistentes foram implementados na 0.17.0. Instrumentação e ensaios de host cobrem as fases 4/6/7, permanecendo a bancada física. [Detalhes](FASES_3_4_6_7_8_10_0_17.md).

Revisão histórica do plano normativo (seções 5, 11, 12 e 16), apps da release 0.16.0, backends nativos e relatos do usuário. “Implementado” não significa que todos os critérios de bancada da fase estejam encerrados. O usuário já confirmou player, gravador/câmera em versões anteriores, tráfego público RAK e agora recebimento de e-mail. Esses resultados não são apagados por registros históricos de “pendente”.

| Fase | O que falta |
| --- | --- |
| 0 — base | Não encontrei entrega central ausente. Falta concluir o checklist físico de cartão ausente/somente leitura, quota de 4 GB, RTC e teclado oficial, incluindo desconexão/reconexão. Há testes de host para as políticas. |
| 1 — ambiente SD | Base implementada. Consolidar jornada física completa de teclado/modificadores/acentos, navegação e substituição de app/tema só pelo SD, com registro de versão. |
| 2 — multitarefa | Base implementada. Ensaio prolongado com três apps, áudio/rede, falhas de script, suspensão/restauração de estado e medição de crescimento de memória/latência. |
| 3 — organizador/arquivos | Favoritos e seleção de três tons entregues na 0.17.0. Soneca já existe, e não é pendência. Validar backup/restauração e concorrência com cartão cheio/retirado. |
| 4 — multimídia | Recursos principais presentes e vários confirmados pelo usuário. Falta fechar medições físicas de áudio+editor+alarme/retomada, fim de espaço durante gravação e orçamento de câmera/miniaturas. Vídeo já existe em MJPEG, mas falta benchmark próprio de taxa/estabilidade/tamanho. |
| 5 — instalador/SDK | Não encontrei entrega central ausente. Falta aceitação física de instalação/atualização/remoção com cartão cheio, retirada de energia e recuperação da transação, preservando documentos. |
| 6 — rede/RSS | Wi-Fi, upload/download e RSS implementados; recebimento de e-mail confirmou tráfego após correção SDIO. Falta validar o novo HTML/imagens no Tab5 e um ensaio longo de quedas, cancelamentos, upload simultâneo e quota. Limite de upload de 64 MiB é intencional, não pendência. |
| 7 — energia/atualizações | OTA interna, atualização SD, rollback e retomada existem. Ainda falta a aceitação física combinada de OTA/rollback/interrupção de atualização e SD removido, mais medições de consumo/autonomia, suspensão/retomada e soak test. QC já foi confirmado pelo usuário; não está sendo listado como faltante. |
| 8 — experiência | Home, Menu 4/2 colunas, temas, PIN, Ajustes/sliders, agenda, playlists e zoom presentes. Português, inglês e espanhol entregues na 0.17.0. Fechar a revisão física de responsividade e PIN/teclado com apps/rede ativos e do novo leitor visual. |
| 9 — Meshtastic | Resolver privadas RAK/Heltec com NO_CHANNEL/channel-key mismatch, explicitamente pausadas pelo usuário. Validar BLE real (busca, conexão, reconexão, estados e coexistência com Wi-Fi), ACK/retentativas/deduplicação em campo. Tráfego público RAK já funciona. Paridade adicional com o cliente dedicado (URL/QR ChannelSet e parâmetros LoRa) ainda não está implementada e deve ter incremento próprio. |
| 10 — e-mail | Recebimento já confirmado. Anexos com limites e escrita/envio em blocos, exclusões por conta/UID e restauração de pasta entregues. Host cobre interrupções, mudança de UIDVALIDITY, autenticação recusada e falta de espaço; falta confirmar esses casos no provedor/SD reais. Falta confirmação física de envio SMTP/STARTTLS/POP3 e da nova visualização. Enviados/flags/lixeira são locais: espelhamento IMAP completo, APPEND em Enviados e sincronização remota de flags seriam ampliação explícita. |

## Ordem prática sugerida

1. Fechar e-mail: confirmar SMTP e visualização; em seguida anexos e regras de sincronização/exclusão.
2. Validar OTA/rollback/recuperação e energia em uma cópia de cartão com dados de teste.
3. Retomar privadas Meshtastic e validar BLE quando o autor voltar a essa frente.
4. Confirmar favoritos, tons, idiomas e os ensaios prolongados de desempenho/memória na placa.

OAuth permanece adiado a pedido do usuário; não é bloqueio desta entrega. Múltiplas contas simultâneas também não fazem parte do primeiro perfil de e-mail implementado. SIS/SISX/JAR pertencem à fase 11; consolidação ampla é fase 12. Mapas/GNSS, modem/SMS e rádio FM da matriz são extensões opcionais e não foram reclassificados como requisitos obrigatórios até a fase 10. Não implementados nesta revisão: ela lista pendências, não autoriza automaticamente ampliar essas frentes.
