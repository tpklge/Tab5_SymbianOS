# Validação — 0.18.1 / API 14

Build ESP32-P4: 2089536 bytes, 7616 bytes livres no slot de 2 MiB. Streaming SDIO ativo e partições preservadas.

24 suítes host e jornada LVGL passaram; dez suítes relacionadas passaram com ASan/UBSan. A regressão de textarea mede o número de eventos ao carregar texto longo UTF-8 e confere limites e filtros. A jornada verifica quatro atualizações dos valores de heap sem trocar a árvore LVGL e posição do botão OTA fora dos cartões. O e-mail inclui uma mensagem longa com 2.200 caracteres multibyte para abrir/responder. SMTP verifica 535 antes de MAIL FROM/DATA, ausência de segredos e senha de envio vazia. Backup/restauração da árvore atual passou após ampliar o limite a 4.096 entradas.

Captura física na versão instalada 0.18.0: quatro watchdogs de taskLVGL/IDLE0; [serial](serial-watchdog-0.18.0.log) e [endereços decodificados](serial-watchdog-decoded-0.18.0.log). O monitor foi autorizado explicitamente e encerrado. Sua abertura provocou CHIP_USB_UART_RESET apesar de DTR/RTS desativados e isso foi informado ao usuário. Nenhum flash, OTA, erase, envio real de e-mail ou publicação foi executado. Validação física da correção 0.18.1 ainda pendente.

[Correções](INTERFACE_SMTP_0_18_1.md), [medição RISC-V do núcleo HTML/CSS](TAMANHO_HTML_CSS_0_18_1.md). Evidências: [host/LVGL](validation-host-0.18.1.log), [ASan/UBSan](validation-sanitizer-0.18.1.log), [build ESP32-P4](validation-build-0.18.1.log).

Pacotes conferidos: BIN idêntico ao compilado, slot respeitado, manifesto 0.18.1/API14, hashes e ZIPs íntegros, pares OTA correspondentes, sem dados pessoais nos pacotes e streaming SDIO ativo. Dist conserva 0.18.1, 0.18.0 e 0.17.1. Prévia de Testes e medições inspecionada no simulador.
