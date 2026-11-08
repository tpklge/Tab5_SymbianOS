# Validação — 0.7.0 / API 6

Build ESP32-P4 com ESP-IDF 5.4.4 concluído. Aplicação: **1939552 bytes**, dentro do slot de 2 MiB. Somente imagem de aplicativo, sem bootloader/partições embutidos.

Seis suites CTest passaram no host: núcleo, organizador, fixtures de pacotes, mídia/instalador, responsividade/mídia e rede/energia. A jornada gráfica LVGL/SDL passou. ASan/UBSan passou nas suites sem SDL, incluindo os novos casos. O teste incremental de mídia também cobre geração de PCM para descarte de dados antigos após Parar. Dispositivos e transporte são simulados no host. A fila I²S e drivers de bateria/C6/câmera são compilados para a placa; não são exercitados fisicamente pelos testes de host.

Testes adicionais de rede verificam caminhos confinados, URLs, senha mascarada, volume, cancelamento, destino existente, HTTP interrompido, snapshot de feed preservado, recusa de DTD, upload/download por streaming, uploads concorrentes recusados e quota próxima do limite.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1939552 |
| `SymbianOS-update-0.7.0.zip` | 388503 |
| `SymbianOS-sdcard.zip` | 2010501 |
| `SymbianOS-SDK-0.7.0.zip` | 18460 |

SD: 6268573 bytes lógicos, incluindo versões anteriores; quota decimal de 4 GB. Atualização contém somente release/manifesto/diretórios, sem documentos/preferências. BIN e SD devem ser atualizados juntos.

| Atribuição do linker | Flash (bytes) |
|---|---:|
| `liblvgl__lvgl.a` | 277951 |
| `libplatform.a` | 172349 |
| `libespressif__esp_hosted.a` | 82244 |
| `libmbedtls.a` | 46203 |
| `libesp-tls.a` | 6478 |
| `libesp_http_client.a` | 8553 |
| `libesp_http_server.a` | 10991 |
| `libespressif__esp_cam_sensor.a` | 8302 |

Atribuições podem refletir fusão de constantes e seções; não são diferenças isoladas de custo por recurso. O aumento desta revisão inclui transporte C6/RPC, HTTP/TLS e driver do sensor, necessários às APIs de rede/câmera. Interface, política, página web e ícones permanecem no SD. DIRAM estática atribuída: 118556 bytes. Isso não mede pico de heap, DMA ou pilha em execução.

Novas tarefas são iniciadas sob demanda: fila I²S com pilha de 32 KiB e rede com 48 KiB. A fila tem até 12 pacotes PCM; o cache de áudio codificado tem 64 KiB. Buffers de câmera, TLS e transporte têm custos adicionais dinâmicos. Nenhuma alegação de consumo, fluidez ou memória física máxima.

Relatórios: [resumo](size-summary-0.7.json), [componentes](size-components-0.7.json), [arquivos](size-files-0.7.json). Logs: [host](host-validation-0.7.log), [sanitizadores](sanitizer-0.7.log), [ESP32-P4](esp32p4-build-0.7.log).

O gravador foi confirmado pelo usuário na revisão anterior. Player ainda apresentou engasgos e câmera apresentou erro 2; há correções nesta versão, mas a confirmação na unidade permanece pendente. Bateria, corrente/QC, associação C6/Wi-Fi, TLS e transferência pelo rádio precisam de bancada. Nenhum flash, monitor ou acesso serial executado. [Entrega e limites](FASE_6_0_7.md), [roteiro físico](TESTES_FISICOS.md).
