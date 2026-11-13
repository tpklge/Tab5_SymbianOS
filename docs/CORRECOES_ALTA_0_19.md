# Correções de nível alto — revisão local 0.19.0

Escopo: as oito falhas classificadas como altas na auditoria. Mantidos API 14, release do SD 0.19.0 e mapa de partições do launcher. O SHA-256 do BIN distingue esta compilação das anteriores.

1. Eventos entre VMs: aplicativos podem enviar para si mesmos; o serviço exporta somente comandos específicos, com verificação do remetente/capacidade. Um pacote sem permissões não pode acionar o salvamento de Notas ou comandos internos arbitrários.
2. Escrita sobre diretório: rejeitada antes de criar temporários ou renomear o destino, preservando pastas vazias e preenchidas.
3. Movimentação de diretórios: valida os caminhos e a profundidade de todos os descendentes, incluindo pastas vazias, antes da renomeação. Renomear não altera o contador de bytes.
4. Anexação: a tarefa incorpora o anexo ao rascunho atual sob mutex, preservando alterações simultâneas de corpo e assunto. Enfileirar para envio aguarda o término da operação de e-mail.
5. Upload: sessões são encerradas junto ao runtime e vinculadas à geração da montagem. Tamanho do parcial e posição são conferidos; o append valida posição/geração sob o mutex do armazenamento. Um cartão novo ou parcial truncado exige reiniciar o envio.
6. HTML: a política de alocação usa estado local à tarefa. A reserva de PSRAM do renderizador não modifica a política das tarefas de áudio/rede/interface.
7. Distribuição: aceita somente o manifesto, recursos versionados do sistema e placeholders vazios. Notas, agenda, mensagens Meshtastic, backups, logs e demais arquivos de usuário fora dessa área bloqueiam a geração. Metadados do Finder são excluídos dos pacotes.
8. BLE: persistência NimBLE em NVS desabilitada; compilação e empacotamento recusam habilitá-la. Bonds são mantidos somente em RAM. Após reiniciar, é necessário informar novamente o PIN do rádio. Dados eventualmente gravados por firmwares antigos não são apagados por esta preparação.

## Validação

As 29 verificações de host passaram, incluindo regressões dos casos de perda de dados, controle de eventos e privacidade. A implementação ESP do alocador também é compilada em um teste separado com heap simulado e duas threads. A jornada gráfica headless passou. Esses testes não equivalem a medições físicas no Tab5.

Build ESP32-P4 após a correção de inicialização: aplicação com 3.283.120 bytes, dentro dos slots OTA de 4.194.304 bytes. O aviso de tamanho da partição factory corresponde ao launcher preservado; o BIN destina-se aos slots OTA do SymbianOS. Não foi executado flash. A aceitação física do novo BIN permanece pendente.

### Captura serial autorizada após a entrega inicial

A imagem inicial desta revisão (ELF SHA-256 prefixo `f15b6c336`) apresentou `Load access fault` antes de `app_main`. O endereço `0x400539e8` corresponde ao `operator new`: a instrução `lbu a5,4(tp)` tentava ler a variável TLS com `tp=0`, durante um construtor global do litehtml (`style.cpp:1673`). A serial também registrou o bloqueio do slot pelo boot-loop guard do launcher.

Correção: o alocador verifica se o scheduler foi iniciado antes de qualquer acesso ao estado TLS e usa a alocação comum durante os construtores globais. A política por tarefa continua ativa durante a renderização. O teste do alocador inclui um construtor global anterior ao scheduler; os 29 testes e a jornada gráfica passaram novamente. O assembly RISC-V confirma o desvio antes da leitura de `tp`.

Captura em `build/serial-launcher-timeout.log`, decodificação em `build/serial-launcher-timeout-decoded.txt`. A abertura da porta provocou `CHIP_USB_UART_RESET`, apesar de DTR/RTS desativados. Não foram enviados comandos pela serial. A captura foi encerrada. O texto `ESP_ERR_TIMEOUT` relatado pelo usuário não apareceu explicitamente nessa captura; o crash de inicialização foi confirmado e corrigido localmente.

## Entrega pela WebGUI do launcher

Após `./tools/idf.sh build`, executar `python3 tools/release.py --launcher-only` gera:

- `dist/launcher/SymbianOS-runtime.bin`: aplicação para o slot OTA do SymbianOS.
- `dist/launcher/SymbianOS/`: manifesto, recursos e diretórios do cartão; copiar preservando documentos e configurações já existentes.
- `dist/launcher/SHA256SUMS`: hash do BIN entregue.

