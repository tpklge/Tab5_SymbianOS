# Validação — residente 0.7.3 / SD 0.7.1 / API 7

Build ESP32-P4 com ESP-IDF 5.4.4 concluído. Aplicação: **2003744 bytes**, dentro do slot de 2 MiB. DIRAM estática atribuída: 118636 bytes; não mede pico dinâmico nem pilha física. O servidor HTTP usa agora pilha de 32 KiB; o buffer do upload é uma alocação limitada de 4 KiB no heap.

Sete suites CTest passaram no host, com jornada gráfica LVGL/SDL. ASan/UBSan passou nas suites sem SDL. Novos casos cobrem streaming acima de 1 MiB, progresso por etapa/contagem exata, retorno de leitura excedendo o buffer e limpeza de parcial. Mantêm-se testes de cancelamento/interrupção, quota, destino existente e concorrência. O caminho HTTP/SD físico e sua pilha não são executados no host.

Logs: [host](host-validation-0.7.3.log), [sanitizadores](sanitizer-0.7.3.log), [ESP32-P4](esp32p4-build-0.7.3.log). Tamanhos: [resumo](size-summary-0.7.3.json), [componentes](size-components-0.7.3.json), [arquivos](size-files-0.7.3.json). [Correção de upload e limites](HOTFIX_0_7_3.md), [validação anterior](BUILD_0_7_2.md).

Os hashes de distribuição e a identidade do BIN com a imagem compilada foram conferidos. O SD permanece 0.7.1: para instalações já nessa versão, atualize somente o BIN.

A causa física do reinício não foi comprovada por backtrace. O usuário relatou tela azul alguns segundos após iniciar o upload na página; o log do C6 não indica o ponto da falha. A correção trata um risco concreto de pilha e adiciona logs/last-upload.txt para registrar etapa, bytes e folga de pilha/memória. É necessário retestar na unidade. Nenhum flash, monitor ou serial foi executado.
