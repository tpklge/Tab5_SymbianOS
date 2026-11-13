# Interface e resposta — 0.13.0 / API 13

## Aparência

Ícones dos aplicativos lidos da pasta icons fornecida pelo usuário, conforme nomes/variantes. A compilação usa os PNGs originais para produzir RGB565+alpha no SD; não gera novos desenhos em seu lugar. Mapeamento e SHA-256 ficam em icon-sources.json; USER-ICONS.txt preserva a distinção entre assets fornecidos e código/controles originais MIT. O atlas possui 75 entradas e permanece abaixo de 2 MiB. Nenhum bitmap das fotografias de referência foi copiado para o sistema.

Interface inspirada nas referências S60: fundos escuros, seleção ciano, ícones destacados, listas com divisões claras e barra superior discreta. Paisagem 1280×720 preservada. Home reúne relógio analógico/digital, data/perfil, agenda, notificações e seis atalhos (três configuráveis). Papéis de parede dos três temas permanecem na Home; os demais apps usam superfícies escuras. Cores de temas instalados continuam respeitadas. Histórico Meshtastic tem balões com remetente, horário e estado, além de acesso a Escrever. O layout é próprio, sem prometer reprodução exata dos aparelhos das referências.

Menu mantém grade de quatro colunas e lista de duas. Anterior, Próxima, Pastas, Lista/Grade e Organizar formam um grupo centralizado de cinco botões de 168×54, com intervalo de 8 px e borda inferior livre de 10 px. Não há botão Voltar duplicado nessa área; Início continua na barra superior. Outros aplicativos preservam seus comandos inferiores. Ícones agora possuem dimensões reais no layout, evitando que imagens pequenas criem área rolável invisível de 96 px.

## Demora, desbloqueio e círculo

A implementação anterior recalculava toda a árvore do cartão antes e depois de cada escrita atômica; append também recontava periodicamente. Os registros de ação e as gravações de estado multiplicavam essas varreduras. As mutações continuam serializadas, com reserva de pico, fsync, renomeação recuperável e verificação de espaço físico. A quota agora atualiza o saldo por diferença de tamanho nas escritas/remoções/appends. Montagem/recuperação e operações que exigem auditoria continuam reconciliando a árvore completa. Alterações externas no cartão exigem remontagem, como previsto no uso offline do SD.

Não houve redução das 4096 rodadas do verificador PIN, alteração do T5PIN1 nem remoção da limitação de tentativas. O desbloqueio correto sem falhas anteriores evita regravar um contador que já é zero; depois de falhas, o zero é persistido antes de liberar. O hash reserva seu buffer hexadecimal, e preferências/estado estáveis deixam de ser regravados a cada refresh. A contagem de quota e a recuperação foram exercitadas em host. O tempo real no cartão do usuário ainda precisa ser medido.

O círculo só aparece em operações iniciadas pelo usuário com duração acima de 150 ms. O tick automático dos serviços e a atualização de relógio/estado não o acionam. A entrada rápida de PIN por teclado físico/touch da 0.12.1 continua preservada.

## RAK e RSS

Conectar no Meshtastic agora entrega o comando ao serviço na própria ação e aguarda sua execução, com a permissão mesh verificada. Outros eventos sys.send continuam assíncronos. A interface mostra o estado real retornado, não apenas uma confirmação de fila. Uma sessão Lua reiniciada pode adotar um transporte nativo ainda aberto. O diagnóstico inclui runtime=0.13.0, modo, pedidos UART, estado e TX/RX/quadros/perdas. UART aberta ainda não equivale a rádio sincronizado; é necessário receber config_complete_id correspondente. Pinagem, alimentação e firmware do RAK não foram alterados. RX contínuo/fila PSRAM da 0.12.1 permanecem.

RSS recebe até 64 KiB em RAM antes de gravar o cache, sem append/fsync/varredura do SD entre fragmentos TLS. Resposta truncada é descartada; em interrupção HTTP/TLS há uma única nova tentativa de GET, reiniciando o buffer para não duplicar o prefixo. O cache anterior só é substituído após corpo completo e validação RSS/Atom. Cancelar fica disponível durante a recepção. Certificados, CAs, redirects seguros, limite de tamanho e checagem chunked permanecem. A correção trata bloqueio da recepção por I/O do cartão; a falha específica de 1363 bytes requer confirmação física no C6/P4.

## Validação e limites

16 suites em host e ASan/UBSan, jornada LVGL/SDL e build ESP32-P4. Testes cobrem saldo de quota na criação/substituição/remoção, concorrência/reserva/recuperação, resposta imediata do comando RAK, histórico em chat, recepção RSS sem arquivo parcial, tentativa reiniciada sem duplicação, PIN rápido e indicador oculto no tick automático. Prévia de mensagens e bateria é sintética; não demonstra rádio, SD ou medições físicas.

Atualizar primeiro o BIN compatível e o SD da mesma versão: a release exige API 13 para impedir combinação com residentes antigos. OTA própria permanece pendente de aceitação. Nenhum flash, monitor, serial ou GitHub executado. dist mantém automaticamente apenas os ZIPs das três versões mais recentes.
