# Instalação e recuperação — 0.5.0

O caminho físico esperado é `/sdcard/SymbianOS`, apresentado aos apps como `/`. O cartão inicial suportado é FAT32. Não há formatação automática nem sistema alternativo embutido. Scripts, fonte, teclado e recursos são verificados no SD antes da execução.

Para atualizar uma instalação existente, siga [Atualização pelo launcher](ATUALIZACAO_LAUNCHER.md). Preserve documentos, sessões, configurações e a release antiga. O pacote de atualização não os substitui. Depois de validar raiz/manifesto/assets, o runtime cria somente diretórios ausentes para os novos apps; colisões com arquivos geram diagnóstico, sem apagar o arquivo.

Para instalação inicial, extraia `dist/SymbianOS-sdcard.zip` no computador e copie a pasta `SymbianOS` completa à raiz do cartão. Verifique antes com `python3 tools/package_sd.py --verify`. O pacote do SD é independente de `dist/SymbianOS-runtime.bin`.

## Falhas de boot/cartão

Sem cartão, pasta, manifesto, fonte ou release compatível, aparece apenas a recuperação residente. Enter ou “Tentar novamente” repete o boot. O driver tenta remontar cartões reinseridos; nenhuma aplicação retoma sem nova validação. Falha de inicialização do painel pode aparecer somente no log nativo, pois não existe tela utilizável para desenhar o diagnóstico.

Manifesto API 4 exige o runtime 0.5.0. O runtime antigo recusará esse manifesto. O runtime novo também aceita releases antigas de API 1/2/3, mas não há seletor automático/rollback de releases implementado. Faça alterações com o sistema parado e mantenha cópia do manifesto anterior junto ao backup da instalação.

Não interpretar o tamanho da tabela de partições do projeto como o layout que o launcher tem no dispositivo. O OTA de aplicativo usa somente o BIN de aplicação e o slot escolhido pelo launcher.

## Documentos e transações

Substituições usam `.t5-new`, sincronização e `.t5-old`. Sem journal de restauração, o boot recupera o anterior se o destino estiver ausente e descarta temporários incompletos. Com `meta/restore.plan` confirmado, valida todos os registros do plano antes de fazer roll-forward; não descarta os estágios necessários à recuperação. Só remove cópias antigas depois de instalar todos os destinos.

FAT não oferece garantia absoluta contra perda de energia. Use cópias/cartão de teste para esses ensaios. Se um estágio ou plano estiver corrompido, o sistema para em diagnóstico e preserva o material disponível, em vez de inventar uma instalação nova.

O backup `.t5bak` contém paths, tamanhos, conteúdo e SHA-256 de integridade. Não inclui outros backups, cache, logs, tmp, updates nem transações pendentes. Restore exige confirmação e espaço para a coexistência de originais + staging. Ele substitui arquivos de mesmo nome e preserva extras; reinicie ao concluir para recarregar sistema e configurações restaurados. Hash não autentica a origem de um backup.

Acima de 4.000.000.000 bytes, o shell é recusado em manutenção. Faça backup pelo computador e reduza o conteúdo com o sistema parado. Nenhum documento é apagado automaticamente. Em uso normal, Arquivos mostra quota/categorias e permite excluir dentro das permissões. A reserva de 64 MiB fica dentro do teto; espaço físico/metadata do FAT podem acabar antes da quota lógica.

Ao fechar app com erro, o gerenciador tenta preservar texto em `/data/<id>/recovered-draft.txt`. Confira esse arquivo e o documento original antes de decidir o que descartar. Um app saudável cujo checkpoint falha não é descarregado silenciosamente.

OTA próprio, confirmação/rollback de firmware, seleção de releases anteriores e suspensão profunda são da fase 7. Os slots estão reservados, sem essas funções anunciadas como concluídas.

Gravações WAV interrompidas ficam como `.part` em `/home/Recordings`; o boot tenta finalizar o cabeçalho e renomear sem sobrescrever. No instalador, somente o registro transacional ativa um payload; diretórios de versões incompletas não são executados. Reinstalar a mesma versão inativa limpa seu resíduo antes de tentar novamente. `/logs/last-audio.txt` contém o resultado do último sinal enviado ao driver.
