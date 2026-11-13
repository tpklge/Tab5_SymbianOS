# Meshtastic — interface e canais 0.14.0 / API 13

O usuário confirmou envio e recebimento reais no RAK com a 0.13.3. A 0.14.0 preserva alimentação Grove, pinos e transporte; amplia a interface compartilhada RAK/BLE e a administração local de canais com implementação original.

## Uso

- **Mensagens:** histórico, destinatário, canal, rascunho e Enviar na mesma tela. Todos no canal envia broadcast; escolher um nó envia mensagem privada. Também é possível escolher Nós → nó → Privada. Conversas privadas e de canal são filtradas separadamente; registros antigos sem destino mantêm a classificação anterior como canal.
- Texto enviado e horário/status ficam alinhados à direita; recebidos à esquerda. Fonte dos balões e metadados mais compacta. A última página acompanha mensagens novas; páginas antigas permanecem acessíveis. Atualizações preservam texto, cursor e teclado touch. A mensagem fica limitada a 228 bytes UTF-8; o envio não sai da conversa.
- **Nós:** lista em ordem de nome e painel lateral com nome/apelido, último contato em formato brasileiro, SNR, saltos, bateria/tensão e posição/altitude. Selecionar um nó atualiza o painel na mesma tela. Dados não fornecidos aparecem como traço. NodeInfo, atualização de usuário, posição e telemetria alimentam o banco. Mantém até 128 nós; o seletor compacto mostra até 64 opções, e qualquer nó pode ser escolhido pela lista → Privada.
- **Anterior/Próxima:** centralizados na base, 168 × 54, como no Menu, sem cobrir o primeiro item. As listas têm seis itens por página e rolagem.
- A tela principal identifica o rádio pelo nome recebido no NodeInfo; se ainda desconhecido, informa que aguarda sincronização.
- **Canais → Ler do rádio:** consulta todos os oito slots. Novo canal ocupa um slot secundário desativado. Selecionar um canal abre o editor de nome/PSK; Salvar no rádio consulta o slot e obtém a autorização administrativa, envia a alteração e relê o canal para conferir. A chave é hexadecimal, com 2, 32 ou 64 dígitos; vazia significa sem criptografia. O nome tem até 12 bytes. O slot é conferido novamente para evitar sobrescrever um canal que ficou ocupado.
- É possível editar um canal existente ou desativar um secundário. O canal primário não pode ser desativado; o editor preserva sua função. Campos de configuração não editados, inclusive campos futuros desconhecidos, são preservados. Uma escrita aceita pelo transporte não é exibida como alteração confirmada.

## Protocolo e limites

Comparação somente de leitura com Tab5-Meshtastic, commit edafd39750a9a8bc3037d77bd11f26c097b0e11b: seleção de canal, mensagens diretas, painel de nós e fluxo administrativo consultar → autorizar → gravar → reler. Não foi copiado código da aplicação de referência. Campos do protocolo público conferidos nos schemas presentes nela: AdminMessage get_channel_request 1 (índice + 1), get_channel_response 2, set_channel 33, session_passkey 101; ADMIN_APP porta 6. A resposta deve corresponder ao request_id e vir do nó local (ou origem zero permitida pelo protocolo). Passkey fica apenas na memória durante a operação. Uma operação por vez, três tentativas de leitura com espera de quatro segundos, cancelamento por desconexão e sem retentar cegamente escritas.

A implementação não anuncia paridade completa com o cliente dedicado: importação/exportação de ChannelSet por URL/QR, leitura de QR pela câmera e alteração de parâmetros LoRa não foram incluídas neste incremento. A nova administração fica restrita aos canais do rádio local. Não muda região nem firmware. APIs/UI permanecem compatíveis com API 13, mas os novos layouts exigem atualizar BIN e SD juntos.

## Validação

Host: IDs, fila offline, deduplicação, ecos, detalhes de nós, nome local, destinatários privados, filtros, rascunho, administração com passkey, oito slots, rejeição de respostas de terceiros, proteção do primário e conferência positiva/negativa. Banco de 128 nós com nomes UTF-8 verifica limites dos seletores.

LVGL/SDL: balões com texto nas direções corretas, composição e envio na própria tela, rascunho preservado durante atualização e paginação abaixo dos nós. As [mensagens](preview-mesh-history-0.14.png), [nós](preview-mesh-nodes-0.14.png) e [canais](preview-mesh-channels-0.14.png) são prévias com dados sintéticos, não capturas físicas.

Build ESP32-P4 e sanitizadores registrados separadamente em BUILD.md. A aceitação física desta interface, mensagens privadas e criação/edição persistente de canais na 0.14.0 está pendente. Nenhum flash, monitor, serial ou publicação no GitHub foi executado.
