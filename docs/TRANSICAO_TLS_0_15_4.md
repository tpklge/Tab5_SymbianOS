# Transição TCP → TLS — 0.15.4 / API 14

A foto da 0.15.3 mostra relógios UI/POSIX iguais, IP remoto 172.65.188.64, estado ESP_TLS_CONNECTING e timeout após aproximadamente 30 segundos. Isso confirma TCP conectado, antes do início do handshake. Não é erro de autenticação nem evidência de falta de memória.

## Defeito localizado

O ESP-TLS do ESP-IDF 5.4.4 instalado inicializa rset/wset na entrada da conexão. No estado CONNECTING, select() pode retornar por timeout curto e esvaziar esses conjuntos. Uma chamada assíncrona posterior reutiliza os conjuntos esvaziados, sem reinicializá-los, e pode continuar retornando timeout mesmo quando o socket já conectou. O servidor IMAPS aguarda ClientHello e não envia saudação em texto simples antes do TLS.

A 0.15.4 verifica getpeername e SO_ERROR quando o estado é CONNECTING. Somente após confirmar TCP sem erro, desativa a espera adicional de select naquela chamada da máquina de estados. O descritor continua O_NONBLOCK, o handshake mbedTLS é executado e certificado/hostname/validade continuam verificados. Não muda biblioteca de terceiros nem usa estruturas internas. STARTTLS usa a mesma verificação após a negociação em texto claro.

Esse defeito afeta o caminho assíncrono usado pela 0.15.3. Não se afirma que explica todos os timeouts das primeiras versões, que usavam conexão síncrona. A conexão real com a conta continua pendente de aceitação no Tab5.

## Regressão

Novo teste mail_transport compila diretamente main/mail.cpp contra um simulador da API ESP-TLS e um socket real conectado não bloqueante no computador. A máquina simulada mantém CONNECTING preso com a espera antiga; o código deve verificar o TCP e avançar para HANDSHAKE/DONE em três chamadas, mantendo O_NONBLOCK. Também testa STARTTLS, cancelamento anterior à conexão, presença da configuração de certificados e recusa de avançar quando o TCP não foi verificado. Não conecta ao provedor nem reproduz o rádio C6.

18 suítes host e jornada LVGL passaram; testes mail e mail_transport com ASan/UBSan são registrados separadamente do build ESP32-P4. Nenhum flash, monitor, serial ou publicação executado. Atualize BIN e SD juntos.
