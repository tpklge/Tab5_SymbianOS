# Revisão de e-mail e transporte — 0.15.8 / API 14

## Leitura da foto

A 0.15.7 aplicou b/g/n (C6 confirmou 7), mas a tentativa terminou sem bytes recebidos pelo transporte TLS. WANT_READ (-26880) e EAGAIN (11) indicam espera de leitura; cert=0 não significa certificado validado quando o handshake está incompleto. Os 183 bytes TX foram aceitos pelo socket: não equivalem a uma captura provando entrega ao servidor. Os relógios UI/POSIX coincidem e não há erro explícito de alocação no relatório. Ainda não há causa definitiva do silêncio da conexão no Tab5.

O contorno exclusivo b/g/n não resolveu essa tentativa. A 0.15.8 restaura os protocolos normais b/g/n/ax (71), conferidos pelo C6, e concentra as mudanças na interoperabilidade TLS e em defeitos concretos do cliente. Nenhum firmware C6 foi regravado.

## Alterações e regressões

| Área | Problema encontrado | Tratamento |
| --- | --- | --- |
| TLS | Testes anteriores dependiam das opções padrão da biblioteca no computador, diferentes do firmware | Perfil explícito TLS 1.2, ECDHE RSA/ECDSA, AES-GCM 128/256, P-256/P-384, assinaturas RSA/ECDSA SHA-256/384; SNI e certificado obrigatório preservados |
| Recepção TLS | Registros/buffers grandes e ausência de verificação do ClientHello real nos testes | Pedido MFL de 1024 bytes, BIO send/recv de até 1024 bytes por chamada, TCP_NODELAY; teste inspeciona SNI, cifras, grupos, assinaturas e extensão MFL do ClientHello efetivamente enviado |
| Reconexão | Timeout de conexão podia exigir recomeçar manualmente | Uma reconexão de recebimento somente quando o transporte marca timeout anterior à autenticação; não repete recusa de LOGIN, erro de certificado ou envio SMTP incerto |
| SMTP AUTH | AUTH PLAIN era considerado inválido quando o servidor respondia 334 após a resposta inicial | Aceita o desafio e envia a resposta SASL; teste unitário e SMTP real sobre TLS |
| SMTP destinatários | Apenas RCPT 250 era aceito | Aceita também 251/252; sem alterar a exigência de 250 após DATA para considerar enviado |
| SMTP memória | Capacidades EHLO podiam acumular linhas grandes demais | Limite agregado de 64 KiB |
| IMAP caixas grandes | UID SEARCH ALL podia retornar mais que o limite de linha/índice | Quando SELECT fornece EXISTS, busca nas 200 sequências mais recentes; até dez mensagens novas por rodada |
| IMAP mensagens grandes | Fetch parcial podia ser salvo como mensagem completa | Consulta RFC822.SIZE; acima de 128 KiB baixa somente cabeçalhos e aviso, sem bloquear o restante do lote. O cache distingue cabeçalho de conteúdo completo. Um servidor que omita/contradiga o tamanho continua limitado pelo teste de 128 KiB + 1 byte, sem salvar truncamento silencioso |
| Ordem IMAP | Buscar mais novas primeiro e ordenar pela chegada podia inverter a lista | Ordenação por UIDVALIDITY/UID nas caixas IMAP, preservando mais recentes primeiro |
| Contas | Leitura por ID, contagem da fila e pastas podiam carregar dados da conta anterior | Filtragem pelo namespace ativo; pastas persistem identificador de conta; configuração volta a Entrada e limpa seleção remota |
| RAM | Resumos mantinham cabeçalhos excessivos que só a leitura detalhada usa | Subject/from/to/date compactados em fronteiras UTF-8; corpo e Message-ID não permanecem nos resumos; dados completos ficam no SD |
| MIME | Windows-1252 era tratado como Latin-1 | Conversão de euro, aspas e pontuação para UTF-8 |
| Texto grande | Limite de 48 KiB podia cortar conteúdo sem aviso adequado | Aviso explícito dentro do orçamento, após conversão MIME/HTML, sem cortar caracteres UTF-8 |

## Validação realizada

18 suítes host e jornada LVGL. Regressores cobrem AUTH 334, RCPT 251/252, conta/fila/pastas, ordenação, mensagens grandes, busca limitada, resumo compacto preservando dados completos, Windows-1252, limite visível do texto e reconexão pré-autenticação. Os testes garantem que polling automático não duplica um envio incerto e que LOGIN recusado não dispara nova tentativa.

Os testes de transporte usam mbedTLS real e o mesmo código de transporte/protocolo do firmware. O servidor local verifica o ClientHello real e exercita TLS válido, CA/nome inválidos, STARTTLS, 32 KiB fragmentados, cancelamento, prazo, fluxo IMAP completo, LOGIN sem resposta e SMTP com desafio SASL e confirmação perdida após DATA. Chaves e credenciais sintéticas existem somente em diretório temporário.

O perfil explícito também concluiu TLS 1.2, verificação do certificado e saudação IMAP em imap.hostinger.com:993 no computador, sem credenciais reais. Isso não comprova autenticação da conta nem o caminho P4/C6. Build ESP32-P4 e testes mail/mail_transport com ASan/UBSan são evidências separadas de aceitação física.

## O que permanece pendente

A falha principal no Tab5 precisa ser retestada; não é declarada resolvida apenas pelo resultado no computador. O pedido MFL depende da negociação com o servidor; mantém-se capacidade de receber registros normais se a extensão for ignorada. Servidores que exijam exclusivamente TLS 1.3 não são atendidos por esse perfil. Persistem os limites planejados: uma conta ativa, cache global de 100 mensagens, texto recebido até 48 KiB, composição até 16 KiB, sem OAuth e sem exportação de anexos. As senhas continuam no SD sem criptografia, conforme documentado desde a 0.15.0.

Atualize BIN e SD juntos e reconecte o Wi-Fi. O novo diagnóstico deve indicar runtime 0.15.8 e o perfil TLS/MFL. Nenhum flash, monitor, serial, publicação ou atualização do C6 foi executado pelo agente.
