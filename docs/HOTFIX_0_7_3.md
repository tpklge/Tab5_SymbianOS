# Residente 0.7.3 — upload HTTP no serviço de arquivos

O usuário relatou tela azul/reinício alguns segundos após clicar em Enviar na página de transferência. O log recebido confirma consulta ao C6 1.4.1, mas não é um backtrace nem comprova a causa do reset.

A revisão encontrou um risco de pilha no caminho de upload: o servidor HTTP tinha 8 KiB e `Network::receive` mantinha um buffer de 4 KiB em sua pilha, além de executar as camadas HTTP/TCP e chamadas do SD, incluindo reconciliação recursiva da quota. A correção move esse buffer para uma alocação limitada no heap, com falha tratada quando indisponível, e amplia a pilha HTTP para 32 KiB.

A leitura é limitada ao espaço realmente fornecido ao callback, recusando também uma resposta maior que o bloco solicitado antes de construir/copiar o conteúdo. O recebimento continua por streaming, com buffer de 4 KiB e limite de 64 MiB por arquivo; não carrega o arquivo inteiro na RAM. Destinos existentes, confinamento, escrita temporária, renomeação ao concluir e quota compartilhada permanecem.

Timeout curto do socket passa a permitir até cinco tentativas de leitura, com timeout de dois segundos por tentativa; fechamento de conexão/abort continua encerrando e removendo o parcial. Não há repetição infinita.

## Diagnóstico

O novo `/SymbianOS/logs/last-upload.txt` registra as etapas `checking`, `receiving`, `committing`, `complete` ou o tipo de falha, bytes recebidos/esperados, último retorno do socket, folga mínima da pilha HTTP e memória interna disponível. Registra progresso a cada 1 MiB e não inclui conteúdo, nome do arquivo, SSID, senha ou token. A gravação é de melhor esforço e usa o mesmo Storage/quota. Após eventual novo reinício, preserve também `boot.txt`/`startup.txt`.

## Validação

Testes de host cobrem upload maior que 1 MiB, payload e progresso exatos, callback excedendo o buffer, interrupção, limpeza de parcial, destino existente, uploads concorrentes e quota. Sete suites CTest, jornada gráfica LVGL/SDL e ASan/UBSan passaram. Build ESP32-P4 e tamanho são verificados separadamente. Esses testes não reproduzem a pilha/TCP/SD físicos da unidade, e não comprovam que a tela azul relatada teve exatamente essa causa.

O residente 0.7.3 permanece compatível com SD 0.7.1/API 7: atualize somente o BIN pelo launcher se o cartão já está nessa versão. O serviço é transferência HTTP de arquivos para `/home/Transfer`, não gravação OTA de firmware. Não foi executado flash, monitor ou acesso serial pelo agente.
