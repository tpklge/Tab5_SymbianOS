# Mensagens privadas — 0.14.1

O usuário validou o restante da interface 0.14.0 e relatou “falhou: rádio 6” ao enviar privada pelo canal aberto 0, onde o mesmo destinatário recebe mensagens públicas.

## Investigação

Routing.Error 6 é NO_CHANNEL. Não identifica sozinho a causa: o rádio local pode não encontrar um canal válido; ReliableRouter também envia esse NAK quando o destinatário não consegue aceitar/decodificar uma mensagem confiável. Na implementação de Router.perhapsDecode, um destinatário com PKI habilitado pode rejeitar texto privado legado mesmo após a decodificação por chave de canal. Portanto receber broadcast no canal 0 não demonstra que o destinatário aceita privadas legadas.

Fontes inspecionadas sem copiar código: [RAK Router.cpp](https://github.com/tpklge/RAK3172h-firmware/blob/a6641d3dc86b56bf1e9b714fc24b4b1c5f086231/src/mesh/Router.cpp), [ReliableRouter.cpp](https://github.com/tpklge/RAK3172h-firmware/blob/a6641d3dc86b56bf1e9b714fc24b4b1c5f086231/src/mesh/ReliableRouter.cpp). A [API oficial Python](https://github.com/meshtastic/python/blob/master/meshtastic/mesh_interface.py) também expõe solicitação PKI e chave pública no pacote. Esses caminhos explicam possibilidades do erro; não houve medição física que prove qual ocorreu no dispositivo do usuário.

## Correção

- Privadas solicitam explicitamente PKI (MeshPacket campo 17), usam campo channel zero para esse transporte e pedem ACK. Quando disponível no NodeInfo/User, a chave pública de 32 bytes é encaminhada no campo 16. O RAK continua responsável pela criptografia; o Tab5 não cria identidades novas nem altera configurações/chaves do rádio.
- Broadcast preserva o canal escolhido e não recebe flag PKI ou chave pública. O campo de canal na conversa passa a indicar que seleciona conversas públicas. Conversas privadas são filtradas por participante, independentemente do canal de transporte.
- O banco de nós acrescenta canal ouvido e chave pública hexadecimal (colunas 13/14), sem persistir chaves privadas. As leituras anteriores permanecem compatíveis.
- Falhas de roteamento mostram motivo, código e origem local/remota dentro do balão. Erro 6 permanece erro, sem ser ocultado por remoção de ACK ou convertido em sucesso. Erros 34/35/39 indicam problemas PKI/chaves de forma explícita.
- ACK do rádio local significa entregue ao rádio; não confirma recebimento remoto. Uma atualização tardia de fila não apaga uma falha terminal.
- Tocar no balão de uma mensagem falha recupera seu texto original no compositor; o usuário pode editar e enviar uma nova tentativa. Não existe retransmissão automática em outro canal ou fallback silencioso para texto privado legado.

## Limites e validação

Se o firmware/configuração do RAK não conseguir usar PKI ou faltar a chave pública do destinatário, o envio ainda pode falhar com indicação específica. Não há confirmação de correção física até testar esta versão. Não foram alterados firmware do rádio, região, PSKs ou chaves privadas.

16 suítes host e jornada LVGL passaram; regressão final Meshtastic e ASan/UBSan verificam campos PKI/chave/ACK/canal, preservação de broadcast, filtragem de DM entre canais, erro 6 remoto, ACK local sem confirmação remota, erro preservado após status tardio e edição manual sem reenvio automático. Build ESP32-P4 separado. Nenhum flash, monitor, serial ou publicação no GitHub.

Atualizar BIN e SD juntos; manifesto release=0.14.1.
