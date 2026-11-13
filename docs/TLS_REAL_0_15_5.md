# TLS real no e-mail — 0.15.5 / API 14

A foto da 0.15.4 confirma relógios sincronizados e TCP conectado, agora no handshake TLS. O contorno de select permitiu avançar, mas não resolveu a negociação física. Não se atribui a falha a senha, certificado ou memória sem evidência.

## Implementação

O e-mail passa a usar um transporte original compartilhado entre firmware e teste host: `mail_tls.hpp`. Faz conexão TCP não bloqueante e handshake diretamente com APIs públicas mbedTLS, sem a máquina de estados ESP-TLS. Os callbacks de rede usam `send/recv`, com tratamento de retornos parciais/EAGAIN e pausas para o escalonador. Os conjuntos usados em select são reconstruídos a cada consulta. Preserva cancelamento, STARTTLS no mesmo socket, SNI/hostname, TLS mínimo 1.2 e certificado obrigatório. No ESP32, a confiança vem do bundle oficial esp_crt_bundle_attach e o relógio POSIX é aplicado/verificado antes de abrir a conexão.

O diagnóstico mostra `Transporte=mbedTLS direto / send-recv`, etapa e quantidade de bytes TLS TX/RX desde o ClientHello. Não grava conteúdo ou credenciais. Prazo padrão é de 30 segundos por fase TCP/TLS; DNS do lwIP ainda pode bloquear a tarefa de rede durante resolução. Continua sendo operação separada da interface.

## Validação ampliada

Os testes anteriores simulavam a máquina ESP-TLS. Foram substituídos por testes com mbedTLS real, compilado do código-fonte da dependência do ESP-IDF instalado. O firmware e o teste compilam a mesma classe de transporte, sem substituir seus sockets ou sua criptografia. No computador, o adaptador de confiança carrega PEM; no ESP32, usa o bundle binário do IDF. O build de host usa backends de criptografia do computador, não o hardware P4/C6.

Servidor TLS local real, com certificados e chaves temporários fora do SD, cobre:

- TLS 1.2 com CA válida e troca de 32 KiB fragmentados;
- rejeição de CA não confiável e nome de servidor incompatível;
- STARTTLS no mesmo socket;
- cancelamento de peer que não negocia;
- timeout curto injetado no teste, com contadores de diagnóstico.

Teste externo sem credenciais: conexão a `imap.hostinger.com:993`, TLS 1.2 com ECDHE-RSA-AES128-GCM-SHA256 (suportado na configuração ESP32), certificado verificado usando o conjunto de raízes do IDF, saudação IMAP `* OK`. Isso comprova a negociação desse transporte na rede do computador. **Não comprova autenticação da conta, leitura de mensagens nem operação via C6.** Não foi executado teste físico no Tab5.

18 suítes host e jornada LVGL, testes de e-mail/transporte com ASan/UBSan e build ESP32-P4 registrados separadamente. O teste real mbedTLS é opcional no host: requer o fonte em TAB5_MBEDTLS_SOURCE ou na instalação ESP-IDF; a configuração informa quando é omitido. Nenhum flash, monitor, serial ou publicação executado. Atualize BIN e SD juntos.
