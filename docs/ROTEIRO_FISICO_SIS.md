# Roteiro físico — SIS sem assinatura no M5Stack Tab5

Estado inicial deste documento: **não executado no hardware**. Atualização de relato: o usuário informou que os SIS se comportaram como esperado e que o pacote incompatível não apresentou Instalar; ainda faltam registros por caso e ensaios físicos de interrupção. Os resultados de host/build estão no relatório da entrega; eles não preenchem esta ficha. SISX, certificados e assinaturas não fazem parte deste roteiro.

## Preparação e registro

Use o Tab5 com teclado físico I²C oficial e um cartão de teste com cópia dos seus dados. Os ensaios de corte de energia, remoção de cartão e exclusão de dados devem ser feitos nessa cópia: eles podem interromper escritas reais no FAT. Reserve o cartão principal para depois dos testes básicos.

1. Pelo launcher, envie o **BIN de aplicação** e a pasta `sist/0.19.0/launcher/SymbianOS/`. Preserve a estrutura e os dados existentes ao mesclar a pasta. Não use bootloader, tabela de partições, erase ou flash por cabo.
2. Extraia `sist/0.19.0/Tab5-SIS-test-kit.zip`. Os SIS para envio estão em `packages/`; confira os hashes em `SHA256SUMS` do kit. As fontes estão em `sources/` e podem ser regeneradas com as ferramentas do SDK.
3. No SymbianOS, conecte ao Wi-Fi, abra Rede, ative o envio web por token e envie os SIS necessários para `home/Transfer`. Não envie os ZIPs ao instalador.
4. Anote a versão/hash do BIN, modelo e capacidade do cartão, alimentação/bateria, versão do launcher e identificação do teclado. Nenhum teste exige NVS.
5. Abra **Testes e medições** para registrar memória, PSRAM e duração no cartão. Registre valores antes/depois de cada atualização grande; o tempo de inspeção aparece em Detalhes técnicos. Não suponha uma meta de tempo: anote o valor observado.
6. Confirme que Notas, Wi-Fi, bloqueio por PIN e navegação básica funcionam antes dos testes. Encerre gravação de áudio/vídeo para não misturar alterações no cartão com a instalação.

Use Enter para acionar o botão selecionado, Tab/Shift+Tab para mudar o foco e Escape para voltar/cancelar uma operação nativa ocupada. Repita pelo menos um fluxo por toque. O indicador de processamento também aceita cancelamento por toque enquanto houver uma operação nativa em andamento.

Ficha de execução:

| Campo | Valor observado |
|---|---|
| Data / responsável | |
| BIN / SHA-256 | |
| Launcher | |
| Cartão / formato / espaço livre | |
| Teclado I²C | |
| Alimentação / bateria | |
| Heap / maior bloco / PSRAM iniciais | |
| Pendências ou aplicativos de teste já instalados | |

## Pacotes do kit

| Arquivo | Finalidade |
|---|---|
| `01-base-1.0.0.sis` | Dependência inicial válida |
| `02-counter-1.0.0.sis` | Contador dependente, somente dados próprios, esquema 1 |
| `03-counter-1.1.0.sis` | Upgrade com nova permissão Documents, esquema 1 |
| `04-counter-2.0.0.sis` | Migração para esquema 2 e preservação do contador |
| `05-counter-2.1.0-broken.sis` | Falha intencional na primeira abertura, depois de escrever 999 nos dados novos |
| `06-counter-0.9.0.sis` | Downgrade explícito em esquema 1 |
| `07-base-2.0.0-incompatible.sis` | Atualização que quebraria a dependência do contador |
| `08-missing-dependency.sis` | Requisito inexistente |
| `09-cycle-a-1.0.0.sis` | Primeiro componente do teste de ciclo |
| `10-cycle-b-1.0.0.sis` | Depende de A |
| `11-cycle-a-2.0.0.sis` | Tentaria fazer A depender de B, formando ciclo |
| `12-uid-collision.sis` | Outro ID usando o UID da base |
| `13-publisher-change.sis` | Autor declarado diferente para o mesmo ID/UID |
| `14-migration-conflict.sis` | Migração com destino já existente |
| `15-counter-2.2.0-large.sis` | Upgrade com recursos grandes, próximo do limite do perfil |
| `16-corrupted.sis` | Conteúdo corrompido |
| `17-truncated.sis` | Arquivo truncado |
| `18-zip-renamed.sis` | ZIP disfarçado de SIS |
| `20-retained-identity-change.sis` | Mesmo ID com outro UID para testar a proteção de dados retidos |
| `19-counter-2.3.0-slow-start.sis` | Primeira abertura com 32 escritas privadas de 64 KiB para o ensaio de reinício |

