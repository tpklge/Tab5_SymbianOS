# Tab5 SymbianOS 1.0.0

Publicação de 13/11/2026, API 14, baseada na revisão 20261113-R3 aceita pelo usuário. A alteração de versão não acrescenta novas funcionalidades ao firmware aceito.

**Leia primeiro [Limitações conhecidas](LIMITACOES_1_0_0.md).** Web moderna, sessões, formulários e SIS têm suporte parcial. Esta release não executa aplicações Symbian originais.

Inclui shell/aplicativos Lua, Wi-Fi sem NVS, e-mail, feeds, navegador, compartilhamento de arquivos por token, SIS sem assinatura e recuperação/rollback. Os últimos ajustes compartilham a preparação limitada de HTML/CSS entre navegador/e-mail/feed, usam PSRAM na preparação e nas pilhas de rede/e-mail, oferecem links por toque e controles laterais por ícones e mantêm leitura alternativa sob pressão de memória.

O usuário confirmou o funcionamento físico da R3. Essa confirmação é relatada pelo usuário; o agente não acessou serial nem gravou o dispositivo nesta publicação. A numeração 1.0.0 foi compilada e verificada em host separadamente. Validação da 1.0.0: 47/47 testes de host aprovados, jornada LVGL aprovada (incluindo links por toque e controles compactos) e build ESP32-P4 aprovado. A mudança de número de versão não foi instalada fisicamente pelo agente.

Arquivos da release: aplicação BIN, ZIP da pasta SymbianOS, SDK e kit de testes SIS, licenças e SHA256SUMS. A pasta e o BIN devem ser enviados juntos pela WebGUI do launcher, que já deve estar instalado com slot de 4 MiB. Atualização inicial de launcher, bootloader, partições e firmware do C6 não está incluída.

BIN: 3601136 bytes. SHA-256: `c240b1476f08f1fbe2c47615a04fba89110f91b80040448c4f8b3d3a5afdaed9`.
