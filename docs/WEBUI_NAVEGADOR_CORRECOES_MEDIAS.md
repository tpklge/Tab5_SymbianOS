# Navegador, transferência Web e seis correções médias

Entrega 0.19.0, atualizada em 12/11/2026. Instalação somente pelo launcher: enviar o BIN e a pasta SymbianOS juntos. O firmware não usa NVS para estes dados nem oferece atualização OTA própria. A distribuição continua exclusivamente em `sist`, com 0.19.0 e 0.18.1.

## Alterações

- Navegador: HTML de até 1.048.576 bytes, em vez de 60.000. O Lua guarda o caminho do cache; a leitura do documento fica no código nativo. A URL final determina a base dos links. Permanecem os limites de 8192 elementos de abertura (scripts e comentários excluídos), profundidade máxima de 128 e até 32768 nós, 256 páginas e os controles de memória/tempo do renderizador; páginas complexas ainda podem exceder esses limites. JavaScript e sessões de login não foram acrescentados. A gravação de uma nova página antecede a remoção de entradas antigas; falhas preservam o cache anterior. Falhas na navegação não avançam o cursor do histórico.
- WebUI: interface responsiva, lista compacta, tamanho dos arquivos e ações Baixar/Apagar. Exclusão exige confirmação no computador, token válido e arquivo na pasta compartilhada; não remove diretórios.
- Envio: seleção de vários arquivos, fila sequencial, progresso individual pelos bytes confirmados no SD, estados de sucesso/falha/cancelamento e indicação dos arquivos ainda não enviados. O histórico permanece enquanto a página estiver aberta, inclusive após atualizar a lista. Recarregar/fechar a página descarta esse histórico; não há promessa de retomada automática após recarga.
- Mantidos limite de 64 MiB por arquivo, blocos de 64 KiB, repetição idempotente do último bloco e preservação de arquivos existentes. Nomes da WebUI têm até 100 caracteres ASCII, compatíveis com o limite FAT do projeto. Cancelar descarta somente o parcial da sessão; se o destino já tiver sido confirmado no SD, ele permanece. Falhas ao encerrar a sessão ficam visíveis e interrompem a fila.
- Exclusão e download são coordenados com os envios. Downloads têm tamanho inicial delimitado e verificam a geração do cartão. Exclusões verificam a geração dentro do bloqueio de armazenamento.

## Itens médios recuperados da auditoria do chat

| Item | Correção | Regressão de host |
| --- | --- | --- |
| 9 — rascunho perdido em Pastas/Ajustes | Salva a composição antes dessas transições e de outras ações que a substituem; falha impede navegação. | `mail`: campos preservados e falta de espaço bloqueando saída. |
| 10 — byte extra em anexo MIME | Remove exclusivamente o CRLF introdutório do delimitador; valida a linha de boundary. | `mail_attachments`: binary, 7bit, quoted-printable, conteúdo terminado em CRLF e prefixo de boundary dentro dos dados, além dos testes Base64 existentes. |
| 11 — exclusões antigas após reinserção/restauração | `Mail::stop` limpa tombstones e caches de pastas/conta para recarregar o cartão atual. | `mail_attachments`: mesmo objeto para, perde o arquivo de exclusões e sincroniza novamente. |
| 12 — suspensão durante gravação/finalização | Recusa áudio gravando, pausado/finalizando e filmagem. Antes de pausar o SD, espera os trabalhadores de áudio e câmera estacionarem; libera após remontagem. | `media_response`: recusa gravação/pausa/filmagem e estaciona antes de pausar/remontar o SD. |
| 13 — DNS do e-mail não cancelável | Resolução em um único trabalhador persistente, com resultado de vida independente do socket. Espera observa cancelamento/prazo; uma consulta tardia impede novas consultas até terminar, evitando acumular tarefas. | `mail_dns`: DNS bloqueado, cancelamento, prazo, tentativa concorrente e conclusão tardia. `mail_transport`: integrações TLS reais no host. |
| 14 — offset maior que 2 GiB | No P4, usa `f_lseek`/FSIZE_t do FATFS para leitura/patch, sem conversão para long assinado. Tamanho e contabilidade normalizam o campo assinado da VFS FAT; volume configurado na montagem/reinserção. Host usa fseeko/ftello. | `high_severity`: arquivo esparso acima de 3 GiB, leitura, patch, limite do arquivo e quota. Build P4 compila o caminho FATFS real; isso não substitui teste físico com arquivo grande. |

