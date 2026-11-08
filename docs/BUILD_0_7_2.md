# Validação — residente 0.7.2 / SD 0.7.1 / API 7

Build ESP32-P4 com ESP-IDF 5.4.4 concluído. Aplicação: **2002640 bytes**, dentro do slot de 2 MiB. O linker confirma controlador ISP, configuração SC202CS de AGC/exposição e AWB presentes no BIN. DIRAM estática atribuída: 118636 bytes; não mede pico de heap/pilhas. O crescimento do código nesta revisão corresponde à ativação do controlador automático/algoritmos de imagem e parâmetros do sensor.

Sete suites CTest passaram no host e ASan/UBSan, com jornada gráfica normal LVGL/SDL. Novos casos verificam prévia acima do antigo teto de 5 fps em backend simulado, prévia mais rápida que a gravação durante filmagem e número de quadros do AVI compatível com a cadência de gravação de até 5 fps. O controlador automático real não é exercitado no host.

| Artefato em dist/ | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2002640 |
| `SymbianOS-update-0.7.1.zip` | 389048 |
| `SymbianOS-sdcard.zip` | 2393455 |
| `SymbianOS-SDK-0.7.1.zip` | 22543 |

Hashes da distribuição e identidade com o BIN compilado conferidos. O conteúdo do SD 0.7.1 não mudou nesta correção: se já está instalado, basta atualizar o BIN. O pacote de SD continua preservando os documentos existentes.

Relatórios: [resumo](size-summary-0.7.2.json), [componentes](size-components-0.7.2.json), [arquivos](size-files-0.7.2.json). Logs: [host](host-validation-0.7.2.log), [sanitizadores](sanitizer-0.7.2.log), [ESP32-P4](esp32p4-build-0.7.2.log). [Mudanças e limites](HOTFIX_0_7_2.md), [versão anterior](BUILD_0_7_1.md).

Alvo de prévia de até 20 fps, luminosidade, convergência de exposição, cor, recursos simultâneos e reabertura precisam de validação física. O usuário confirmou imagem escura/lenta na versão anterior, além de áudio e carga QC funcionando. Nenhum flash, monitor ou serial foi executado.
