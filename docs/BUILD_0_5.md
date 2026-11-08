# Validação — 8 de novembro de 2026 — 0.5.0 / API 4

Build ESP32-P4 concluído com ESP-IDF 5.4.4, BSP 1.3.2, LVGL 9.5.0 e Lua 5.4.9. Aplicação: **1419504 bytes**, dentro do slot de 2 MiB. Não é imagem merged nem tabela do launcher.

377 verificações passaram no host: 62 núcleo, 175 organizador e 140 mídia/instalador. Também passou a geração de fixtures de pacotes e a jornada gráfica LVGL/SDL. Foram exercitados máscara brasileira e backspace, dropdown por teclado, conversão decimal, cópia pelo gerenciador e tamanho dos controles, 20 ciclos de navegação, imagem de câmera simulada, clipboard, teclado virtual por ponteiro, alarme e reinserção.

ASan/UBSan passou nas suites sem SDL, incluindo MP3/JPEG/PNG, limite de expansão de PNG, pacotes malformados, dados de SDK e permissões. A GUI normal passou; a GUI instrumentada não é declarada validada por causa da limitação SDL2 do host documentada na 0.4.0.

| Artefato em `dist/` | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1419504 |
| `SymbianOS-update-0.5.0.zip` | 349263 |
| `SymbianOS-sdcard.zip` | 1274451 |
| `SymbianOS-SDK-0.5.0.zip` | 14071 |

SD: 3163531 bytes lógicos, incluindo releases anteriores. Quota decimal de 4.000.000.000 bytes, reserva de 64 MiB. O ZIP de atualização só contém manifesto/release/diretórios, preservando dados e preferências. SDK e pacotes de exemplo são separados e não são instalados automaticamente.

DIRAM estática atribuída: 109970 bytes. Buffers LVGL e widgets/fontes usam PSRAM. Pilhas configuradas: Lua 64 KiB, mídia 64 KiB, sinais de áudio 32 KiB, LVGL 16 KiB. Essas configurações e o relatório do linker não medem heap dinâmico ou máximo real de pilha. Codecs de imagem têm teto de alocação de 8 MiB e prévias são limitadas por dimensão/pixels. Cada app residente retém no máximo sua prévia, liberada no descarregamento.

Atribuição individual dos codecs e justificativas em [Fases 4/5](FASES_4_5.md). Relatórios completos: `size-summary-0.5.json`, `size-components-0.5.json`, `size-files-0.5.json`. Logs: `host-validation-0.5.log`, `host-case-results-0.5.log`, `sanitizer-0.5.log`, `esp32p4-build-0.5.log`.

O usuário relatou ausência de som no alarme. O backend foi revisto para estéreo 48 kHz, amplificador/desmute explícitos e diagnóstico em `/logs/last-audio.txt`. **Som, microfone, câmera, RTC, touch, teclado real, latências, consumo, cortes de energia e cartão cheio ainda precisam de ensaios físicos.** Uma chamada do driver bem-sucedida não comprova som audível. A câmera tem prévia por captura manual, sem vídeo ao vivo.

Nenhum flash, monitor, erase-flash ou acesso serial executado. Critérios físicos de passagem das fases 4/5 permanecem pendentes. [Atualização pelo launcher](ATUALIZACAO_LAUNCHER.md), [SDK](SDK.md), [versão anterior](BUILD_0_4.md).
