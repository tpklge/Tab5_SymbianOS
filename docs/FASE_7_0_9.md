# 0.9.0 / API 9 — rede, energia e recuperação

Há somente uma fase restante no planejamento: fase 7. Esta versão implementa seus fluxos de energia, atualização e recuperação. A aceitação física de consumo, retomada, OTA, corte de energia e uso prolongado permanece pendente. Não houve flash, monitor nem acesso serial nesta entrega.

## Upload de 30 MB

O caminho anterior sincronizava o FAT a cada leitura de até 4 KiB. Isso aumenta operações de metadados e torna transferências longas vulneráveis a pausas. O serviço agora acumula até 64 KiB antes de sincronizar. A página envia blocos de 64 KiB, aguarda confirmação, mostra porcentagem e repete até três vezes em falhas transitórias. Um bloco incompleto não é gravado; repetição do último bloco confere SHA-256 e não duplica bytes, inclusive quando a confirmação final se perde. O arquivo só aparece em `/home/Transfer` após conclusão.

O limite permanece **64 MiB = 67.108.864 bytes**, verificado antes do envio e no servidor. O arquivo temporário `/tmp/web-upload.part` conta na quota. Só há uma sessão ativa, com identificador aleatório; uma nova pode substituir sessão inativa há dez minutos. Encerrar o serviço libera a sessão e remove o parcial. Depois de cancelar ou recarregar a página, encerre e reabra o serviço no Tab5 para recomeçar imediatamente. Uma reinicialização exige reiniciar o envio; não há retomada persistente após reboot.

O servidor tem prazo de 15 segundos por leitura, cabeçalhos até 2048 bytes e quatro sockets com expulsão de conexões ociosas. As requisições de blocos encerram a conexão depois da resposta para evitar retenção dos poucos sockets por keep-alive. Atualize BIN e SD e **recarregue a página web**; a página anterior envia o arquivo em uma única requisição.

Testes usam o método C++ real para 30 MiB com leituras curtas, interrupção, repetição e integridade byte a byte. Outro teste executa o JavaScript real da página com rede simulada, incluindo HTTP 408 e confirmação perdida. Isso não mede velocidade do SD nem estabilidade do rádio físico. Diagnóstico: `/logs/last-upload.txt`.

## HTTP/TLS dos feeds

`ESP_ERR_HTTP_CONNECT` sozinho não diferencia handshake, certificado, DNS e falta de memória. O cliente agora usa PSRAM para mbedTLS, preservando SRAM interna para rádio/UI, conjunto completo de autoridades certificadoras, IPv4 e 20 segundos para conexão. Uma falha de conexão sem dados entregues pode ser repetida uma vez. Não há repetição depois de começar a gravar a resposta. Os limites e proteções de redirecionamento da 0.8.1 continuam ativos.

O erro passa a registrar código TLS, flags de certificado e maior bloco interno livre em `/logs/network.txt`. Isso permite distinguir causas se a falha persistir na placa. Não foi desligada a verificação de cadeia ou hostname. A configuração de validação temporal permanece a da base anterior; não foi adicionado sincronismo de relógio pela rede. TLS é somente cliente; a transferência local continua HTTP com token temporário.

## Energia

No app **Energia e volume**, escolha tela sempre ligada ou apagamento após 1, 2, 5, 10 ou 30 minutos. A preferência fica em `/config/public/screen_minutes`; padrão 2 minutos. Nos últimos 15 segundos o brilho cai para 20%; depois a iluminação é apagada. Música, gravação, rede e alarmes continuam ativos. Toque ou teclado restaura brilho; a primeira tecla somente desperta. A cobertura de toque impede ativar acidentalmente controles sob a tela apagada. Há também “Apagar tela agora”.

**Suspensão profunda** é manual e separada: 1, 5 ou 15 minutos, com confirmação explícita. Faz checkpoint dos demais apps, recusa gravação ativa, para áudio/câmera/rede, pausa o armazenamento, desmonta SD e programa despertar por temporizador. A retomada é um novo boot que revalida o cartão/manifesto e recupera estados persistidos. Não há despertar por touch/teclado nem garantia de alarme no meio da suspensão; serviços verificam compromissos ao retornar. Não usar esse modo para depender de um alarme até a bancada física. Se desmontagem falhar, a suspensão é cancelada.

`/logs/sleep.txt` registra a duração solicitada. `/logs/health.txt`, atualizado aproximadamente por minuto, registra uptime, causa de despertar, RAM interna livre/maior bloco, PSRAM livre e tensão/corrente da bateria. Valores só existem após execução física; não são resultados desta entrega. O monitor mede corrente da bateria, não um ensaio completo de potência da fonte.

