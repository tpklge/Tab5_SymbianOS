# Desenvolvimento incremental

Seguir os critérios do plano_sistema_s60_tab5.md.

| Fase | Estado |
|---|---|
| 0 — runtime, raiz, quota, teclado | Base implementada e testada em host/build. Usuário confirmou boot/interface da versão anterior; bancada completa ainda pendente. |
| 1 — ambiente externo | Shell, home, menu, Notas, Calculadora e teclado virtual no SD; simulador compartilhado. Agora tema escuro e paisagem. |
| 2 — multitarefa e persistência | Gerenciador/LRU/checkpoints, falhas isoladas entre VMs, alternador, gerações de eventos, notificações, perfis e quotas Lua implementados. Testes de host disponíveis; validação física pendente. |
| 3 — organizador e arquivos | CRUD/importação/exportação, agenda/alarmes fora de foco, conversor, arquivos, quota, backup/restore com journal implementados. Limites documentados; bancada física pendente. |
| 4 — multimídia | Implementação 0.7.0: WAV/MP3/AU, fila de áudio, volume, gravador AU, galeria e prévia/foto/vídeo MJPEG. Sensor habilitado; aceitação física das correções pendente. Host/ASan/build separados; critérios físicos de passagem pendentes. |
| 5 — instalador e SDK | Implementação 0.5.0: ZIP STORED .t5pkg, validação de metadados/CRC/hash/limites, revisão de permissões, ativação transacional, remoção com dados opcionais, paletas e SDK externo. Validação de corte de energia e demais critérios físicos pendentes. |
| 6 — rede e extensões | Implementação 0.7.0: Wi-Fi/C6, transferência web por token confinada, downloader e títulos RSS/Atom. Testes de host/build; associação e quedas na placa ainda pendentes. Mensagens requer escolha de transporte. |
| 7 — energia, atualizações e recuperação ampliada | Pendente; há journal de documentos/restore e slots reservados, sem OTA próprio, rollback de releases ou deep sleep validado. |

A próxima aceitação é atualizar manualmente pelo launcher e testar rotação/toque, teclado, persistência, som/RTC, controle de rádio e falhas de cartão na unidade real, com dados de teste. O agente não executou flash nem abriu serial. Resultados desktop e de compilação não substituem medições físicas.
