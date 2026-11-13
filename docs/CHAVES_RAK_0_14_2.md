# Criptografia delegada ao RAK — 0.14.2

A 0.14.1 não resolveu o envio privado no hardware: o usuário relatou erro de criptografia/sem canal. A investigação comparou o projeto Tab5Meshtastic e o firmware tpklge/RAK3172h-firmware, commit a6641d3dc86b56bf1e9b714fc24b4b1c5f086231. Esta versão retira a imposição PKI introduzida na 0.14.1 e acrescenta diagnóstico sem valores de chaves.

## Alteração

O cliente envia texto decodificado, destino e canal solicitado, deixando o RAK selecionar a criptografia. Não envia MeshPacket.pki_encrypted nem MeshPacket.public_key e não substitui o canal selecionado por zero. A chave pública em cache no SD não é usada para sobrescrever a seleção do rádio. Broadcast mantém o mesmo comportamento.

A delegação corresponde ao codificador do Tab5-Meshtastic original. Preservamos o pedido de ACK das privadas, acrescentado no Symbian, para continuar observando respostas reais; não ocultamos erro 6 desligando a confirmação. A fila offline, os balões, a edição de uma mensagem falha e a distinção entre ACK local/recebimento remoto permanecem.

## Diagnóstico

Meshtastic RAK/BLE → Diagnóstico mostra:

- Criptografia decidida pelo rádio.
- Presença da chave pública local e da chave privada quando informadas nos frames de configuração.
- Modo licenciado informado pelo User do nó local e região (código) informada pelo rádio.
- Último destino/canal solicitado e último retorno de roteamento, com código e origem local/remota.

O parser descarta os valores de configuração de segurança após extrair os indicadores. Apenas textos de presença são persistidos no status; nenhuma chave privada é gravada ou exibida. Campo omitido é “não informada”, não prova de chave ausente. Campo vazio, de tamanho incorreto ou composto por zeros é “ausente ou inválida”. “Presente” significa que recebemos 32 bytes não nulos, não uma validação criptográfica do par ou de sua persistência no RAK. Ao conectar novamente, os indicadores e a identidade anterior são reinicializados.

As chaves privadas continuam no rádio, em /prefs/config.proto. PSKs de canais ficam em /prefs/channels.proto e as chaves públicas remotas no banco /prefs/nodes.proto. O app não regenera chaves, não altera o modo licenciado nem modifica região/firmware do RAK.

## Validação e limites

Host: 16 suítes e jornada LVGL; regressão Meshtastic sanitizada. Testes de pacote garantem que uma chave pública em cache não força PKI, que o canal solicitado é preservado e que ACK local não confirma entrega remota. Frames sintéticos de segurança verificam presença/omissão/zeros, ausência de valores no diagnóstico e ausência da chave privada em todos os arquivos de /data/mesh.

Build ESP32-P4 separado. Nenhum teste físico, flash, monitor, serial ou publicação. A entrega privada não está validada nesta versão. Se o erro persistir, o novo diagnóstico deve ser usado para distinguir recusa local, PKI indisponível e recusa do destinatário; a alteração não promete corrigir por si só a configuração de criptografia do rádio.

Atualize BIN e SD juntos, manifesto release=0.14.2.
