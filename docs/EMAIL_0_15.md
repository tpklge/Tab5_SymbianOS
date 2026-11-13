# E-mail — 0.15.0 / API 14

Cliente original para uma conta ativa, com IMAP ou POP3 no recebimento e SMTP no envio. Interface com os mesmos ícones, tema, navegação e rodapé dos demais aplicativos. Meshtastic permanece como na 0.14.5: o usuário confirmou diagnóstico abrindo no primeiro toque e ausência das telas azuis; a incompatibilidade de mensagens privadas fica para outra ocasião.

## Configurar

Abra **E-mail → Ajustes**. Em **Identidade / protocolo**, informe seu endereço, nome e escolha IMAP ou POP3. Em **Recebimento**, configure servidor, porta, TLS direto ou STARTTLS, usuário e senha. Em **Envio SMTP**, configure esses dados separadamente: a senha de envio pode ser diferente da de recebimento. Confirme cada formulário com OK e finalize com **Salvar conta**.

Portas comuns (qualquer porta entre 1 e 65535 pode ser informada):

| Serviço | TLS direto | STARTTLS |
| --- | --- | --- |
| IMAP | 993 | 143 |
| POP3 | 995 | 110 |
| SMTP | 465 | 587 |

Não há OAuth nesta versão. O provedor deve permitir autenticação por senha ou senha de aplicativo. Usuário de envio pode ficar vazio quando o servidor não exige autenticação. Não existe opção de enviar credenciais em conexão sem TLS. Certificados e nome do servidor são verificados; ajuste data/hora e conecte o Wi-Fi antes de receber/enviar.

**As credenciais ficam em `/SymbianOS/config/mail/account`, codificadas em hexadecimal, sem criptografia. Quem acessa o cartão pode lê-las.** Não compartilhe esse arquivo nem backups com credenciais. Senhas são mascaradas na interface e não entram na recuperação genérica de campos de aplicativos com falha. Artefatos de distribuição recusam configuração privada de e-mail.

## Uso offline e confirmação

- **Receber** baixa até dez mensagens ainda não armazenadas por sincronização, começando pelo lote mais recente. IMAP lista pastas; POP3 oferece apenas a caixa de entrada do servidor.
- **Pastas** reúne Entrada, Rascunhos, Saída, Enviados, Lixeira local e pastas IMAP. Mensagens baixadas podem ser lidas sem rede; leitura marca o item localmente.
- **Escrever** permite destinatários separados por vírgula, assunto e corpo multilinha. Salvar rascunho funciona offline. Responder prepara uma nova mensagem com trecho do texto anterior.
- **Enviar** guarda na Saída; abra essa pasta e toque em **Enviar fila**. O envio automático ao conectar Wi-Fi é opcional e desligado por padrão.
- Apenas a resposta SMTP final **250 após DATA** move a mensagem para Enviados. Isso indica aceitação pelo servidor, não entrega ao destinatário. A pasta Enviados é local; não há APPEND na pasta do provedor.
- Se a conexão cair durante DATA ou esperando sua confirmação, o estado fica **Envio incerto**. Confira o servidor antes de autorizar reenvio manual. Falhas e envios incertos não são reenviados automaticamente. O Message-ID permanece estável nas tentativas.
- Lixeira e exclusão atuam somente no cache local. O cliente não envia DELE, STORE de exclusão nem EXPUNGE ao servidor.

## Escopo e limites

Cache de até 100 mensagens, corpo recebido até 48 KiB, mensagem bruta até 128 KiB e composição até 16 KiB/4096 caracteres. Mensagens acima do limite bruto são rejeitadas com aviso. Assunto até 200 bytes e até dez destinatários. Conteúdo textual MIME em UTF-8/base64/quoted-printable e Latin-1; multipart prefere texto simples e converte HTML em texto sem executar conteúdo. Anexos não são baixados/exportados nesta etapa. Não há notificações push, múltiplas contas simultâneas ou espelhamento de flags no servidor. Portanto, este é um incremento da fase 10, não seu encerramento integral.

## Validação

Testes de host cobrem IMAP, POP3, SMTP, portas personalizadas, senhas distintas, negociação STARTTLS antes de autenticar, falha de TLS sem downgrade, MIME, cache/drafts offline, retomada da fila e envio incerto. A jornada LVGL verifica editor multilinha, rodapé e máscara da senha. Compilação ESP32-P4 e sanitizadores são registrados separadamente. Conexão com servidores reais, consumo e coexistência com áudio/rádio precisam ser validados no Tab5; não foi executado flash nem aberta serial.

Protocolos de referência: [IMAP](https://www.rfc-editor.org/rfc/rfc9051.html), [POP3](https://www.rfc-editor.org/rfc/rfc1939.html), [SMTP STARTTLS](https://www.rfc-editor.org/rfc/rfc3207.html).
