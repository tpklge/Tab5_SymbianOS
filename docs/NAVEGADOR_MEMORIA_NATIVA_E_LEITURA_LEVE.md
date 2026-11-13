# Navegador: memória nativa, leitura leve e diagnóstico

**Revisão atual 13/11 R1:** o navegador usa extração em fluxo e leitor nativo, com CSS/layout remoto suspensos. As descrições anteriores abaixo são históricas. Veja [NAVEGADOR_20261113_R1.md](NAVEGADOR_20261113_R1.md).

Revisão de 12/11/2026, runtime 0.19.0. O usuário informou que o G1 continuava sem carregar e que o Google, após receber cerca de 60 KiB, deixava a tela inteiramente azul e reiniciava o Tab5. Não havia texto na tela. **Não foi capturado o reset físico nesta tarefa; a causa exata dessa queda ainda não pode ser declarada confirmada.**

A correção anterior do limite de elementos foi insuficiente: renderizar no host não demonstrava que a árvore completa cabia na memória do P4.

## O que mudou

### Páginas grandes passam a usar leitura leve

Depois de remover scripts/comentários, documentos com mais de 512 tags de abertura são convertidos para uma representação menor. O leitor prioriza `<main>`; na ausência dele, usa o corpo. Preserva texto, quebras e links, omitindo navegação global, formulários, scripts, estilos e conteúdo marcado como oculto. A interface identifica o resultado como **Leitura leve**.

A representação limita texto a 32 KiB, links a 128 e quebras a 192, preservando caracteres UTF-8 e fechamento de links. Quando o texto é cortado, informa **trecho da página**. O HTML completo continua no cache do cartão. Documentos pequenos mantêm HTML/CSS formatado. JavaScript e formulários interativos continuam sem implementação.

Permanecem o limite de entrada de 1 MiB, 8.192 aberturas HTML, profundidade máxima de 128, 32.768 nós e 256 páginas. A leitura leve não elimina essas proteções.

### Falta de memória deixa de entrar em caminhos de ponteiro nulo do parser

O Gumbo tinha alocações sem verificação de NULL. Um gerenciador próprio fornece as opções de alocação ao parser, limita a memória viva a 8 MiB e interrompe com segurança suas chamadas em C caso uma alocação falhe. Libera todos os blocos parciais e retorna falha para o tratamento C++. O salto de recuperação atravessa somente funções C do parser, sem pular objetos C++.

A rasterização de fontes também tinha um caminho que usava o buffer de scanline sem verificar NULL. O stb_truetype, compilado como C++ neste motor, agora recebe um alocador que lança erro recuperável e mantém a posse dos buffers temporários para liberá-los ao interromper a rasterização. Não foi alterado o arquivo da dependência instalada.

O renderizador e a preparação nativa da view tratam falta de memória. Uma falha não dispara repetidamente a mesma renderização: fica associada àquela revisão do documento, com diagnóstico; Recarregar ou uma nova navegação permite nova tentativa. A view de erro não mantém atualização periódica. Os alocadores HTML mantêm a reserva de 2 MiB de PSRAM; o build habilita uma reserva emergencial C++ de 4 KiB para conseguir construir exceções sob pressão de memória.

### Pilha e ciclo de atualização

O executor da interface passou de uma pilha interna de 48 KiB para uma pilha de 96 KiB em PSRAM, usando a API do ESP-IDF que permite definir capacidades de memória. Isso amplia a margem das chamadas nativas e deixa de reservar aquela pilha na RAM interna. A exclusão da tarefa usa a API correspondente. A opção de pilhas externas já era exigida pelo serviço Web; o empacotador também confere a reserva de exceções.

A página aberta não é mais relida e preparada a cada segundo quando está parada. Continuam as atualizações durante download e carregamento explícito de recursos.

### Resultado específico do download

O navegador não usa mais a frase global “Página salva no cartão” para decidir se seu pedido terminou. Cada pedido guarda resultado, progresso e erro próprios. Uma conexão Wi-Fi ou outra operação pode alterar a mensagem geral sem fazer o navegador descartar um download bem-sucedido. Uma tentativa recusada por rede ocupada preserva o pedido anterior.

