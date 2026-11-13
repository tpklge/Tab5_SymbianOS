# Validação — 0.8.1 / API 8

Build ESP32-P4 com ESP-IDF concluído: **2015536 bytes**, dentro do slot de 2 MiB. BIN e SD 0.8.1 devem ser atualizados juntos.

As dez suites CTest passaram no host, incluindo redirecionamentos HTTP, limite do upload e teclado. A jornada gráfica LVGL/SDL passou. ASan/UBSan passou nas suites sem SDL. O JavaScript real da página de upload passou em dez verificações, incluindo rejeição de 100 MiB antes de chamar fetch e aceitação do limite de 64 MiB.

O teclado foi verificado com Sym mantido e tocado antes de `(`, ambos produzindo `/`; repetição e consumo do modificador também foram testados. O servidor recusa uploads acima de 67108864 bytes com HTTP 413, antes de receber o corpo ou criar arquivo. Redirecionamentos têm limite de cinco saltos, detecção de ciclos e recusa de downgrade HTTPS para HTTP.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2015536 |
| `SymbianOS-update-0.8.1.zip` | 390184 |
| `SymbianOS-sdcard.zip` | 3161193 |
| `SymbianOS-SDK-0.8.1.zip` | 28910 |

SD modelo: 11231556 bytes lógicos, incluindo versões anteriores. Hashes SHA-256 e correspondência entre o BIN compilado e distribuído foram conferidos. O pacote de atualização preserva dados e configurações existentes.

Relatórios: [resumo](size-summary-0.8.1.json), [componentes](size-components-0.8.1.json), [arquivos](size-files-0.8.1.json). Logs: [host](host-validation-0.8.1.log), [sanitizadores](sanitizer-0.8.1.log), [ESP32-P4](esp32p4-build-0.8.1.log). [Detalhes das correções](CORRECOES_0_8_1.md), [validação anterior](BUILD_0_8.md).

As novas jornadas ainda precisam de confirmação física no Tab5. Nenhum flash, monitor ou serial foi executado. Desligamento automático da tela continua reservado à fase 7. Esta versão foi preparada localmente, sem publicação no GitHub.