## Atualizações do SD

A atualização inicial para 0.9.0 continua pelo launcher: BIN e pacote `SymbianOS-update-0.9.0.zip` juntos. Preserve documentos e configurações. O novo runtime aceita APIs 1–9; a release 0.9.0 exige API 9. Uma primeira migração de runtime anterior ainda não tem histórico confirmado: mantenha backup externo.

Depois, o kit `SymbianOS-release-0.9.0.zip` extrai a release em `/system/releases/0.9.0/` e o manifesto em `/update/manifest.txt`. No app **Atualizações**, “Preparar release do SD” verifica arquivos/hashes/API, reserva espaço para os metadados e registra `/boot/pending.txt`. Reinicie para testar. Shell e serviços essenciais precisam iniciar; rede opcional não faz parte desse autoteste.

`/boot/attempt.txt` marca uma tentativa não confirmada. Falha do shell devolve a release confirmada no mesmo boot; reinício durante a tentativa recupera a confirmada no boot seguinte. `/boot/confirmed.txt` e `/boot/previous.txt` guardam os manifestos de confirmação e anterior. “Restaurar release anterior” e o botão residente “Release anterior” oferecem recuperação manual. Documentos/configurações não são revertidos. FAT não garante sobrevivência a qualquer corte de energia: o journal e a auditoria reduzem o risco, mas precisam do ensaio no cartão de teste.

Ferramenta offline, sem serial ou flash:

```sh
python3 tools/recover_sd.py --root /caminho/do/cartao/SymbianOS
python3 tools/recover_sd.py --root /caminho/do/cartao/SymbianOS --release 0.9.0
# Release antiga sem campo API no catálogo: forneça a API documentada.
python3 tools/recover_sd.py --root /caminho/do/cartao/SymbianOS --release 0.8.1 --api 8
```

A ferramenta cria `boot/recovery-backup.txt` e substitui apenas o manifesto. A pasta deve estar offline. Não formata nem apaga documentos. A API selecionada ainda precisa ser suportada pelo runtime.

## OTA do runtime pelo SD

Extraia `SymbianOS-runtime-OTA-0.9.0.zip` no cartão: `/update/runtime.bin` e `/update/runtime.sha256`. No app Atualizações, “Revisar runtime” valida SHA-256, magic/chip ESP32-P4, identidade do projeto e capacidade do slot **da tabela real do dispositivo**. “Instalar runtime revisado” pede confirmação e revalida a imagem. A gravação usa `esp_ota_*` no slot inativo, verifica os bytes escritos e só altera a seleção de boot depois de `esp_ota_end`. Interrupção/cancelamento anterior à seleção preserva o slot ativo. A revisão não autentica procedência; use artefatos confiáveis.

Encerre transferência e pare áudio antes de atualizar. Após sucesso, escolha Reiniciar. O app salva checkpoints antes do reboot. O novo runtime confirma uma imagem pendente após pelo menos 15 segundos com UI, cartão e shell prontos. Se o autoteste não ficar saudável, solicita rollback.

**Rollback nativo automático depende do bootloader instalado suportar os estados OTA.** O bootloader standalone compilado aqui habilita essa capacidade; enviar somente BIN pelo launcher não atualiza o bootloader existente. Não declare rollback automático validado no launcher do usuário. “Runtime anterior” aceita somente outro slot marcado VALID e com identidade deste projeto; sem slot confirmado, a recuperação continua pelo launcher. Nenhuma tabela de partições foi alterada. Sleep/OTA usam implementações nativas compiladas, mas não foram executados no hardware pelo agente.

## Evidência e aceitação

Host: suites de políticas/armazenamento, 30 MiB íntegros, protocolo JS, fallback de boot interrompido e falha Lua, preservação de documento, preferências, confirmação/cancelamento e 100 ciclos de navegação/salvamento. LVGL/SDL: reduzir/apagar tela e despertar por toque/teclado. Sanitizadores cobrem o núcleo sem SDL. Build ESP32-P4 e tamanhos estão em BUILD.md.

Ainda medir no Tab5: feed HTTPS real e redirecionado; upload 30 MB com telefone/computador, cartão e áudio concorrente; corrente nos três estados de tela; retomada de deep sleep e RTC; OTA interrompida em cartão de teste; histórico do bootloader do launcher; remoção/reinserção; quota excedida e horas de uso com amostras de health.txt. Nenhum desses critérios físicos é substituído pelo simulador.