Esse modo não gera imagens de bootloader/partições nem executa a limpeza de versões antigas de distribuição. Não acessa o Tab5, não abre serial e não publica arquivos.

Os pontos de nível médio da auditoria permanecem fora deste lote.

## Wi-Fi em instalação sem NVS

O usuário confirmou o erro `Inicializar NVS: ESP_ERR_NOT_FOUND (261)` e informou que a flash instalada pelo launcher não contém NVS. Embora os perfis Wi-Fi já fossem gravados em `/SymbianOS/config/network/known-wifi.tsv`, a inicialização do rádio ainda chamava `nvs_flash_init()` no P4 e abortava antes de conectar.

Removidas as chamadas de inicialização NVS do Wi-Fi e BLE e a dependência direta do componente no `main`. A configuração enviada ao C6 estabelece `nvs_enable=0`; o armazenamento Wi-Fi continua explicitamente em RAM. Desabilitados `WIFI_RMT_NVS_ENABLED`, `ESP_WIFI_NVS_ENABLED` e a persistência de bonds NimBLE na configuração local/release. Compilação e empacotamento recusam persistência NVS de credenciais.

O teste `network_sd_only` compila as funções nativas de inicialização com APIs simuladas, onde NVS não existe (261), e verifica que Wi-Fi/BLE iniciam sem chamar NVS. Também testa RAM no driver e preservação de erros reais de inicialização. Os 30 testes de host passaram. A associação Wi-Fi com esse novo BIN ainda exige reteste na unidade física; não foi executado flash nem aberta serial nesta correção.

Build P4 desta correção: 3.272.832 bytes, SHA-256 `30ebf43b47cdd2dfb0a5c2e01068f93379d9e791d4acdd9f96f7b56e172c132d`. A inspeção dos símbolos do ELF não encontrou `nvs_flash_init`, `nvs_open`, `nvs_set_*` ou `nvs_commit`. A revisão OTA de host passou novamente com esse BIN real.

Dados, preferências, credenciais, recursos e logs da aplicação permanecem sob `/SymbianOS` e a quota total de 4.000.000.000 bytes. Estado temporário de drivers e pareamento BLE fica em RAM. Não criar, formatar ou usar uma partição NVS como substituto do cartão.

## Inicialização repetida da interface Wi-Fi

Na captura física autorizada `build/serial-wifi-sd-only.log`, a imagem com ELF prefixo `92b78d686` abortou com `assert failed: netif_add ... (netif already added)`. A pilha decodificada aponta para `esp_netif_start_api`/`esp_netif_lwip_add`, independentemente do tratamento de PIN. O transporte também apresentou notificações repetidas de início da estação nas capturas.

A integração instala um filtro na função de postagem de eventos da tabela de abstração do ESP-Hosted 1.4.0, antes da inicialização de Wi-Fi ou BLE. START/STOP idênticos são aceitos somente uma vez por transição, antes de alcançarem o handler padrão que adiciona a interface ao lwIP. A decisão e a postagem são serializadas; uma falha ao enfileirar preserva a transição para nova tentativa. Estação e AP têm estados independentes; os demais eventos/payloads são encaminhados sem alteração. Não se usa a associação/IP como sinal de interface adicionada, pois o segundo START pode ocorrer antes do DHCP.

Após desinicialização confirmada do driver, uma parada ainda não recebida é enfileirada antes de aceitar outro início. Os contadores de START/STOP repetidos ficam em RAM e aparecem no diagnóstico do transporte. Não há novas gravações fora de `/SymbianOS`, nem alteração nas fontes ou licenças das dependências.

O teste `wifi_lifecycle` compila o filtro nativo e os handlers START/STOP do ESP-IDF instalado contra fila/radio/netif simulados. Reproduz a dupla adição sem filtro e verifica 1.000 ciclos de reinício, falhas de postagem, parada ausente, preservação de payloads, isolamento STA/AP e 16 produtores concorrentes. Esses testes de host e a compilação P4 não substituem reteste físico da conexão e da tela de bloqueio.

O BIN desta correção tem 3.273.712 bytes e destina-se ao slot OTA, com a pasta do SD em `dist/launcher/SymbianOS`. Nenhum flash nem monitor serial foi executado durante esta implementação.
