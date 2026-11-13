# Estabilidade Meshtastic — 0.14.3

O usuário relatou telas azuis/reinício da aplicação e retorno 6 NO_CHANNEL após o erro de chave pública desconhecida. O texto fornecido ainda não identifica origem local/remota; não é suficiente para atribuir uma causa de criptografia.

## Falha reproduzida em host

Um teste com banco completo (128 nós), recebendo 18 quadros NodeInfo em lotes, falhou na 0.14.2: `services tick: [string "mesh_core"]:8: instruction budget exhausted`. Cada quadro relia e regravava todo o banco em Lua. Também foi exercitado o recebimento de seis mensagens de 228 bytes com histórico cheio (80 registros) e banco de nós completo.

Isso comprova um defeito na execução do serviço; não prova que ele explica os reinícios físicos relatados. A classificação de erros agora persistida ajudará a distinguir essa falha de outro problema nativo.

## Correção

- Cache limitado aos 128 nós e 80 mensagens, pertencente ao serviço que já é o escritor desses bancos.
- Atualizações do lote são agrupadas. No máximo um banco é serializado por invocação, alternando nós/histórico quando ambos estão pendentes. Carregamento inicial de cada banco também ocorre em ciclos separados.
- O limite global de instruções Lua não foi aumentado nem desativado. A tarefa UART nativa continua recebendo para a fila PSRAM; os lotes continuam limitados a seis quadros por ciclo.
- Escritas pendentes permanecem marcadas até sucesso e são retomadas nos ciclos seguintes, inclusive offline. A conexão não é ignorada quando existe uma segunda gravação pendente. Dados alterados podem aguardar ciclos adicionais antes de aparecerem na leitura do arquivo pela UI; não há promessa de persistência imediata de cada quadro.
- Diagnóstico abre com último retorno (código e origem) e destino/canal solicitado. Isso evita ter de procurar o trecho no fim do relatório.
- Falha Lua gera `/SymbianOS/logs/lua-fault.txt` com runtime, app, classe, código Lua e bytes Lua. Não grava argumento, conteúdo da mensagem, texto bruto da exceção ou valores de chaves. As classes são instruction-budget, lua-memory ou lua-error. É um registro da última falha Lua, não um capturador de panic nativo, e permanece para inspeção após reiniciar.

## Validação

16 suítes host e 16 ASan/UBSan passaram, além da jornada LVGL. Testes finais Meshtastic repetidos em host e sanitizador após ajuste de reconexão. Cobertura inclui burst de NodeInfo com 128 nós, histórico de 80 mensagens, seis textos máximos por lote, renderização da última página sob carga e preservação do rascunho. A persistência diferida é verificada após dois ciclos cooperativos; cada invocação mantém seu próprio limite de instruções.

Build ESP32-P4 separado, sem acesso a hardware. Não houve flash, monitor, serial ou publicação no GitHub.

## Pendente no Tab5

Confirmar estabilidade física e envio privado. Esta versão não modifica as chaves, criptografia automática do RAK ou firmware do rádio e não anuncia correção do erro 6. Se o retorno persistir, copiar as primeiras linhas do Diagnóstico (ÚLTIMO RETORNO e DESTINO). Se houver outra tela azul, consultar lua-fault.txt junto dos registros de inicialização; ausência desse arquivo não exclui falha nativa.

Atualizar BIN e SD juntos; manifesto release=0.14.3.
