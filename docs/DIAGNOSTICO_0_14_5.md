# Navegação e painel de diagnóstico — 0.14.5

O usuário confirmou runtime 0.14.4, rádio sincronizado (config 1/1), 51 respostas FromRadio, sem inválidos/perdas/eco, chaves locais informadas, modo não licenciado e região ANZ. O NAK 6 veio do próprio destino !04399e48. Portanto o transporte e a sincronização funcionaram nesse teste; não há evidência de UART desconectada.

Também relatou: Heltec com firmware 2.7.26 retorna channel/key mismatch enviando no sentido inverso; o botão de DM no Tab5-Meshtastic original não funciona, ficando para correção posterior nesse outro projeto. Não usar esse botão como prova de sucesso de DM no projeto de referência. A incompatibilidade bidirecional ainda não identifica qual chave/estado do firmware diverge. Não regeneramos identidades nem alteramos chaves.

O registro fornecido de Voltar terminou em lua-complete, com 185079 bytes internos livres, maior bloco 108544, PSRAM livre 27403548 e margem de pilha do worker 38184 bytes. Esse instante não sugere memória/pilha esgotadas, mas não é captura da causa do flash azul. Não foi fornecido um registro lua-fault correspondente ao flash.

## Correções de interface

- Toques em Diagnóstico/Voltar/Início/Menu/Abertos durante trabalho foreground passam para uma fila limitada, com deduplicação consecutiva. A versão anterior descartava esses toques, exceto cancelamento. O cancelamento explícito permanece e o dono da ação é conferido antes de executá-la. Não enfileiramos automaticamente todos os botões, evitando duplicar envios.
- Diagnóstico usa painel rolável e label, sem textarea, cursor de edição ou teclado virtual. Setas e PageUp/PageDown rolam o relatório. Exportação ao SD permanece.
- Contadores alteram o mesmo label/painel; não há reconstrução periódica da tela. O caminho reader genérico permanece para os outros leitores.
- A verificação de atualização pendente lê o estado compartilhado apenas após verificar que o worker está ocioso. A publicação da conclusão continua sincronizada por completed/join.

## Metadados do rádio

O relatório acrescenta versão do firmware e DeviceMetadata.hasPKC, recebidos de FromRadio.metadata. Não deduz suporte PKI apenas do tamanho das chaves gravadas. true aparece como sim (hasPKC), false explícito como não, e campo omitido como não anunciado (false/omitido). Metadados não recebidos aparecem como não informado. Esses indicadores são reinicializados na nova conexão e não alteram o rádio.

## Validação

16 suítes host e 16 ASan/UBSan passaram. Jornada LVGL reproduz uma conexão lenta e envia um único toque em Diagnóstico durante o trabalho: o painel abre após a conclusão, sem segundo toque. Verifica ausência de textarea, estabilidade da tela/label em três atualizações, rolagem por teclado e exportação. Protocolo testa metadados PKI independentemente da presença de chaves.

Build ESP32-P4 separado. Não há medição física da eliminação dos flashes. O NAK privado 6 não foi declarado resolvido; o botão do outro projeto não foi alterado. Nenhum flash, monitor, serial, publicação ou alteração de chaves/região.

Atualize BIN e SD juntos, release=0.14.5.
