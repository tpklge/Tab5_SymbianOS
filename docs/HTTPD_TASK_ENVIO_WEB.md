# Envio web: ESP_ERR_HTTPD_TASK seguido de ESP_FAIL

Relato físico do usuário: erro de tarefa na primeira tentativa de ativar o envio e ESP_FAIL na segunda. A investigação usou o ESP-IDF local 5.4.4, sem abrir serial ou gravar o Tab5.

## Causa e correção

A configuração anterior pedia uma pilha HTTPD de 32 KiB com as capacidades padrão, que exigem RAM interna. Essa alocação depende de um bloco contíguo além da memória das outras tarefas e do Wi-Fi. A falha de criação de tarefa é compatível com o erro informado; ainda não foram medidos os blocos livres no hardware nessa situação.

Há também uma falha confirmada no caminho de erro do SDK local: depois de criar os sockets TCP de escuta e os dois sockets UDP de controle/mensagens, uma falha na criação da tarefa chama apenas a destruição das estruturas em memória. Os três sockets ficam abertos, e uma nova tentativa pode falhar no bind com ESP_FAIL. O teste de host reproduziu essa sequência com os trechos reais do SDK e sockets POSIX, injetando falha na criação da tarefa.

A integração agora:

- Mantém a pilha de 32 KiB, mas a aloca explicitamente em PSRAM com as capacidades suportadas pelo HTTPD/FreeRTOS. O TCB continua em RAM interna.
- Exige suporte a pilha externa na configuração, inclusive em builds limpos.
- Compila um overlay local de `httpd_main.c` que fecha os três sockets se a criação da tarefa falhar. O SDK instalado não é alterado; o arquivo gerado conserva o copyright e a licença upstream. O patch valida seu ponto de aplicação e interrompe o build se o caminho do SDK mudar.
- Só publica o handle do servidor depois de iniciar e registrar todas as rotas. Falhas de registro encerram o candidato e invalidam o token.
- Mostra nome ESP_ERR, errno, memória interna/PSRAM livre e maiores blocos se a inicialização falhar.

O envio continua na porta 8080 e continua sendo transferência de arquivos por token, sem OTA interno, NVS ou atualização de firmware.

## Validação executada

39/39 testes de host e smoke da interface passaram (`build/httpd-startup-host.log`). O novo teste compila o início do servidor do SDK e a função nativa de compartilhamento; reproduz o vazamento original e verifica a limpeza em ciclos de falha/repetição, IPv4/IPv6, semáforo opcional, falta de alocação, registro de rotas, ativação repetida e encerramento. A criação de tarefa, capacidades de heap e esgotamento são simulados: isso não é uma medição de PSRAM ou stack real no Tab5.

Build ESP32-P4 concluído (`build/httpd-startup-final-esp32p4.log`). O build usa o arquivo de integração em `build/tab5-httpd/httpd_main.c` e não compila o original em paralelo. A distribuição confere a opção de pilhas externas e recusa binário anterior às alterações de integração/configuração.

## Teste no Tab5 pendente

1. Enviar o novo BIN e a pasta SymbianOS pela WebGUI do launcher e reiniciar o Tab5. Isso limpa os sockets deixados em RAM pelas tentativas no firmware anterior.
2. Conectar o Wi-Fi dentro do SymbianOS e ativar Enviar arquivos. Conferir endereço `http://<IP>:8080` e token.
3. Abrir a página no computador, enviar um SIS pequeno e atualizar a lista do instalador.
4. Encerrar e ativar o envio novamente, repetindo pelo menos cinco vezes. O endereço/token devem acompanhar a sessão, sem ESP_ERR_HTTPD_TASK nem ESP_FAIL.
5. Enviar um pacote maior por blocos e conferir o resultado no cartão. Se houver erro, registrar a mensagem completa com os números de heap/blocos, sem apagar o cartão ou usar NVS.

Não foi executada validação física desta correção nesta tarefa.