Os pacotes com falha são intencionais e originais do projeto. Envie inicialmente 01–04 e os casos necessários; use Anterior/Próxima no instalador para percorrer a lista.

## T01 — Revisão e dependência ausente

1. Antes de instalar a base, selecione 02 no Instalador.
2. Confira UID, autor declarado, versão, esquema, permissões e dependência. A interface deve informar dependência ausente/incompatível e não oferecer uma instalação válida.
3. Faça o mesmo com 08.
4. Verifique que nenhum contador apareceu no menu e que seus dados pessoais não mudaram.

**Esperado:** recusa sem ativação, reinício ou tela azul. O token de transferência não faz instalação automática.

## T02 — Instalação inicial e validação

1. Revise 01: deve aparecer **sem assinatura / autoria não verificada**. Abra Detalhes técnicos e registre hash e tempo.
2. Instale com Enter. Antes de abrir a base, tente instalar 02: o sistema deve pedir validação/cancelamento da tentativa pendente.
3. Use **Abrir / validar sis_base**. Volte ao Instalador.
4. Instale 02 e abra/valide `sis_counter`.
5. Pressione **Somar 1** cinco vezes, por teclado e toque. O valor deve ser **5**.
6. Volte ao menu, reabra o contador e reinicie normalmente pelo launcher/Ajustes. Reabra outra vez.

**Esperado:** base e contador aparecem no menu; contador continua em 5 após reabertura e reinício; nenhuma tentativa fica pendente depois da primeira abertura válida. A versão 1.0.0 não oferece exportação para Documents.

## T03 — Atualização, dados e permissão nova

1. Selecione 03. Confira versão atual 1.0.0 → 1.1.0 e **NOVAS permissões: documents**.
2. Cancele a revisão uma vez: a versão ativa deve continuar 1.0.0 e o contador, 5.
3. Revise novamente, confirme e abra/valide.
4. Confira versão 1.1.0 e contador 5.
5. Use **Exportar**. Em Arquivos, leia `/home/Documents/sis-counter.txt`.

**Esperado:** o arquivo contém `5`; a permissão só foi aplicada após confirmar a atualização. A versão/dados anteriores ficam retidos para rollback.

## T04 — Migração declarativa

1. Revise 04 e confira a mudança de esquema **1 → 2**.
2. Confirme e abra/valide. O contador deve continuar **5**, agora na versão 2.0.0.
3. Em Arquivos, confira `/data/sis_counter/`: `counter-v2.txt` deve conter `5`, `note.txt` deve existir e o antigo `counter.txt` não deve estar ativo nessa árvore.
4. Some mais duas vezes. O valor deve ser **7**.
5. Reinicie normalmente e confira versão/valor outra vez.

**Esperado:** migração na cópia, sem perda do contador. Snapshot anterior conserva o esquema 1 e o valor 5; árvore ativa conserva esquema 2 e valor 7.

## T05 — Falha na primeira abertura e recuperação automática

1. Instale 05. Antes de abrir, confirme que há uma tentativa pendente.
2. Abra/valide o contador. Esse pacote grava `999` na cópia nova e provoca erro Lua intencionalmente.
3. Volte ao menu/instalador e reabra o contador restaurado.
4. Confira `/logs/sis-lifecycle.txt` e registre a mensagem da interface.

**Esperado:** falha controlada; retorno à versão **2.0.0**, contador **7**, sem conservar o `999` da tentativa. A aplicação anterior abre normalmente e o Tab5 não reinicia nem entra em tela azul.

