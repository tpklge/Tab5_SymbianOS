# Validação — 0.12.0 / API 12

Build ESP32-P4 / ESP-IDF 5.4.4 concluído: **1992992 bytes**, **104160 bytes livres** no slot original de 2 MiB. BLE NimBLE central/C6 e Grove UART incluídos. Sem mudança de partições; CAs e TLS cliente preservados. Parsers de linhas sem streams/locale e LTO de main/platform/Lua/LVGL reduziram o residente mesmo com os transportes novos. A mudança do otimizador exige teste geral na placa; host não executa esse binário.

**15 suites CTest passaram em host e sob ASan/UBSan.** A suite mesh verifica protocolo original, truncamento, UTF-8, fila offline, espera por config_complete_id correspondente, entrega ao transporte versus recibo do rádio/ACK, deduplicação, notificações, perfil offline, campos BLE, atalhos da Home e bloqueio do evento privilegiado para app sem mesh. Compara também o novo parser com std::getline nos limites de EOF/delimitador. JavaScript executa fora da instrumentação do motor.

A jornada gráfica LVGL/SDL passou: linhas do alternador com intervalo entre nome e memória, fechamento X, sliders, Wi-Fi automático, PIN, teclado/toque, máscaras, arquivos, navegação, alarmes e persistência. Prévia usa bateria sintética e memória Lua real do simulador; dados físicos de heap/PSRAM permanecem explicitamente indisponíveis no host.

No computador, HTTP/1.1 de gizmodo.com.br/feed seguiu 301 para www e recebeu HTTP 200/RSS completo de 25143 bytes. No runtime, a janela sem progresso é de 30 segundos com leitura de 1 KiB e cancelamento. Esse ajuste não equivale à validação do erro da foto no Tab5.

| Artefato local | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1992992 |
| `SymbianOS-update-0.12.0.zip` | 459492 |
| `SymbianOS-sdcard.zip` | 4844443 |
| `SymbianOS-SDK-0.12.0.zip` | 66642 |

SD modelo: **21305512 bytes lógicos**, 452 arquivos incluindo releases anteriores. Conferidos SHA-256 de todos os artefatos e licenças, identidade entre BIN compilado/distribuído, manifesto API 12, atlas de 74 ícones abaixo de 2 MiB, presença dos apps/licenças NimBLE e ausência de documentos/credenciais no ZIP incremental. Nenhum PIN BLE é persistido no cartão. Atualizar BIN e SD juntos; kit de runtime OTA também tem BIN/SHA conferidos.

[Recursos/uso/limites](MESHTASTIC_0_12.md), [Home](preview-shell-home-0.12.png), [Abertos](preview-shell-switcher-0.12.png), [Meshtastic](preview-mesh_ble-0.12.png), [relatório anterior](BUILD_0_11.md). Logs: [host](host-validation-0.12.log), [sanitizadores](sanitizer-0.12.log), [ESP32-P4](esp32p4-build-0.12.log). Tamanhos: [resumo](size-summary-0.12.json), [componentes](size-components-0.12.json), [arquivos](size-files-0.12.json).

**Pendente no Tab5:** firmware HCI do C6, busca/pareamento/bonds/MTU, mensagens e confirmações em rádios reais, alimentação/cabeamento do RAK, queda/reconexão, coexistência Wi-Fi/BLE, RSS, suspensão e teste geral com LTO. OTA própria continua aguardando aceitação física. Nenhum flash, monitor, abertura de serial ou publicação no GitHub executado. E-mail offline e SIS/SISX/JAR seguem planejados.
