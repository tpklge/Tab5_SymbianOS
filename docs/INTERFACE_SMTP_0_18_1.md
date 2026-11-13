# Interface e SMTP — 0.18.1 / API 14

## Captura física autorizada

O usuário autorizou monitorar a serial do Tab5 conectado. O monitor usou /dev/cu.usbmodem101 a 115200, sem enviar comandos. Apesar de DTR/RTS desativados antes da abertura, o dispositivo registrou CHIP_USB_UART_RESET ao abrir a porta e voltou ao launcher; isso foi informado ao usuário. Após iniciar o Symbian e reproduzir as falhas, foram capturados quatro watchdogs: IDLE0 não recebeu CPU, CPU0 em taskLVGL, CPU1 em IDLE1. As amostras decodificadas com o ELF 0.18.0 preservado apontam para contagem UTF-8, quebra de linhas e cache de glyphs TinyTTF. Não foi capturado um panic/reboot do aplicativo após essas amostras. Evidência: serial-watchdog-0.18.0.log.

O código de lv_textarea_set_text da dependência LVGL 9.5 insere caractere a caractere se max_length está ativo. O editor do projeto ativava o limite antes de carregar todo o texto. Responder inclui até 2.000 caracteres da mensagem anterior; cada inserção repetia cálculos de comprimento/layout, compatíveis com as amostras do watchdog.

## Correções implementadas

- setEditorText pré-limita o texto por caracteres UTF-8, usa uma carga única sem limite temporário e restaura o limite antes de devolver o controle. Filtros de caracteres, como PIN, conservam seu caminho de validação. A digitação continua limitada. O teste confere carga de 2.000 caracteres multibyte, até dois eventos de mudança, truncamento válido e respeito ao limite/filtro após a carga.
- Atualizações periódicas de cartões de status alteram somente labels/barras que mudaram; a tela não é destruída/recriada. O simulador verifica quatro atualizações de heap com a mesma árvore LVGL.
- O botão OTA em Testes e medições fica centralizado abaixo dos cartões, sem cobrir o heap nem o rodapé. O teste confere sua posição. Os caches de fonte normal/compacta foram ampliados em PSRAM para reduzir trocas de glyphs.
- O watchdog não foi desativado nem teve seu prazo aumentado.

A nova versão não foi gravada na placa. A correção foi validada no host e no build ESP32-P4, mas a aceitação física pós-correção continua pendente.

## SMTP 535

O servidor recusou a autenticação, conforme informado pelo usuário; a versão instalada não imprime a resposta SMTP na serial, portanto não se afirma que o monitor capturou o 535. A montagem de AUTH PLAIN (incluindo resposta a desafio 334), a senha SMTP independente e STARTTLS têm regressões de protocolo. A mensagem agora identifica a autenticação recusada e aponta Conta → Envio SMTP, sem registrar usuário/senha ou a resposta bruta do servidor. Senha de envio vazia é recusada antes de conectar; não se substitui automaticamente pela senha IMAP/POP. O teste verifica 535 antes de MAIL FROM/DATA, resultado rejeitado e ausência de segredos no diagnóstico.

Corrigir a interface não torna credenciais recusadas válidas. Para Hostinger, conferir smtp.hostinger.com, porta 465 com TLS direto, usuário com endereço completo e senha da caixa/SMTP; salvar a conta após editar. A documentação oficial também lista 587 com STARTTLS como alternativa quando oferecida pela conta. A mensagem rejeitada permanece local para revisão; uma nova tentativa não deve ser anunciada como enviada sem confirmação SMTP.

Referências primárias: https://www.rfc-editor.org/rfc/rfc4954.html ; https://www.hostinger.com/support/1575756-how-to-get-email-account-configuration-details-for-hostinger-email/ .

## Regressão encontrada durante os testes

A nova release fez a árvore de instalação ultrapassar o limite antigo de 2.048 entradas do backup. O limite foi ampliado consistentemente para 4.096 no backup, restauração e recuperação, com diário limitado a 2 MiB. O formato não mudou; runtimes antigos recusam com segurança backups acima do limite antigo. A suíte de organizador confere backup/restauração real da árvore atual.
