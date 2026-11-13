# Desenvolvimento incremental

> **Estado publicado: 1.0.0 / API 14.** O usuário confirmou funcionamento dos ajustes R3 em 13/11/2026. A publicação mantém compatibilidade limitada, descrita em [Limitações 1.0.0](docs/LIMITACOES_1_0_0.md). Esta atualização de estado substitui referências históricas a validações pendentes e versões locais; não certifica testes físicos exaustivos. Atualização de firmware permanece exclusiva pelo launcher.


Ajustes atuais de instalador e envio: [lista SIS, rolagem e envio web](docs/INSTALADOR_ROLAGEM_ENVIO_WEB.md). Controles à direita, posição preservada e envio por token na porta 8080. A validação física SIS informada pelo usuário foi registrada; estes ajustes novos ainda aguardam teste no Tab5.

## SIS sem assinatura — fases 5 e 6

Implementados atualização, dependências/ciclos, novas permissões revisadas, esquema de dados com migração declarativa, downgrade confirmado, primeira abertura com recuperação, rollback com snapshot e remoção transacional. SDK e kit de testes originais são gerados com `python3 -B tools/release_sis.py`. Contrato: [Ciclo de vida SIS](docs/SIS_CICLO_DE_VIDA.md). Validação física pendente: [Roteiro físico SIS](docs/ROTEIRO_FISICO_SIS.md). Esta seção substitui os estados históricos abaixo que indicam essas etapas como futuras. SISX e assinaturas continuam adiados para a fase final. Tudo persiste no SD; firmware continua pelo launcher.

## SIS sem assinatura — fases 1 a 4

Implementados contrato, ferramentas de host, inspeção e instalação transacional para aplicações Lua no [Perfil SIS 1](docs/PERFIL_SIS_1.md). A validação física no Tab5 está pendente. Este estado substitui a indicação anterior de SIS apenas planejado. Assinaturas/SISX ficam para a última fase; atualizações de firmware continuam exclusivamente pelo launcher.

## Decisão vigente: navegador e atualizações pelo launcher

Na revisão 20261113-R3, o navegador abre em leitura nativa, oferece HTML/CSS e imagens com limites explícitos, JavaScript inline ES5 básico, formulários GET/POST e cookies/sessões no SD. Os controles usam ícones em coluna estreita e os links aceitam toque; a compatibilidade com sites modernos permanece parcial. A tela PIN aceita OK por toque ou Enter no teclado físico oficial. Estado dos seis itens e roteiro físico: [Navegador, e-mail e feeds R3](docs/NAVEGADOR_EMAIL_FEEDS_20261113_R3.md). A causa exata da queda anterior e os testes físicos desta revisão permanecem pendentes.

Atualizações do runtime passam exclusivamente pelo launcher. Ficam abandonados o aplicativo e o fluxo de OTA interno, incluindo kits de atualização gerados para esse fluxo. A interface web autenticada por token continua para transferência de arquivos. Referências anteriores a OTA neste documento registram decisões históricas e não autorizam reintrodução desse mecanismo.

Pacotes SIS/SISX ficam somente planejados em [Plano de pacotes](docs/PLANO_PACOTES_SIS_SISX.md); o instalador atual não passa a reconhecer essas extensões. Wi-Fi foi confirmado funcionando pelo usuário; o teste físico de e-mail foi adiado para a próxima implementação.

Seguir os critérios do plano_sistema_s60_tab5.md.

