# Validação — 8 de novembro de 2026 — 0.4.0

Build ESP32-P4 concluído com ESP-IDF 5.4.4, BSP 1.3.2, LVGL 9.5.0 e Lua 5.4.9. Binário de aplicação: **1225424 bytes**, em slot de 2 MiB.

237 verificações de host passaram: 62 núcleo e 175 organizador. Jornada gráfica LVGL/SDL passou, incluindo 20 ciclos dos caminhos de relógio, conversor, alternador e voltar; calculadora touch, clipboard, teclado virtual, alarmes e recuperação de cartão.

ASan/UBSan passou nas suites de núcleo e organizador. A GUI instrumentada não completou a inicialização SDL2 no host; não é declarada validada com sanitizadores. Detalhes e limites em [Interface 0.4](INTERFACE_0_4.md).

| Artefato em `dist/` | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1225424 |
| `SymbianOS-update-0.4.0.zip` | 329210 |
| `SymbianOS-sdcard.zip` | 930392 |

SD: 2101346 bytes lógicos, incluindo releases anteriores. Quota decimal: 4.000.000.000 bytes. O ZIP de atualização contém apenas manifesto, release 0.4.0 e diretórios; não substitui documentos/sessões/configurações existentes. [Atualização pelo launcher](ATUALIZACAO_LAUNCHER.md).

DIRAM estática do linker: 106154 bytes (antes: 172166). LVGL usa alocador PSRAM, sem pool fixo de 64 KiB; buffers de desenho/rotação usam PSRAM DMA. Executor permanente Lua: pilha de 64 KiB; áudio: 16 KiB; LVGL: 16 KiB. São valores estáticos/configurados, não medições físicas de heap, fragmentação ou máximo de pilha utilizado.

Relatórios: `size-summary-0.4.json`, `size-components-0.4.json`, `host-validation-0.4.log`, `host-case-results-0.4.log`, `esp32p4-build-0.4.log`. A [validação anterior](BUILD_0_3.md) permanece histórica.

O usuário relatou tela azul e reinício na 0.3.0. A condição de pool gráfico insuficiente foi corrigida e os caminhos passaram no host; **a 0.4.0 ainda precisa de confirmação no Tab5**. Nenhum flash, monitor ou acesso serial foi executado. Não foram medidas memória dinâmica, RTC físico, áudio, rádio, toque ou teclado real.
