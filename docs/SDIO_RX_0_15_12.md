# Correção da recepção SDIO — 0.15.12

## Causa encontrada no projeto

O projeto fixava `CONFIG_ESP_HOSTED_SDIO_OPTIMIZATION_RX_NONE=y` desde a integração de rede. Isso seleciona recepção de um pacote por leitura. O driver ESP-Hosted 1.4.0 rejeita um comprimento pendente maior que 1536 bytes nesse ramo. O próprio Kconfig do host tem streaming como padrão, e o Kconfig do C6 também habilita envio em stream por padrão.

O firmware oficial C6 1.4.1 do Tab5 contém a mensagem compilada `sending mode: SDIO_SLAVE_SEND_STREAM`, sem a variante de envio em pacotes. A imagem foi baixada apenas para inspeção, não gravada nem incorporada à distribuição:

- [Imagem oficial M5Stack](https://github.com/m5stack/M5Tab5-UserDemo/blob/main/platforms/tab5/wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin)
- 1.144.432 bytes; SHA-256 `1173bf86ab86c8dbd3f9863d10b15c2e46f57000b2b77c803e2b79bd0a3fa38e`.
- Documentação Espressif da versão instalada (`managed_components/espressif__esp_hosted/docs/sdio.md`), seção de configuração: os modos host e coprocessador precisam ser compatíveis.

A versão de firmware relatada pelo usuário é 1.4.1. Não foi lida a flash da placa individual para comparar hashes.

## Por que os sintomas variavam

No driver instalado, a tarefa limpa a interrupção de novo pacote antes de consultar o comprimento pendente. Se o ramo de pacote rejeita um lote agregado, não lê os bytes nem avança o contador consumido. O acúmulo continua maior que o limite: quadros de dados e respostas RPC que compartilham o SDIO deixam de ser entregues. Isso é consistente com aceitar um primeiro quadro (como os 1363 bytes do RSS), depois perder TLS, DNS, OTA e até respostas de disconnect/stop/deinit. Reiniciar com a mesma configuração permite que a incompatibilidade ocorra novamente.

O mecanismo de rejeição foi reproduzido em host. A associação com todas as falhas observadas na placa é fortemente sustentada pela configuração e pelo firmware oficial, mas a estabilidade física da correção ainda não foi medida.

## Alteração

A recepção usa `CONFIG_ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE=y`, tanto nos defaults versionados quanto no sdkconfig efetivo do build. O parser streaming já presente no ESP-Hosted extrai cada quadro do lote, em vez de tratar o lote inteiro como um único pacote. Nenhuma atualização do C6, mudança de senha, servidor ou certificado é necessária para esta correção.

O build nativo falha explicitamente caso o modo streaming não esteja ativo. A geração dos pacotes também confere o sdkconfig.json do build. O diagnóstico registra `SDIO RX=streaming (compatível com C6 1.4.1)`; isso identifica a configuração realmente compilada. Os tratamentos de falha das versões anteriores permanecem como proteção, sem substituir a correção do transporte.

## Regressão

`tests/sdio_receive_tests.py` extrai as funções de comprimento, validação e separação de quadros do arquivo real do ESP-Hosted instalado e as compila em fixture temporária, preservando os arquivos/licenças upstream. Apenas registradores e entrega à fila são simulados; não é teste físico de DMA/SDIO.

- Configuração antiga: primeiro quadro de 1363 bytes aceito; lote de dois quadros de dados e uma resposta de controle recusado, contador sem progresso; resposta seguinte também recusada.
- Streaming: o mesmo lote entrega os três quadros; virada do contador de 20 bits correta; 10000 lotes e quadros isolados passam.
- O teste também exige que os defaults do projeto selecionem streaming. Antes da correção, essa verificação falhou; depois passou.
- A fixture passa também com AddressSanitizer/UndefinedBehaviorSanitizer. Testes de protocolos e interface continuam separados da medição física.

Atualize BIN e SD juntos. Esta versão corrige uma incompatibilidade concreta anterior à camada de e-mail, em vez de apenas repetir reconexões ou aumentar tempos limite.