| Fase | Estado |
|---|---|
| 0 — runtime, raiz, quota, teclado | Base implementada e testada em host/build. Usuário confirmou boot/interface da versão anterior; bancada completa ainda pendente. |
| 1 — ambiente externo | Shell, home, menu, Notas, Calculadora e teclado virtual no SD; simulador compartilhado. Agora tema escuro e paisagem. |
| 2 — multitarefa e persistência | Gerenciador/LRU/checkpoints, falhas isoladas entre VMs, alternador, gerações de eventos, notificações, perfis e quotas Lua implementados. Testes de host disponíveis; validação física pendente. |
| 3 — organizador e arquivos | 0.17.0: favoritos persistentes e escolha de três tons, mantendo CRUD, agenda, arquivos e backup. Host/falhas cobertos; bancada de áudio/cartão/restore pendente. |
| 4 — multimídia | Player/gravador/câmera funcionais confirmados anteriormente. 0.17.0 acrescenta contadores e coleta para vídeo/áudio simultâneo; medições físicas e benchmark no dispositivo continuam pendentes. |
| 5 — instalador e SDK | Implementação 0.5.0: ZIP STORED .t5pkg, validação de metadados/CRC/hash/limites, revisão de permissões, ativação transacional, remoção com dados opcionais, paletas e SDK externo. Validação de corte de energia e demais critérios físicos pendentes. |
| 6 — rede e RSS | HTML/imagens e fluxo SDIO implementados; e-mail recebido confirmado. 0.17.0 cobre 60 ciclos de quedas/cancelamento e falta de espaço em host. Ensaio físico prolongado continua pendente. |
| 7 — energia e atualizações | OTA/rollback/retomada implementados; 0.17.0 acrescenta registro de energia/memória em segundo plano e revisão OTA sem gravação. Cortes de energia, autonomia e rollback real continuam pendentes. |
| 8 — experiência | 0.17.0: português, inglês e espanhol persistentes, dados/chaves preservados, componentes principais localizados e rodapé corrigido. Home/Menu/temas/PIN/agenda/playlists/zoom existentes. Aceitação física pendente da nova interface. |
| 9 — Meshtastic/RAK e BLE | Tráfego público RAK confirmado; interface/canais/fila/nós presentes. Privadas com incompatibilidade de canal/chave seguem pausadas, além de aceitação BLE/coexistência. |
| 10 — e-mail offline | 0.17.0: anexos recebidos/exportados/enviados em blocos e exclusões persistentes por conta/UID, restauração à pasta original e cache sob falta de espaço. SMTP real/anexos e casos físicos ainda pendentes; sem OAuth ou espelhamento IMAP completo. |
| 11 — SIS/SISX/JAR próprios | Investigação aprovada de pacote versus execução/ABI/VM; não implementado nem compatibilidade geral com apps antigos. |
| 12 — consolidação | Testes físicos, consumo/desempenho, recuperação e distribuição; OTA própria fica para a próxima validação física. |

A próxima aceitação é atualizar manualmente pelo launcher e testar rotação/toque, teclado, persistência, som/RTC, controle de rádio e falhas de cartão na unidade real, com dados de teste. O agente não executou flash nem abriu serial. Resultados desktop e de compilação não substituem medições físicas.

Entrega local 0.13.0/API 13: nova apresentação inspirada no S60 com ícones fornecidos, Home e chat, comandos do Menu centralizados, quota incremental, recebimento RSS em RAM e confirmação imediata do comando RAK. Preserva paisagem. Falhas/tempos reais de RAK, RSS, heap e desbloqueio continuam para aceitação na placa.

### Incremento Meshtastic 0.14.0

Envio e recebimento RAK confirmados pelo usuário na 0.13.3. A 0.14.0 acrescenta composição inline, seletor de canal/destinatário, mensagens privadas filtradas, painel de nós e nome local. Administração de canais locais assíncrona com passkey, proteção do primário e confirmação por releitura. Paginação inferior e textos direcionais. Testes host/LVGL/sanitizadores e build P4 separados de aceitação física pendente da nova versão; não encerra toda a fase 9. URL/QR de canais e parâmetros LoRa permanecem futuros. Detalhes em docs/MESHTASTIC_0_14.md.


### Incremento e-mail 0.15.0 / API 14

Fase 10 parcialmente implementada: IMAP/POP3 e SMTP, portas editáveis, senhas de envio/recebimento independentes, TLS direto e STARTTLS, cache textual, rascunhos e fila offline com confirmação SMTP e reenvio incerto manual. Uma conta ativa, sem OAuth. Anexos e aceitação física de servidores permanecem pendentes. Detalhes e limites em [E-mail 0.15.0](docs/EMAIL_0_15.md). Usuário confirmou estabilidade/diagnóstico 0.14.5; Meshtastic pausado para outra ocasião.