## T06 — Migração que deve falhar

1. Com 2.0.0 e valor 7, tente instalar 14.
2. A migração tenta renomear o contador para `note.txt`, que já existe.
3. Reabra o contador e confira os dois arquivos.

**Esperado:** recusa por conflito; versão 2.0.0 e contador 7 permanecem ativos. Não deve haver mistura de esquemas nem tentativa ativa incompleta.

## T07 — Cancelar uma tentativa pendente

1. Instale 15, mas ainda não abra a nova versão.
2. No Instalador, selecione **Cancelar tentativa pendente** e confirme a restauração.
3. Reabra o contador.

**Esperado:** versão 2.0.0 e valor 7. A versão grande não permanece ativa. Anote inspeção, duração, heap/PSRAM antes/depois.

## T08 — Rollback manual e efeito sobre dados posteriores

1. Em Detalhes de `sis_counter`, selecione **Restaurar 1.1.0** e leia a confirmação sobre os dados.
2. Cancele uma vez: ainda deve haver versão 2.0.0 e valor 7.
3. Confirme a restauração e abra/valide.
4. Confira **1.1.0**, esquema 1 e valor **5** — o snapshot foi feito antes dos dois incrementos na versão 2.
5. Some uma vez: valor **6**.
6. Use Restaurar novamente, agora para a versão 2.0.0, e abra/valide.

**Esperado:** versão 2.0.0 volta com valor **7** do snapshot correspondente; a versão 1.1.0 e seus dados 6 passam a ser a opção anterior. Exportações em Documents não são desfeitas pelo rollback.

## T09 — Downgrade explícito e compatibilidade do esquema

1. Estando em 2.0.0/esquema 2, revise 06. A confirmação de downgrade não deve permitir uma mudança silenciosa para esquema 1: sem migração compatível, a instalação deve ser recusada.
2. Restaure 1.1.0 pelo snapshot e abra/valide; valor esperado: **6**.
3. Selecione 06. Deve surgir uma segunda confirmação de downgrade.
4. Cancele: versão continua 1.1.0/valor 6.
5. Repita, confirme e abra/valide: versão **0.9.0**, valor **6**.
6. Restaure 1.1.0 e abra/valide para continuar os próximos testes.

**Esperado:** comparação numérica de versões, confirmação explícita e contrato de esquema respeitado. Um botão de confirmação não contorna dependências/migração.

## T10 — Dependência reversa, ciclo e identidade

1. Com o contador instalado, tente remover a base: deve ser recusado por dependente.
2. Tente instalar 07: deve ser recusado porque o contador aceita base 1.x, não 2.0.0.
3. Instale/abra 09, depois 10. Tente atualizar A com 11.
4. Tente remover A antes de B; depois remova B e A nessa ordem.
5. Tente 12: UID pertence à base e não pode ser reutilizado por outro ID.
6. Tente 13: mesmo ID/UID, autor declarado diferente; deve ser recusado sem alterar o contador.

**Esperado:** ciclo A → B → A e quebra de dependências são recusados. Nenhum caso altera a versão/dados ativos. A comparação do autor declarado não deve ser interpretada como assinatura/autenticação.

## T11 — Pacotes inválidos e vínculo da revisão

1. Selecione 16, 17 e 18, individualmente.
2. Para testar troca após revisão, faça uma cópia do kit: envie um SIS válido com um nome de teste, abra a revisão, depois substitua os bytes desse arquivo no cartão usando outro SIS de versão diferente (se a WebGUI não permite sobrescrever, use Arquivos para substituir o arquivo sem mudar o caminho). Só então tente confirmar a revisão antiga.
3. Faça uma nova revisão do arquivo final e confira o novo hash.

**Esperado:** corrupção, truncamento e ZIP renomeado não são aceitos. Troca dos bytes invalida a revisão antiga; não instala uma versão diferente sem revisão nova. Use apenas nomes/arquivos de teste nesse ensaio.

## T12 — Cancelamento durante operação longa

