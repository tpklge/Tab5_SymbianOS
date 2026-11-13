# Conexão de e-mail cancelável — 0.15.3 / API 14

O usuário relatou que a 0.15.2 continuou sem conectar, com interface lenta, relatório aparentemente sem mudanças e relógio em 72. Não se atribui isso a arquivos copiados incorretamente. A conexão física continua sem confirmação.

## Alterações

- Transporte passa de esp_tls_conn_new_sync para esp_tls_conn_new_async, com socket não bloqueante, consultas curtas, prazo global de 30 segundos após entrada no transporte e pausas explícitas entre tentativas. DNS do lwIP ainda pode bloquear a tarefa de rede durante sua resolução; não bloqueia intencionalmente a interface.
- Esperas EAGAIN/WANT_READ/WANT_WRITE durante leitura/escrita também cedem execução ao escalonador. Antes, essas respostas podiam repetir sem pausa no protocolo.
- Cancelar somente sinaliza o trabalhador; não chama stop/join no caminho da interface. Encerramento do sistema mantém join para proteger a vida útil dos objetos.
- Mudanças de etapa TCP/TLS são publicadas enquanto a conexão ocorre, em tela e no log. Gravações finais de diagnóstico saem da região protegida pelo mutex consultado pela interface.
- O painel mostra versão do runtime e relógios UI/POSIX atuais, mesmo antes da primeira tentativa. Esses campos são consultados no runtime, não lidos de um log antigo.
- Antes da conexão nativa, a data do runtime é aplicada novamente ao POSIX e o retorno é verificado. Se inválida ou não aplicada, a conexão não começa e exibe erro específico. Preserva certificado, hostname/SNI e validação temporal.
- STARTTLS mantém o mesmo socket e aguarda a conclusão real do TCP não bloqueante antes de autenticar. Não repete INIT abrindo sockets nem permite enviar senha antes de TLS.

## Validação

Teste de host simula uma conexão que não termina até receber cancelamento: progresso consultável enquanto pendente, chamada Cancelar retorna sem esperar e trabalhador finaliza em seguida. Os demais protocolos, portas, senhas independentes, fila offline, MIME e estados incertos continuam cobertos. A jornada LVGL verifica os controles reais; ASan/UBSan cobre o teste de e-mail. Transporte ESP-TLS não é emulado por esses testes: seu código é compilado no build ESP32-P4 e ainda exige aceitação no Tab5/C6.

Não se declara que o IMAP Hostinger foi corrigido fisicamente. A entrega trata bloqueio/cancelamento, relógio aplicado e observabilidade. BIN e SD devem ser atualizados juntos. O diagnóstico deve começar por `runtime=0.15.3` e mostrar o relógio UI e POSIX atuais. Nenhum flash, monitor, serial ou publicação executado.