0.15.2/API14: corrigida divergência entre relógio da interface e POSIX no ESP32; validação temporal dos certificados habilitada. Timeout de e-mail persistiu em duas redes, ainda não atribuído à data nem resolvido fisicamente. Diagnóstico passou a distinguir estado TCP/TLS e exibir data/IP/duração. Ver docs/RELOGIO_IMAP_0_15_2.md.


0.15.5/API14: transporte de e-mail mbedTLS direto com send/recv; teste real TLS1.2/saudação Hostinger no computador, sem credenciais, e testes locais de certificados, STARTTLS, tráfego e falhas. O usuário confirmou progresso da 0.15.4 até handshake, ainda com timeout físico. Não declarar IMAP validado na placa; ver docs/TLS_REAL_0_15_5.md.


0.15.7/API14: usuário confirmou nova parada no handshake com RX parcial, portanto avanço até protocolo na tentativa anterior não equivale a estabilidade TLS. Mitigação de rede b/g/n com confirmação RPC e diagnóstico do protocolo anterior/aplicado. Hipótese baseada em relato upstream, aceitação física pendente. Ver docs/WIFI_COMPAT_0_15_7.md.


0.15.8/API14: revisão ampla do e-mail (TLS, SMTP, limites IMAP, cabeçalhos de mensagens grandes, cache/conta, memória, ordem e MIME), com testes reais locais de TLS/IMAP/SMTP. A foto 0.15.7 não confirmou solução via b/g/n; perfil normal Wi-Fi restaurado. A falha física permanece para reteste, sem alegação de conclusão da fase 10. Ver docs/AUDITORIA_EMAIL_0_15_8.md.


0.15.9/API14: usuário confirmou que a OTA também para no estado DNS202; investigação passa a incluir caminho global de dados P4/C6. Recuperação assíncrona com proteção/cooldown e ação manual, HTTP reiniciado e informações IP/DNS. Backend simulado e build não comprovam recuperação física; não encerrar fase 10 por esse resultado. Ver docs/RECUPERACAO_REDE_0_15_9.md.

0.15.10/API14: recuperação com ciclo de limpeza/recriação do driver Wi-Fi, bloqueio de buscas após falha e preservação da intenção OTA. Host/build aprovados; fase 10 continua parcial até validação física. Ver docs/DRIVER_WIFI_0_15_10.md.

0.15.11/API14: confirmado pelo usuário ESP_FAIL em disconnect/stop/deinit, sem STA_STOP. Interrupção de repetidas tentativas, orientação de reinício confirmado e captura de avisos SDIO. Correção da causa de perda de tráfego e aceitação física permanecem pendentes; fase 10 parcial. Ver docs/TRANSPORTE_C6_0_15_11.md.

0.15.12/API14: corrigida recepção packet RX incompatível com streaming do firmware C6 oficial 1.4.1. Regressão usa funções reais do driver e reproduz rejeição de lotes agregados na configuração antiga. Build/distribuição exigem streaming. Estabilidade física ainda pendente; fase 10 parcial. Ver docs/SDIO_RX_0_15_12.md.

Revisão em 10/11/2026: [pendências por fase até a 10](docs/PENDENCIAS_ATE_FASE_10.md), diferenciando implementação, relatos já confirmados e ensaios físicos pendentes. OAuth continua adiado pelo usuário.

Entrega 0.17.0: [fases solicitadas e limites de validação](docs/FASES_3_4_6_7_8_10_0_17.md). Testes de host/build e coleta disponível não encerram critérios físicos.


Atualização 0.19.0: leitor HTML/CSS litehtml para e-mail/RSS, recursos no SD, pastas por ícones, repetição SMTP rejeitado somente por ação manual, gravação incremental de medições. Mapa launcher com dois slots OTA de 4 MiB. Ver docs/HTML_CSS_0_19.md para limites e validação; testes físicos desta versão pendentes.