1. Com 2.0.0 ativa (restaure pelo snapshot ou reinstale em ordem, se necessário), selecione 15.
2. Durante a inspeção/instalação, pressione Escape ou toque no indicador de processamento. Repita em pelo menos três momentos.
3. Reabra o contador e volte ao instalador. Se o commit já ocorreu, use Cancelar tentativa pendente; depois reabra.

**Esperado:** antes do commit, versão/dados anteriores; depois do commit, tentativa nova completa e cancelável pelo fluxo de restauração. Nunca uma combinação parcial de arquivos ou dados. O cancelamento após a ativação não promete apagar uma instalação já efetivada.

## T13 — Espaço insuficiente

1. No cartão de teste, anote o espaço livre e a quota/reserva em Arquivos.
2. Copie arquivos descartáveis para reduzir o saldo abaixo do pico exigido para 15, sem apagar pastas do sistema ou documentos pessoais.
3. Tente a atualização e registre a mensagem.
4. Remova somente os arquivos descartáveis e tente novamente; cancele a tentativa resultante para manter a versão de referência.

**Esperado:** recusa por espaço físico/quota/reserva, sem ativar versão parcial. Depois de liberar espaço, a operação volta a funcionar. O limite de 4.000.000.000 bytes é global; não exige preencher todo o cartão para testar a reserva operacional.

## T14 — Remoção e reinstalação com dados preservados

1. Anote versão e contador atuais. Remova `sis_counter` com **Remover e manter dados**.
2. Confira que saiu do menu, mas os dados privados continuam em `/data/sis_counter/`. Documents deve permanecer intacto.
3. Tente 20 antes de reinstalar: deve ser recusado pelo marcador de identidade, mantendo os dados.
4. Reinstale uma versão de esquema compatível (03 para esquema 1; 04 para esquema 2), abra/valide e confira o contador preservado.
5. Agora remova com **Apagar app e dados** e confirme explicitamente.
6. Confira ausência de `/data/sis_counter/`, das versões e snapshots do aplicativo; o documento exportado permanece.
7. A base deve poder ser removida após remover o contador.

**Esperado:** retenção como padrão; exclusão somente após a escolha específica. Reinstalar um UID diferente com o mesmo ID não pode acessar dados retidos; o marcador de identidade/esquema impede a troca.

## T15 — Remoção do cartão e recuperação

Somente no cartão de teste e após T01–T14 aprovados.

1. Instale novamente 01–04 na ordem, abrindo cada tentativa. Prepare contador 2.0.0 com valor conhecido e anote-o.
2. Inicie atualização com 15. Em execuções separadas, retire o cartão durante a cópia/instalação e reinsira pelo fluxo de recuperação já existente.
3. Reinicie pelo launcher quando necessário, usando o mesmo cartão.
4. Verifique menu, versão, contador e log. Valide ou cancele qualquer tentativa completa apresentada.
5. Repita ao menos três vezes. Registre se a remoção ocorreu antes ou depois da mensagem de instalação concluída.

**Esperado:** erro de SD controlado, sem continuar a escrever em outro cartão. No retorno, versão anterior completa ou versão nova completa em tentativa. O journal não pode ativar dados de um cartão diferente. Se o FAT do cartão de teste estiver danificado, preserve uma cópia para diagnóstico antes de tentar reparação.

## T16 — Corte de energia na atualização e na remoção

Somente no cartão de teste. Repita cada caso pelo menos três vezes, variando o momento. O tempo da interface não revela com exatidão o ponto de commit; registre o que viu.

| Momento do corte | Verificação após reiniciar |
|---|---|
| Durante inspeção de 15 | Nenhuma ativação; versão/dados anteriores |
| Durante instalação de 15, antes de concluir | Versão anterior ou tentativa nova completa; sem mistura |
| Depois de concluir, antes da primeira abertura | Tentativa completa pode aguardar Abrir/validar ou Cancelar |
| Durante a primeira abertura de 19 | Tentativa iniciada é revertida; versão/dados anteriores reaparecem |
| Logo após a primeira abertura válida | Nova versão e snapshot anterior íntegros; finalização retomável |
| Durante Remover e manter dados | App ainda ativo ou removido; dados preservados |
| Durante Apagar app e dados | App ainda ativo antes do commit ou limpeza retomada após commit; dados do app removidos somente na segunda opção |

