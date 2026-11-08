# Fases 2 e 3 — runtime 0.3.0 / release SD 0.3.0

## Interface

Tema externo em cinza escuro, texto claro e campos escuros. Paisagem 1280×720, com editor à esquerda, ações à direita, comandos permanentes abaixo e teclado virtual sobreposto. Ações extensas rolam no painel sem cobrir os controles comuns. A posição do cursor e a rolagem do editor são guardadas ao alternar. O relógio do painel atualiza sem depender da conclusão de uma cópia.

## Fase 2

- Estados Lua independentes, até três aplicativos residentes além de shell e serviços. LRU salva sessão antes de descarregar; falha de checkpoint impede descarregar um app saudável.
- Alternador lista apenas apps abertos, informa estado/memória e permite fechar. Apps encerrados retomarão a sessão versionada do SD. Não são processos Symbian nem programas SIS.
- Erros e orçamento de instruções não bloqueiam a saída para o menu. Apps com erro podem ser encerrados; quando possível, o texto é preservado em `/data/<id>/recovered-draft.txt`. Falha de persistência é informada.
- Fila de eventos limitada, associada à geração da VM: mensagens de uma instância fechada não são aplicadas à instância reaberta.
- Notificações persistentes e soneca. Perfil Normal permite avisos sonoros; Silencioso mantém avisos visuais; alarmes têm política audível separada. Offline solicita desativação do recurso Wi-Fi pelo BSP e persiste somente no SD. Isso não entrega rede, que segue na fase 6.
- 1 MiB por VM, 4 MiB totais de heap Lua e 100.000 instruções por callback, verificadas em lotes de 1.000. Operações de arquivos fazem streaming cancelável; não expõem VFS bruto aos apps.
- Worker de Lua/arquivos separado do LVGL; agendador nativo continua avaliando alarmes durante operações longas. Áudio de sinalização é serializado fora da tarefa gráfica. Entrada recebida durante o serviço de fundo mantém o foco de destino e é reaplicada em ordem.

## Fase 3

| Aplicativo | Funções entregues |
|---|---|
| Contatos | Criar, ler, editar, excluir com confirmação, buscar, importar/exportar vCard UTF-8 básico. Nome, telefone e e-mail. |
| Agenda | Calendário mensal, vista diária, compromissos/tarefas, conclusão, busca, CRUD, importação/exportação versionada e avisos fora da Agenda. |
| Relógio/alarmes | Ajuste de data/hora e fuso, alarmes únicos/diários, ativar/desativar, excluir, soneca de cinco minutos e leitura/gravação do RTC por adaptador nativo. |
| Conversor | Comprimento, massa, volume, tempo, velocidade e temperatura; dimensões incompatíveis e temperatura abaixo do zero absoluto são recusadas. |
| Arquivos | Navegar na raiz, pastas, leitura, propriedades, copiar, mover/renomear, excluir com confirmação e indicador de quota/ocupação por categoria. |
| Backup | Snapshot real do sistema/documentos/preferências, hash de integridade, cancelamento, restauração confirmada com staging integral e journal de roll-forward. |

Os avisos usam identidade e último vencimento entregues para não duplicar após ajustes do relógio. O histórico é reconstruído no boot sem tocar novamente o som. Durante uma cópia, o agendador entrega sinalização e a política em Lua confirma/persiste a entrega quando o worker volta a ficar disponível. Uma queda de energia nessa janela pode repetir um aviso; não há transação atômica entre alto-falante e FAT.

Backups excluem outros backups, cache, logs, temporários, updates e transações pendentes. A restauração valida todos os caminhos/hashes antes de alterar o destino, exige espaço para o pico total e preserva os arquivos extras que não estavam no snapshot. Depois de restaurar, reinicie para recarregar sistema, sessões e preferências restaurados. Não há atualização OTA ou rollback de releases implementado por essa função.

## Jornadas e formatos

- Contatos: Novo → nome/telefone/e-mail em linhas → Salvar. Importa `/home/Documents/contacts.vcf`; exporta para esse mesmo caminho. Propriedades extras de um vCard não são modeladas; o arquivo original é preservado.
- Agenda: Novo → título/data `AAAA-MM-DD HH:MM`/descrição/tipo (`tarefa` ou `compromisso`), uma linha por campo. Importa/exporta `/home/Documents/agenda.tsv`, formato `T5DATA1`, não iCalendar.
- Relógio: ajuste a data se ainda não houver um horário confiável. O RTC só é interpretado como UTC se houver o marcador criado após um ajuste bem-sucedido por este sistema. Se falhar o RTC, o ajuste da sessão e a falha de persistência são informados.
- Arquivos: linha 1 é a origem/caminho; linha 2 é o destino para copiar/mover. “Mover / renomear” aceita o caminho lógico completo de destino. Não sobrescreve destinos existentes; pastas só são excluídas quando vazias. `/boot`, `/system`, `/apps` e `/meta` ficam protegidos contra alteração comum; restore é uma capacidade específica.
- Backup: escolha um nome novo em `/backups/*.t5bak`. A restauração pede confirmação. Esc ou “Cancelar” interrompe antes do journal de commit; após esse ponto, a operação conclui ou é retomada no boot.

Registros e sessões usam `T5DATA1` com campos escapados. Notas mantém o documento TXT existente. Não há migração destrutiva dos documentos da release 0.1.0. Preferências públicas ficam em `/config/public/`, sem NVS.

## Limites e aceitação física

Até 256 contatos, 128 compromissos/tarefas e 128 alarmes por arquivo; arquivos estruturados até 64 KiB, editor até 16.000 caracteres, histórico de 32 notificações, 32 mensagens pendentes e 512 agendamentos nativos. Nomes de contatos/alarmes até 256 caracteres; descrição de compromisso até 2048. Backups até 2048 arquivos e a mesma quota total de 4.000.000.000 bytes, incluindo reserva operacional de 64 MiB. Essas são capacidades delimitadas deste marco, sem alegação de escala ilimitada.

Alarmes exigem o runtime ativo, cartão acessível, hora válida e alimentação. Não há promessa de despertar de deep sleep ou dispositivo desligado. Som, RTC, perfil Offline físico, sentido de rotação, toque e teclado real ainda precisam dos ensaios no Tab5. Testes de host e build ESP32-P4 não comprovam esses efeitos. O usuário relatou boot/interface do runtime anterior; isso não valida os novos caminhos de 0.3.0.
