# Protocolo IMAP — 0.15.6 / API 14

A foto da 0.15.5 mostra `Tempo limite do servidor de e-mail`. Essa mensagem é gerada pela camada de protocolo, após open() retornar sucesso: o transporte concluiu TCP/TLS na tentativa em questão. Ainda não comprova autenticação nem recebimento. O diagnóstico anterior perdia a operação corrente e os contadores, o que impedia distinguir saudação, LOGIN, LIST, SEARCH e FETCH.

## Correções

O prazo anterior era global de 90 segundos, incluindo handshake e todas as mensagens, e expirava mesmo havendo tráfego. Agora há prazo de inatividade de 30 segundos renovado por bytes efetivamente lidos/enviados e limite total de cinco minutos para a sessão. O prazo de inatividade é reiniciado ao entrar no protocolo após TLS/STARTTLS, sem descontar a negociação anterior. Limites de corpo/cache e quantidade de mensagens permanecem.

Cada comando IMAP publica somente o verbo e tag (exemplo: `IMAP LOGIN T2 · aguardando resposta`, `IMAP UID FETCH T6`), sem argumentos. Timeout mantém essa etapa, estado e contadores TLS TX/RX, memória e relógio. Respostas NO/BAD também preservam etapa, sem copiar textos livres que possam conter dados privados. Não registra credenciais nem mensagens.

O limite global anterior pode interromper lotes lentos; a foto, isoladamente, não prova que essa foi a causa específica. Se o servidor permanece silencioso no LOGIN, a nova versão indicará isso em vez de esconder o ponto da espera. A autenticação da conta real continua sem confirmação.

## Validação ampliada

Além de TLS real, o teste local agora executa o protocolo IMAP de produção sobre a classe mbedTLS de produção: saudação, CAPABILITY, LOGIN com credenciais sintéticas, LIST, SELECT/UIDVALIDITY, UID SEARCH e UID FETCH de mensagem maior que 50 KiB em respostas fragmentadas. Um segundo cenário deixa o LOGIN sem resposta e verifica timeout identificado como LOGIN T2 com contadores e ausência da senha no diagnóstico.

18 suítes host e jornada LVGL, testes de e-mail/transporte com ASan/UBSan e build ESP32-P4 registrados separadamente. Os testes locais não acessam a conta real nem substituem aceitação via C6. Não houve flash, monitor, serial ou publicação. Atualize BIN e SD juntos.
