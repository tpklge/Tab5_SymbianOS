# Validação — 0.11.0 / API 11

Build ESP32-P4 com ESP-IDF 5.4.4 concluído: **2092560 bytes**, **4592 bytes livres** no slot de 2 MiB. Partições preservadas; TLS cliente e CAs mantidos. O residente está próximo do limite; próximas extensões nativas exigem redução antes de ampliar o escopo.

**14 suites CTest passaram em host e sob ASan/UBSan.** Os testes JavaScript executam fora da instrumentação do motor. A suite polish cobre enquadramento HTTP, espera temporária/EOF, Ajustes/sliders/PIN, perfis, estados de rede, memória e apresentação das ações. A jornada gráfica passou novamente após corrigir o limite do atlas e alinhamento das legendas: sliders, descarte da prévia, atualização automática de Wi-Fi, ícones de arquivos, fechamento X, PIN, teclado, navegação, alarmes e persistência. Prévia usa dados sintéticos de bateria; memória física indisponível é explicitada no host.

| Artefato local | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2092560 |
| `SymbianOS-update-0.11.0.zip` | 441111 |
| `SymbianOS-sdcard.zip` | 4391424 |
| `SymbianOS-SDK-0.11.0.zip` | 63413 |

SD modelo: **18128382 bytes lógicos**, 399 arquivos incluindo releases anteriores. SHA-256 e identidade entre BIN compilado/distribuído conferidos. ZIP incremental contém manifesto API 11 e release, sem documentos nem credenciais; atlas de 72 ícones dentro de 2 MiB. Atualizar BIN e conteúdo SD juntos. OTA própria não foi executada.

[Interface e limites](INTERFACE_0_11.md), [prévia de Ajustes](preview-energy-0.11.png), [relatório anterior](BUILD_0_10.md). Logs: [host](host-validation-0.11.log), [sanitizadores](sanitizer-0.11.log), [ESP32-P4](esp32p4-build-0.11.log). Tamanhos: [resumo](size-summary-0.11.json), [componentes](size-components-0.11.json), [arquivos](size-files-0.11.json).

**Pendente no Tab5:** validar RSS nos servidores afetados, toque, áudio, heap/PSRAM, suspensão/retomada e consumo. OTA e atualização própria continuam aguardando a validação física combinada. Nenhum flash, monitor, serial ou publicação no GitHub executado. Meshtastic/RAK, BLE, e-mail offline e SIS/SISX/JAR próprios permanecem no planejamento.