## Validação e limites

Resultado: 40/40 testes de host e smoke da interface passaram; build ESP32-P4 concluído.

Logs locais: `build/web-medium-host.log` (testes e smoke), `build/web-medium-p4.log` (build ESP32-P4). A suíte inclui páginas de 100 KiB, 512 KiB e aproximadamente 1 MiB, recusa acima de 1 MiB e preservação do cache em falhas; JavaScript real da WebUI com envio de 30 MiB, timeout/perda de confirmação, fila, cancelamento, token, download, exclusão e retenção dos resultados na tela.

O usuário informou que a WebUI anterior abriu corretamente e que enviou um arquivo simples. Esses são resultados físicos relatados pelo usuário, anteriores a esta entrega. As mudanças deste documento não foram instaladas nem testadas fisicamente pelo agente. Não foi aberto monitor serial nem executado flash/erase.

## Roteiro físico desta entrega

1. Pelo launcher, enviar `sist/0.19.0/launcher/SymbianOS-runtime.bin` e o conteúdo de `launcher/SymbianOS/`. Reiniciar e conectar o Wi-Fi; ativar Enviar arquivos. Abrir novamente a página, para carregar a interface nova.
2. Tentar listar sem token/com token errado; deve recusar. Informar o token atual; listar e conferir pasta/tamanhos. Enviar três arquivos de nomes diferentes, incluindo um maior que 60 KiB. Conferir barras individuais até 100%, estados finais e arquivos no cartão.
3. Atualizar a lista: os resultados anteriores devem continuar na tela. Baixar os arquivos e comparar SHA-256 com os originais no computador. Cancelar uma exclusão na confirmação; depois confirmar e verificar que somente o escolhido desapareceu. Tentar substituir um nome existente: deve informar o conflito e preservar o original.
4. Enviar um arquivo grande e cancelar durante o envio. Deve manter o resultado cancelado com bytes confirmados; os arquivos seguintes devem ficar como não enviados. Selecionar outro arquivo e enviar sem reiniciar o serviço. Se a limpeza falhar por perda de Wi-Fi, deve informar isso; reabrir o serviço antes de repetir.
5. Desligar temporariamente o Wi-Fi do computador durante outro envio. Conferir repetição de blocos ou erro explícito, sem falso sucesso. Restaurar rede e conferir a lista/arquivo antes de repetir, pois a confirmação final pode ter sido perdida. Conferir que outros resultados continuam visíveis. Acima de 64 MiB, vazio ou nome inválido deve ser recusado antes de transferir.
6. No navegador, abrir páginas controladas com HTML de 100 KiB, 512 KiB e até 1 MiB, incluindo links relativos e redirecionamento. Reabrir sem rede para testar cache. Tentar resposta acima de 1 MiB: deve explicar o limite e preservar a página anterior. Usar páginas pouco complexas para distinguir tamanho de limites de elementos/memória.
7. No e-mail, escrever destinatário/assunto/corpo; abrir Pastas e Ajustes em tentativas separadas. Voltar a Rascunhos e conferir os campos, inclusive após reiniciar. Com cartão sem espaço, sair da composição deve falhar mantendo o texto. Conferir anexos de conteúdo conhecido, incluindo binary/7bit, e comparar hashes ao baixar/exportar.
8. Testar e-mail com DNS indisponível: cancelar durante “Resolvendo DNS” e verificar que a interface responde. Repetições durante consulta antiga podem informar “DNS anterior ainda em andamento”; depois da conclusão do resolvedor devem funcionar. Não é necessário remover o cartão enquanto há escrita para testar cancelamento.
9. Iniciar gravação de áudio, pausar e solicitar suspensão em cada estado; deve recusar. Parar, aguardar finalização, conferir arquivo reproduzível e suspender. Repetir com filmagem. Retomar deve manter o cartão disponível e novos arquivos utilizáveis.
10. Em cartão de teste, respeitando a quota total de 4 bilhões de bytes e a reserva, usar arquivo conhecido acima de 2 GiB para conferir tamanho, reprodução/leitura por offsets avançados e integridade. O envio Web continua limitado a 64 MiB, portanto esse arquivo deve ser preparado diretamente no cartão. Não considerar o teste esparso do host como validação FAT física.

A restauração/troca de cartão que exercita o item 11 deve ser feita com cópia de teste e serviço encerrado, comparando a lista de e-mails ao estado restaurado. A regressão automatizada cobre a recriação do cache; nenhum cartão do usuário foi manipulado.
