# Validação — 0.7.1 / API 7

Build ESP32-P4 com ESP-IDF 5.4.4 concluído. Aplicação: **1945936 bytes**, dentro do slot de 2 MiB. DIRAM estática atribuída: 118572 bytes; não mede pico de heap/pilhas.

Sete suites CTest passaram no host, com jornada gráfica LVGL/SDL. ASan/UBSan passou nas suites sem SDL. O novo teste de protocolo usa headers V4L2 da dependência e simula G_FMT com tipo zero; verifica captura padrão RGB565, negociação RAW, campos de requisição e erros. Não é um ensaio do sensor físico.

Novos casos de rede verificam scan assíncrono, deduplicação pelo sinal, filtragem de SSID com quebra de linha, seleção e máscara de senha, diagnóstico real, navegação durante scan e cancelamento preservando a última lista válida. O backend é simulado. WPA2/WPA3 e descoberta pelo C6 são compilados; precisam de aceitação no roteador/unidade reais.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 1945936 |
| `SymbianOS-update-0.7.1.zip` | 389048 |
| `SymbianOS-sdcard.zip` | 2393455 |
| `SymbianOS-SDK-0.7.1.zip` | 21110 |

SD: 7920377 bytes lógicos, incluindo versões anteriores. Hashes e identidade do BIN conferidos; ZIP de atualização contém somente manifesto, release e diretórios, preservando dados/configurações existentes.

Logs: [host](host-validation-0.7.1.log), [sanitizadores](sanitizer-0.7.1.log), [build ESP32-P4](esp32p4-build-0.7.1.log). Tamanhos: [resumo](size-summary-0.7.1.json), [componentes](size-components-0.7.1.json), [arquivos](size-files-0.7.1.json). [Detalhes da correção](CORRECOES_0_7_1.md), [validação anterior](BUILD_0_7.md).

Evidência física relatada pelo usuário na 0.7.0: duas músicas e set de DJ de 133 MB reproduzidos; QC negociado e carga chegando a 12 W. Câmera: erro 22 em Configurar RGB565; C6 respondeu versão 1.4.1, mas Wi-Fi não conectou. Nenhum desses resultados foi medido de forma independente pelo agente. Câmera e autenticação desta revisão continuam pendentes de novo ensaio físico. Nenhum flash, monitor ou serial foi executado.
