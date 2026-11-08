# Validação — SD 0.6.0 / residente 0.6.1 / API 5

[Hotfix de inicialização e sua validação](HOTFIX_0_6_1.md).

Build ESP32-P4 concluído com ESP-IDF 5.4.4. Aplicação: **1439024 bytes**, dentro do slot de 2 MiB. Trata-se somente da imagem de aplicativo.

As cinco suites CTest passaram no host, incluindo núcleo, organizador, pacotes e mídia; a jornada gráfica LVGL/SDL também passou. Testes novos verificam comandos/navegação com dispositivo de áudio bloqueado, notas independentes, resultado do conversor, contador/nível de gravação, AU e AVI. ASan/UBSan passou nas suites sem SDL. A GUI instrumentada continua sem validação pela limitação SDL do host documentada anteriormente.

FFprobe/FFmpeg identificaram e decodificaram os arquivos de teste AU/G.711 e AVI/MJPEG: [relatório](media-format-check-0.6.json). Isso verifica os arquivos produzidos com dispositivos simulados, sem comprovar áudio ou câmera físicos.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1439024 |
| `SymbianOS-update-0.6.0.zip` | 358938 |
| `SymbianOS-sdcard.zip` | 1627694 |
| `SymbianOS-SDK-0.6.0.zip` | 14614 |

Todos os hashes de distribuição foram conferidos. O ZIP de atualização contém somente manifesto, release 0.6.0 e diretórios; não contém documentos/preferências para substituir dados existentes. SD: 4617598 bytes lógicos incluindo releases anteriores, sob quota decimal de 4 GB.

Logs: [host](host-validation-0.6.log), [ASan/UBSan](sanitizer-0.6.log), [ESP32-P4](esp32p4-build-0.6.log), [tamanho do linker](size-summary-0.6.json). Tamanho estático não mede pico de heap ou pilha.

Som contínuo, microfone audível, câmera, desempenho e recuperação de energia precisam de validação física no Tab5. Nenhum flash, monitor ou acesso serial executado. [Correções e limites](CORRECOES_0_6.md), [atualização](ATUALIZACAO_LAUNCHER.md), [validação anterior](BUILD_0_5.md).
