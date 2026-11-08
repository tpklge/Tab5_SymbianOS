# Validação — 0.8.0 / API 8

Build ESP32-P4 com ESP-IDF 5.4.4 concluído. Aplicação: **2010576 bytes**, dentro do slot de 2 MiB. DIRAM estática atribuída: 118636 bytes; não mede picos físicos.

Oito suites CTest passaram no host, com jornada gráfica LVGL/SDL. ASan/UBSan passou nas suites sem SDL. A nova suite verifica redes salvas entre instâncias, senha correta reutilizada nativamente, conectar sem salvar, falha de autenticação sem persistência, reconexão/backoff após perda, desconexão manual, preferências automáticas, esquecer, arquivo inválido preservado, interface sem senha e token de oito caracteres. Backend de rádio é simulado.

Mantém a correção do upload 0.7.3 e testes de streaming acima de 1 MiB, limite do buffer, quota, concorrência, interrupção e diagnóstico. O usuário confirmou arquivo enviado com sucesso após a correção, sem fornecer medições de pilha ou uma bancada exaustiva.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2010576 |
| `SymbianOS-update-0.8.0.zip` | 389739 |
| `SymbianOS-sdcard.zip` | 2777103 |
| `SymbianOS-SDK-0.8.0.zip` | 26738 |

SD modelo: 9575433 bytes lógicos incluindo versões anteriores. A release exige BIN e SD 0.8.0 juntos. As credenciais novas só são criadas no SD do usuário; a ferramenta de release recusa a pasta privada com arquivos pessoais.

Relatórios: [resumo](size-summary-0.8.json), [componentes](size-components-0.8.json), [arquivos](size-files-0.8.json). Logs: [host](host-validation-0.8.log), [sanitizadores](sanitizer-0.8.log), [ESP32-P4](esp32p4-build-0.8.log). [Uso e armazenamento das redes](REDES_SALVAS_0_8.md), [validação anterior](BUILD_0_7_3.md).

Reconexão, token curto e preferências desta versão precisam de aceitação física. Relatos anteriores confirmaram câmera, música, gravador, QC e transferência de arquivo, mas não substituem testes instrumentados de consumo, fps ou recuperação de falhas. Nenhum flash, monitor ou serial executado.
