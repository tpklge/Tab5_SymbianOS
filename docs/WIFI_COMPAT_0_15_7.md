# Recepção Wi-Fi e compatibilidade — 0.15.7 / API 14

A foto da 0.15.6 mostra timeout durante TLS, TX=183 e RX=2880, com WANT_READ (-26880), errno=11 e nenhum erro de certificado registrado. São dados de negociação parcial; certificado ainda não foi confirmado. A tentativa anterior chegou ao protocolo, mas esse avanço não foi consistente. Não se considera IMAP resolvido.

## Hipótese e fonte

Há um relato de recepção travada com ESP32-P4/C6 em 802.11ax, contornada permitindo somente b/g/n antes de conectar. O relato usa versões e hardware diferentes; não comprova a causa no Tab5. Referência primária: [Espressif esp-hosted-mcu #184, comentários de outubro](https://github.com/espressif/esp-hosted-mcu/issues/184). O usuário já testou hotspot: não se pede repetir a troca de rede como se ainda não tivesse sido feita.

## Implementação

Antes de esp_wifi_connect, o host aplica WIFI_PROTOCOL_11B|11G|11N e lê de volta o bitmap pelo RPC do C6. Se a aplicação ou confirmação falhar, a conexão informa o erro em vez de alegar que o perfil foi usado. Captura também o bitmap anterior e a versão do firmware C6.

Registro em `/SymbianOS/logs/wifi-transport.txt` e nos detalhes do diagnóstico de e-mail: `WiFi anterior=...; solicitado=b/g/n (7); C6 confirmou=7`. O bitmap representa protocolos permitidos, não prova qual modo havia sido negociado anteriormente. Se já era 7, esse perfil não terá removido um modo ax ativo e a hipótese perde força. Confirmar 7 valida o pedido ao C6, não o sucesso da recepção.

Essa entrega limita as conexões Wi-Fi do sistema a b/g/n. Preserva a negociação WPA2/WPA3 e as verificações TLS. Não regrava o C6, não muda parâmetros SDIO nem aumenta buffers/timers sem evidência. Depois de atualizar BIN e SD, desconecte/reconecte o Wi-Fi e tente receber, para aplicar e verificar o perfil.

## Validação e limites

Build ESP32-P4 confere as APIs remotas set/get_protocol e o vínculo do diagnóstico. 18 suítes host, jornada LVGL e testes de e-mail/TLS real com ASan/UBSan são registrados separadamente. Testes de host não executam o RPC no C6 nem confirmam que esse contorno resolve a recepção. O teste físico continua pendente; não se declara causa definitiva ou solução confirmada. Nenhum flash, monitor, serial, publicação ou atualização de firmware C6 foi executado.
