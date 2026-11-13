# Relógio e diagnóstico IMAP — 0.15.2 / API 14

O usuário confirmou timeout ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT também no hotspot móvel, com TLS=0 e cert=0. Esse código pode ser produzido tanto por select durante conexão TCP quanto por expiração do prazo global durante negociação TLS. Portanto, o relatório anterior não permite afirmar que senha, relógio, certificado ou rede causaram o timeout.

## Correção concreta

Runtime::setClock mantinha apenas uma base interna usada pela interface; não aplicava a data ao relógio POSIX do ESP-IDF. time(nullptr) podia continuar em segundos desde o boot, como 171, apesar da barra exibir data correta. Agora, no ESP32, setClock também aplica a data UTC com settimeofday, tanto ao restaurar o RTC quanto ao ajustar a hora. No simulador, não altera o relógio do computador.

A validade temporal dos certificados passa a ser verificada pelo mbedTLS (CONFIG_MBEDTLS_HAVE_TIME_DATE=y). Na configuração anterior esse recurso estava desabilitado; por isso **não se atribui o timeout relatado à data incorreta**. O reparo garante coerência de hora entre interface, diagnóstico, data de envio SMTP e bibliotecas de rede. Data incorreta agora deve ser corrigida pelo usuário; não há relógio fictício, desativação de hostname ou certificado.

## Diagnóstico

O erro agora registra estado ESP-TLS, duração, servidor/porta, IP remoto quando TCP já conectou, códigos ESP/TLS/certificado, errno, memória e relógio Unix mais data UTC legível. Usa o estado público do ESP-TLS, sem ler estruturas privadas nem reconectar com credenciais em texto claro.

- `Etapa=DNS/TCP; TLS nao iniciado`: a conexão TCP não foi concluída.
- `Etapa=TLS; TCP estabelecido`: o socket conectou; a negociação TLS não terminou.
- Estado de falha exige ler os códigos ESP/TLS/certificado associados.

O relatório completo continua em E-mail → Ajustes → Diagnóstico e `/SymbianOS/logs/mail.txt`. Não registra senhas nem mensagens. Aguarda-se também o relatório completo da 0.15.1, para verificar servidor e porta realmente utilizados. Usar `imap.hostinger.com`, 993, SSL/TLS direto para a configuração de referência da Hostinger.

## Validação e limites

Testes de host, jornada LVGL e teste de e-mail com ASan/UBSan são separados do build ESP32-P4. O build verifica a configuração mbedTLS gerada e a integração settimeofday no alvo. As suítes host não medem relógio físico nem conexão via C6. O timeout continua em investigação até receber o estado real no Tab5; não se declara conexão reparada apenas por compilar. Nenhum flash, monitor, serial ou publicação executado.
