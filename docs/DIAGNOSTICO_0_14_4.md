# Diagnóstico sem reconstrução — 0.14.4

O usuário relatou travadas e flashes azuis ao abrir/atualizar o diagnóstico na 0.14.3, sem reinício do Tab5. Informou modo não licenciado, região 6 e roteamento 6. Esses dois códigos pertencem a enums diferentes: região 6 é ANZ; roteamento 6 é NO_CHANNEL. Não alteramos região nem modo licenciado.

## Alterações

Na atualização automática, uma vista reader somente leitura cujo único conteúdo alterado é text atualiza a textarea existente em vez de destruir a árvore LVGL inteira. Cursor e rolagem são restaurados. Se outros componentes da vista mudarem, o caminho completo de renderização continua sendo usado. A comparação ocorre no worker; atualização LVGL ocorre após sua conclusão, na thread da interface.

O relatório Meshtastic começa por linhas separadas de código, origem, destino, presença das chaves, modo licenciado e região, seguida do motivo detalhado. A região ANZ aparece com nome e código. O estado original vindo do rádio é preservado.

Salvar diagnóstico exporta o relatório exibido para /SymbianOS/data/mesh/diagnostic.txt. O arquivo não contém valores de chaves privadas. O app já possui permissão para /data/mesh; não há transmissão externa nem mudança de permissões. O botão permite copiar o arquivo pelo fluxo de arquivos/OTA existente em vez de transcrever a tela.

## Validação e limites

Teste LVGL força três mudanças consecutivas nos contadores e verifica que tela e textarea mantêm os mesmos objetos; testa também o botão de exportação. Host verifica exportação com origem e sem valor da chave privada. Mantidas regressões de 128 nós/histórico cheio e classificação de falhas Lua.

Build ESP32-P4, host e sanitizadores separados. A eliminação física dos flashes não foi medida: a reconstrução periódica está comprovada no código anterior, mas não há prova de que explique todos os flashes/travadas relatados.

O envio privado com erro 6 ainda não foi corrigido nem diagnosticado por completo. Precisamos do código com origem local/remota e dados de disponibilidade das chaves; o texto sem origem não distingue falha local, recusa de criptografia no destinatário ou estado de chaves. Esta versão não altera chaves, algoritmo do RAK, canal RF ou firmware do rádio.

Atualizar BIN e SD juntos, release=0.14.4. Se ocorrer outra tela azul, guardar também /SymbianOS/logs/lua-fault.txt, quando existir. Nenhum flash, monitor, serial ou publicação no GitHub.
