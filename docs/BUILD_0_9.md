# Validação — 0.9.0 / API 9

Build ESP32-P4 com ESP-IDF 5.4.4 concluído: **2093968 bytes**, dentro do slot de 2 MiB, com **3184 bytes livres**. A tabela de partições permanece igual; crescimento futuro exigirá redução de componentes ou outra decisão de distribuição. DIRAM estática atribuída: 125994 bytes; não representa picos físicos.

As **12 suites CTest** passaram no host. A jornada gráfica LVGL/SDL verificou reduzir/apagar a tela e despertar por touch/teclado, além das jornadas existentes. ASan/UBSan passou nas suites sem SDL; os testes JS também foram executados nessa configuração, mas JavaScript não recebe instrumentação de ASan. Os logs distinguem os ambientes.

A suite de fase 7 verifica boot interrompido, release com falha Lua, fallback, rollback manual, documento preservado, preferências de tela e confirmação/cancelamento de suspensão com backend simulado. O upload real do serviço foi exercitado com 30 MiB, leituras curtas, interrupção de bloco, repetição idempotente, confirmação final perdida e verificação byte a byte. A página JS real envia 30 MiB com rede simulada e repete HTTP 408 e confirmação perdida. Há 100 ciclos adicionais de navegação/salvamento; não equivalem a horas de uso físico nem a uma medição de memória livre no P4.

A ferramenta offline foi executada sobre cópia temporária: manifesto reconstruído idêntico, backup criado, documento preservado e traversal recusado. SHA-256 de todos os artefatos, correspondência do BIN compilado/distribuído, kit OTA e conteúdo dos ZIPs foram conferidos. O ZIP de atualização não contém dados/configurações para substituir os do usuário; o SD completo é modelo para instalação inicial.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2093968 |
| `SymbianOS-update-0.9.0.zip` | 394284 |
| `SymbianOS-sdcard.zip` | 3549383 |
| `SymbianOS-runtime-OTA-0.9.0.zip` | 1217142 |
| `SymbianOS-release-0.9.0.zip` | 390234 |
| `SymbianOS-SDK-0.9.0.zip` | 36660 |

SD modelo: 12920422 bytes lógicos, incluindo versões anteriores. Atualize BIN e SD 0.9.0 juntos e recarregue a página de transferências. O firmware inclui somente infraestrutura; os apps, temas e ícones permanecem no cartão. TLS utiliza PSRAM e conjunto completo de CAs; compilar somente o cliente TLS permitiu manter o slot existente.

Relatórios: [resumo](size-summary-0.9.json), [componentes](size-components-0.9.json), [arquivos](size-files-0.9.json). Logs: [host](host-validation-0.9.log), [sanitizadores](sanitizer-0.9.log), [ESP32-P4](esp32p4-build-0.9.log). [Fluxos e limitações](FASE_7_0_9.md), [validação anterior](BUILD_0_8_1.md).

**Validação física pendente:** conexão HTTPS/TLS, transferência de 30 MB no rádio/SD reais, consumo, deep sleep/RTC, interrupção de OTA, bootloader e horas de uso. O rollback nativo automático depende do bootloader efetivamente instalado; o BIN enviado pelo launcher não altera esse bootloader. Nenhum flash, monitor ou serial foi executado; nenhuma publicação no GitHub foi realizada.