### Diagnóstico no cartão

`/SymbianOS/logs/browser-render.txt` registra a última fase (`prepare`, `reader`, `parse`, `layout`, `links`, `draw`, `ready` ou `error`), quantidade de bytes HTML, heap interna livre/maior bloco, PSRAM livre e margem mínima da pilha do trabalhador. Não grava URL, conteúdo da página, senha ou token.

O arquivo é sobrescrito por fases/pedidos posteriores. Em caso de nova queda, copiá-lo pelo launcher **antes de abrir outra página**, junto de `logs/startup.txt`, `logs/last-action.txt` e `logs/lua-fault.txt`, quando existentes. O startup inclui o motivo do reset. O aviso de memória e o último estágio ajudam na investigação, mas não substituem a captura do panic/watchdog físico.

## Evidência de host e limites da medição

Foram obtidas respostas com HTTP/1.1, o User-Agent do firmware e `Accept-Encoding: identity`. A cópia do G1 desta rodada tem 939.117 bytes; a do Google tem 91.234 bytes, após redirecionamento. Respostas podem variar por rede, região e horário; não se assume que sejam idênticas aos 60 KiB relatados no Tab5.

| Captura local | Antes, formatação integral | Depois |
| --- | --- | --- |
| G1 | aproximadamente 78,4 MB em uso na fase final | aproximadamente 4,7 MB, leitura leve, 3 páginas |
| Google | aproximadamente 4,0 MB | aproximadamente 4,0 MB, formatado, 2 páginas |

Esses números são de **memória alocada do processo de teste no macOS de 64 bits**, obtidos por `malloc_zone_statistics`; não são pico de RAM, RSS, consumo de PSRAM ou tempo medidos no P4. O resultado demonstra que a versão integral do G1 era imprópria para validar apenas pelo sucesso no host e que a representação menor reduz fortemente o custo. Logs: `build/browser-g1-memory-host.log`, `build/browser-g1-memory-light-host.log`, `build/browser-google-memory-light-host.log`.

**Validação executada:** 44/44 testes de host e smoke da interface passaram (`build/browser-native-final-host.log`). A integração completa Runtime/Lua/cache/imagens também abriu as capturas do Google e G1 no host (`build/browser-native-final-extra.log`). O build ESP32-P4 está registrado em `build/browser-native-final-p4.log`.

A suíte verifica falhas de alocação no parser, no desenho de fontes, recuperação sem vazamentos, rejeição por orçamento, conteúdo malformado/profundo, memória insuficiente sem repetição automática, seleção de `<main>`, ocultação de menus, links e corte de UTF-8. O teste do navegador injeta outra operação de rede entre o download e a atualização da tela. As capturas reais passam pelo mesmo motor, mas isso **não é validação física do Tab5**.

Durante a validação, foi corrigido um caso de teste SIS não determinístico: o ZIP inválido do kit usava a hora corrente. Ele agora usa uma data fixa, sem mudar sua condição de pacote recusado nem o instalador.

## Teste físico prioritário

1. Enviar novamente **BIN e pasta SymbianOS juntos**, de `sist/0.19.0/launcher/`, pela WebGUI do launcher. Usar o SHA256SUMS que acompanha esta entrega para identificar o BIN. Reiniciar.
2. Abrir `https://www.google.com.br/`. Deve renderizar ou apresentar erro legível, mantendo a aplicação utilizável. O navegador não implementa envio do formulário de busca.
3. Abrir `https://g1.globo.com/`. A página grande deve apresentar **Leitura leve**, texto e links. Percorrer Anterior/Próxima e abrir uma notícia.
4. Usar Voltar/Recarregar e alternar entre páginas, verificando que o conteúdo estável não provoca atualizações periódicas pesadas.
5. Se voltar a ocorrer tela azul/reinício, copiar imediatamente os logs indicados acima pelo launcher. O último estágio diferencia recepção/preparação, parser, layout, desenho e apresentação posterior.

Nenhum flash, erase, monitor ou acesso serial foi executado pelo agente. Toda a persistência acrescentada fica no cartão. Não há gravação NVS, OTA interno nem SISX.
