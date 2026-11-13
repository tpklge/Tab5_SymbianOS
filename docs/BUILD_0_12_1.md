# Validação — 0.12.1 / API 12

Build ESP32-P4 / ESP-IDF 5.4.4 concluído: **1997040 bytes**, **100112 bytes livres** no slot original de 2 MiB. Nenhuma mudança em partições ou firmware C6. LTO e validação TLS/CAs preservados. O malloc acima de 1 KiB e Wi-Fi/lwIP priorizam PSRAM; pilhas UI/rede de 48/32 KiB continuam internas. Tarefa UART de 4 KiB e fila de quadros em PSRAM. Esses valores são de configuração, não medições do heap na placa.

**16 suites CTest passaram em host e ASan/UBSan**; JavaScript executa fora da instrumentação do motor. radio_rx testa 200 quadros recebidos antes de qualquer consumo, fragmentação byte a byte, ruído, ressincronização, comprimento inválido, limite/perdas e níveis Wi-Fi. Integração testa polling RSSI/cache/adiamento no PIN, erro HTTP/TLS inteiro no leitor, fim do aviso Baixando após falha, Menu 4/2 colunas e fluxo RAK em um toque. Suites anteriores de protocolo, memória, instalação, persistência, áudio, câmera, HTTP e segurança permanecem verdes.

A jornada gráfica passou com sequência física matrix:1..4/Enter sem aguardar cada ação, toque rápido, teclado touch estável entre dígitos, rascunho preservado e demais fluxos. Dados de bateria/sinal nas prévias são sintéticos; o simulador não abre UART nem um rádio BLE real. A recepção nativa/filas e stacks exigem teste no Tab5.

| Artefato local | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1997040 |
| `SymbianOS-update-0.12.1.zip` | 459901 |
| `SymbianOS-sdcard.zip` | 5297240 |
| `SymbianOS-SDK-0.12.1.zip` | 69313 |

SD modelo: **24483364 bytes lógicos**, 504 arquivos incluindo releases anteriores. Conferidos SHA-256, identidade BIN compilado/distribuído, versão/API, ZIP incremental sem documentos/credenciais, scripts atualizados, licenças NimBLE e kit OTA com BIN correspondente. Atualizar BIN e SD juntos.

[Correções e uso](CORRECOES_0_12_1.md), [grade](preview-shell-0.12.1.png), [lista](preview-shell-menu_mode-0.12.1.png), [Wi-Fi simulado](preview-shell-signal-0.12.1.png), [relatório anterior](BUILD_0_12.md). Logs: [host](host-validation-0.12.1.log), [sanitizadores](sanitizer-0.12.1.log), [ESP32-P4](esp32p4-build-0.12.1.log). Tamanhos: [resumo](size-summary-0.12.1.json), [componentes](size-components-0.12.1.json), [arquivos](size-files-0.12.1.json).

**Pendente no Tab5:** resposta/sincronização real do RAK, contadores/perdas, Wi-Fi/RSSI e coexistência, heap/maior bloco/stacks, RSS do Gizmodo e PIN físico/touch. OTA própria continua aguardando aceitação. Nenhum flash, monitor, abertura de serial ou publicação executado pelo agente.