Para 19, comece em contador 2.0.0 com valor conhecido. O pacote faz 32 escritas de teste na primeira abertura para aumentar a janela do ensaio. Se a execução terminar antes do corte, ela já pode ter sido confirmada; nesse caso não conte a execução como teste de tentativa interrompida. Restaure o snapshot e repita. A simples aprovação do primeiro foco não demonstra estabilidade prolongada.

Após cada reinício, confira `/logs/sis-lifecycle.txt`, contador, menu e capacidade de abrir/fechar Notas. Repita o reinício sem nenhuma nova alteração: a recuperação deve ser idempotente.

## Registro por caso e critérios finais

| Caso | Resultado (passou/falhou/não executado) | Versão/valor antes → depois | Tempo / heap / PSRAM | Mensagem, log e observações |
|---|---|---|---|---|
| T01–T16, uma linha por execução | | | | |

Conservar fotos das mensagens, hashes do kit/BIN e cópia dos logs do cartão. Uma tela azul, reinício inesperado, perda de dados privados, alteração de documento compartilhado sem ação explícita, pacote parcial ativo ou ciclo de recuperação é falha. Interrompa a sequência, preserve o cartão de teste e registre o último pacote/ação.

Aprovação básica: T01–T14 completos por teclado, com pelo menos um fluxo por toque. Aprovação de recuperação física: T15/T16 executados separadamente e documentados; não substituídos por testes de host. E-mail, navegador e OTA de firmware não fazem parte deste roteiro; o launcher continua responsável pelo runtime.

## T17 — Envio de arquivos do próprio SymbianOS

O antigo Compartilhar foi renomeado para **Enviar arquivos**; não é OTA. Com Wi-Fi conectado no SymbianOS, ative o envio e use o endereço completo mostrado, incluindo `:8080`, com o token da sessão. Envie um SIS e toque Atualizar no instalador. Confira que o IP da rede continua sem porta no campo de conexão. Se falhar, registre a mensagem ESP_ERR completa: o erro do servidor não deve dizer simplesmente para conectar o Wi-Fi quando ela já está conectada. Encerre o envio e confirme a invalidação do token. Atualizações do sistema continuam na WebGUI do launcher.

## T18 — Coluna lateral e rolagem do instalador

1. Com os pacotes do kit no cartão, abra o instalador e confirme que Anterior, Próxima, Atualizar e Tema padrão ficam numa coluna estreita à direita, sem cobrir a lista.
2. Arraste a lista até o fim, volte por toque e repita com setas, PgDown/PgUp e Home/End. Confira barra e ausência de saltos para o início ao percorrer linhas.
3. Abra a revisão de um pacote mais abaixo e volte: posição e foco devem ser mantidos. Confirme que um pacote incompatível segue sem Instalar.
4. Se houver mais de 40 itens, avance/volte páginas e confira que não há duplicações ou omissões. Cópias de SIS com nomes diferentes podem ser usadas apenas para testar a lista; não precisam ser instaladas.
5. Envie outro arquivo por uma das WebGUIs e use Atualizar. Um arquivo `.SIS` deve aparecer; arquivos de outros tipos e pastas terminadas em `.sis` não devem aparecer como pacotes.

Registre esses dois casos separadamente dos testes SIS já informados. Validação em simulador não substitui teste de toque/teclado e envio real no Tab5.

Atualização do envio web: a sequência de falha na tarefa HTTP e ESP_FAIL na tentativa seguinte recebeu uma correção específica de pilha em PSRAM e limpeza de sockets. Antes de repetir T17, instalar os novos arquivos e reiniciar o Tab5. O roteiro complementar está em [HTTPD_TASK_ENVIO_WEB.md](HTTPD_TASK_ENVIO_WEB.md). Validação física dessa correção ainda pendente.
