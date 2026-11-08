# Validação — 8 de novembro de 2026 — versão 0.3.0

## Resultado

- `./tools/idf.sh build`: ESP32-P4, concluído sem erros. ESP-IDF 5.4.4, BSP 1.3.2, LVGL 9.5.0, Lua 5.4.9.
- `./tools/host.sh test`: **226 verificações passaram** (62 núcleo + 164 fases 2/3), além da jornada gráfica headless.
- UI exercitada em paisagem 1280×720: tema/field escuros, UTF-8, seleção e clipboard, alternância, entrada durante serviço de fundo, aviso sonoro simulado, teclado virtual por toque, salvar e recuperação/reentrada.
- Operações reais testadas: CRUD/busca/importação/exportação, checkpoints/LRU, falhas e fechamento de VM, gerações de eventos, perfis, alarmes fora de foco, soneca/dedup, cópia/movimentação/exclusão, backup/restore, hash/caminhos/aliases, journal e quota.
- ENOSPC é injetado no host; tamanho decimal de 4 GB também é auditado com arquivo esparso. Não se trata de cartão fisicamente cheio.
- Teste concorrente confirma que o agendador nativo vence enquanto a cópia mantém o filesystem ocupado. O probe de cartão pula filesystem ocupado, sem esperar a cópia e prender o polling do teclado.
- Migração de diretórios testada com documento anterior preservado; colisão arquivo/pasta recusa alteração destrutiva.

Logs: `host-validation.log`, `host-case-results.log`, `esp32p4-build.log`. Preview do simulador: [Notas em paisagem](preview-notes-landscape.png).

## Distribuição

| Arquivo | Bytes | Uso |
|---|---:|---|
| `dist/SymbianOS-runtime.bin` | 1222432 | Aplicação residente 0.3.0, API 1/2; upload OTA pelo launcher |
| `dist/SymbianOS-update-0.3.0.zip` | 311687 | Nova release, manifesto e diretórios; sem documentos/configurações para sobrescrever |
| `dist/SymbianOS-sdcard.zip` | 606335 | Instalação inicial completa |
| `dist/launcher/` | — | Árvore de atualização organizada para envio manual |
| `dist/runtime-installation/` | — | Partes da instalação independente; não são OTA de aplicativo |

A árvore fonte do SD ocupa 1201630 bytes lógicos, incluindo a release antiga preservada. A quota continua 4.000.000.000 bytes, com reserva operacional de 64 MiB dentro do teto. ZIP de atualização não leva documento, sessão ou preferência de usuário. Aplicativo e SD não são embutidos um no outro.

`dist/SHA256SUMS` verifica os artefatos. `dist/release.json` registra versão/API/tamanho. `tools/release.py` verifica versão e fontes mais novas que o BIN antes de empacotar; não conecta ao dispositivo.

O BIN cabe tanto nos slots de 2 MiB deste projeto quanto no limite histórico de 1600 KiB documentado no workspace. A tabela efetivamente instalada no launcher deve ser a referência ao instalar. O BIN de aplicação não altera essa tabela.

Layout independente deste build: bootloader `0x2000`, partições `0x8000`, otadata `0xe000`, aplicativo inicial `0x10000`. Não enviar essas partes pelo fluxo OTA de aplicativo do launcher. Não foi gerada imagem merged nem executado flash.

## Medidas estáticas do linker

- Soma de seções: 1222179 bytes.
- DIRAM estática: 172166 bytes.
- Dois slots de aplicativo de 2 MiB, flash configurada em 16 MiB, sem filesystem de usuário na flash.
- Lua aloca em PSRAM, até 1 MiB por VM e 4 MiB totais. Até três apps comuns residentes, além de shell/serviços. Worker pthread configurado com pilha de 24 KiB.

| Biblioteca | Flash atribuída | RAM estática atribuída |
|---|---:|---:|
| liblvgl__lvgl.a | 278961 | 66124 |
| libstdc++.a | 147390 | 4669 |
| liblua.a | 99883 | 0 |
| libesp_app_format.a | 98639 | 10 |
| libc.a | 97411 | 1132 |
| libplatform.a | 61603 | 32 |
| libesp_hw_support.a | 55350 | 19788 |
| libhal.a | 31495 | 16043 |
| libm.a | 27162 | 0 |
| libfatfs.a | 24208 | 64 |
| libesp_ipa.a | 23351 | 0 |
| libfreertos.a | 22117 | 23053 |

Relatórios completos: `size-summary.json` e `size-components.json`. Esses números não medem heap/PSRAM dinâmica, pilhas máximas, fragmentação, consumo ou latência real. Os limites são configuração; o cumprimento das metas de desempenho exige medições físicas.

## Evidência física

O usuário relatou que a versão anterior iniciou e exibiu menu, Notas e Calculadora, em retrato e tema claro. Isso é evidência parcial daquela versão. **0.3.0 ainda não foi instalada/testada pelo agente no Tab5.**

Continuam pendentes: sentido físico da paisagem, toque/teclado real, áudio de avisos, RTC offline, controle do rádio, remoção/reinserção e lotação real do SD, cortes de energia e medições de desempenho/memória. Alarmes exigem runtime ativo, cartão e alimentação; deep sleep/despertar e rede C6 são etapas posteriores.

Nenhum flash, monitor, erase-flash ou abertura de serial executado. Ver `ATUALIZACAO_LAUNCHER.md`, `FASES_2_3.md` e `TESTES_FISICOS.md`.
