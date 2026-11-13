# Validação — 0.10.0 / API 10

Build ESP32-P4 com ESP-IDF 5.4.4 concluído: **2081568 bytes**, com **15584 bytes livres** no slot de 2 MiB. DIRAM estática atribuída: 124152 bytes. Não houve mudança na tabela de partições. O perfil usa logs WARN/ERROR e diagnósticos em arquivo; TLS cliente mantém ECDHE-RSA/ECDHE-ECDSA e CAs completas, removendo métodos legados não utilizados. LTO não está habilitado.

**13 suites CTest passaram no host**. A nova suite cobre três temas, wallpaper limitado, preferências, Home com compromisso, Menu/pastas/favoritos/lista, formulário brasileiro e calendário semanal, biblioteca acima de 64 músicas, playlists, modos persistidos, zoom/navegação, assinaturas/artigos offline e PIN. Inclui bloqueio de abertura direta de apps, desbloqueio correto, novo boot bloqueado e espera após cinco erros, preservada após reinício.

A jornada gráfica LVGL/SDL passou com teclado numérico do PIN e preservação/retomada de rascunho não salvo ao bloquear. Mantém os testes de toque/teclado, máscaras/dropdowns, navegação, calculadora, persistência, alarmes e energia. ASan/UBSan passou no núcleo sem SDL; a experiência foi conferida novamente após os ajustes finais de tema e apresentação. JavaScript de upload foi executado, sem instrumentação do motor JS pelos sanitizadores. Upload de 30 MiB e repetição idempotente continuam nas suites; o usuário também confirmou transferência funcionando na versão anterior.

HTTP/1.1 no computador recebeu RSS do Gizmodo; [resumo da coleta](feed-http1-host-0.10.txt). Não é evidência de conexão do P4. O novo diagnóstico de cabeçalhos e a suspensão em memória precisam de aceitação na placa. Prévia gráfica não demonstra consumo ou retomada física.

| Artefato local | Bytes |
|---|---:|
| `SymbianOS-runtime.bin` | 2081568 |
| `SymbianOS-update-0.10.0.zip` | 413624 |
| `SymbianOS-sdcard.zip` | 3956853 |
| `SymbianOS-SDK-0.10.0.zip` | 61388 |

SD modelo: 15036838 bytes lógicos, incluindo releases anteriores. Verificados hashes SHA-256, identidade entre BIN compilado/distribuído, manifesto API 10, três wallpapers distintos, conteúdo do ZIP de atualização e ausência de credenciais/PIN no SD distribuído. O pacote preserva documentos/configurações; BIN e SD precisam ser atualizados juntos. Os kits opcionais para atualizador próprio também foram gerados e seus hashes conferidos, sem executar OTA.

[Recursos e limites](EXPERIENCIA_0_10.md), [planejamento ampliado](../plano_sistema_s60_tab5.md), [relatório anterior](BUILD_0_9.md). Logs: [host](host-validation-0.10.log), [sanitizadores](sanitizer-0.10.log), [ESP32-P4](esp32p4-build-0.10.log). Tamanhos: [resumo](size-summary-0.10.json), [componentes](size-components-0.10.json), [arquivos](size-files-0.10.json).

**Pendente no Tab5:** feed/cabeçalhos, suspensão/retomada no próprio runtime, consumo, validação de touch/teclado e uso prolongado dos novos fluxos. OTA e atualização própria ficam para a próxima validação física, conforme solicitado. Nenhum flash, monitor, serial ou publicação no GitHub executado. Meshtastic/RAK, BLE, e-mail offline e SIS/SISX/JAR próprios constam no planejamento; não estão implementados como provedores/runtimes.
