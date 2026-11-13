# Validação — 0.13.0 / API 13

Build ESP32-P4 / ESP-IDF 5.4.4 concluído: **2001264 bytes**, **95888 bytes livres** no slot original de 2 MiB. Partições, firmware C6, paisagem e validação TLS preservados. Native UI genérica usa ícones externos e novos layouts; assets e regras dos aplicativos permanecem no SD.

**16 suites CTest passaram em host e ASan/UBSan.** Incluem quota concorrente/reserva/transações/recuperação e novos saldos incrementais, confirmação síncrona de comando RAK com permissões, histórico em chat, recepção RSS sem tocar SD durante os chunks, cache anterior preservado e repetição com buffer reiniciado. Nenhuma resposta incompleta é promovida a cache. JavaScript roda fora da instrumentação do motor.

A jornada LVGL/SDL passou: PIN por matriz física/touch rápido e widgets estáveis, indicador oculto em atualização automática, navegação, máscaras, sliders, Wi-Fi, alternador, arquivos, câmera sintética, alarmes e persistência. Prévia de chat/bateria usa fixtures sintéticas. O teste não executa os drivers UART/C6 da placa nem mede tempo no SD físico.

| Artefato local | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2001264 |
| `SymbianOS-update-0.13.0.zip` | 536122 |
| `SymbianOS-sdcard.zip` | 5826259 |
| `SymbianOS-SDK-0.13.0.zip` | 72516 |

SD modelo: **27693360 bytes lógicos**, 558 arquivos, incluindo releases anteriores. Verificados todos os hashes, identidade BIN compilado/distribuído, API 13, atlas 75 ícones abaixo de 2 MiB, mapeamento/hash dos 24 assets de apps fornecidos, avisos/licenças e ausência de documentos/credenciais no ZIP incremental. Dist conserva somente 0.13.0, 0.12.1 e 0.12.0 e arquivos compartilhados.

[Interface e correções](INTERFACE_0_13.md), [Home](preview-shell-home-0.13.png), [Menu](preview-shell-0.13.png), [lista](preview-shell-menu_mode-0.13.png), [chat simulado](preview-mesh_rak-history-0.13.png), [anterior](BUILD_0_12_1.md). Logs: [host](host-validation-0.13.log), [sanitizadores](sanitizer-0.13.log), [ESP32-P4](esp32p4-build-0.13.log). Tamanhos: [resumo](size-summary-0.13.json), [componentes](size-components-0.13.json), [arquivos](size-files-0.13.json).

**Pendente no Tab5:** tempo real de desbloqueio/UI e quota no cartão, conexão/sincronização RAK e tráfego TX/RX, RSS de 1363 bytes, heap/stacks/coexistência. Atualizar BIN/API 13 e SD juntos; OTA própria segue pendente de aceitação. Não houve flash, monitor, abertura de serial ou publicação no GitHub. E-mail/SIS/SISX/JAR continuam planejados.
