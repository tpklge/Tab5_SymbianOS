> Revisão atual: [20261113-R2](NAVEGADOR_20261113_R2.md). Este documento descreve a revisão anterior.

# Navegador — revisão 13/11 R1

## Evidência física recebida

O usuário informou tela toda azul e reinício ao abrir Google, além de HTTP/TLS interrompido no Globo. O arquivo enviado mostrou:

- `stage=parse`, `html_bytes=226261`;
- `internal_free=112623`, `internal_largest=31744`;
- `psram_free=21260000`, `worker_stack_free_bytes=87260`.

Isso situa a última fase conhecida na construção do documento, com folga de heap e pilha naquele instante. `parse` incluía parser, criação do DOM e processamento inicial de estilos. Não há captura de panic para afirmar qual dessas operações falhou. `reset_reason=3` foi observado depois do retorno pelo launcher e não prova o motivo original da queda. A causa interna do motor no P4 continua sem confirmação.

## Mudança de caminho no navegador

Nesta revisão, o navegador **não entrega HTML/CSS remoto ao litehtml/Gumbo nem rasteriza a página em imagem**. Durante o recebimento, um extrator incremental próprio mantém buffers limitados, descarta scripts/estilos/comentários e recolhe título, texto e links. Prioriza `<main>` e omite menus, formulários e rodapés. O resultado é exibido no leitor nativo LVGL, em páginas de até 4 KiB, com Anterior/Próxima e lista de Links.

**O layout original, CSS e imagens das páginas ficam suspensos no navegador.** Isso evita o caminho em que a queda foi registrada; não é uma alegação de que o defeito interno do motor foi identificado e corrigido. Os outros aplicativos não tiveram seu renderizador substituído.

O texto é limitado a 32 KiB, os links a 128, URLs a 1024 bytes e títulos/rótulos a 160 bytes. UTF-8 inválido é substituído antes da exibição. Há tratamento de tags, aspas, entidades e sequências UTF-8 divididas entre leituras. Tags/atributos enormes são descartados com armazenamento limitado; scripts grandes não são acumulados em RAM.

O cache `T5B2` contém o resultado de leitura, não uma cópia integral do HTML remoto. A chave de cache mudou para impedir o uso acidental do formato anterior. Histórico/favoritos e demais dados do usuário permanecem no SD.

## Globo e limites de transporte

A resposta obtida no host de `https://globo.com/`, redirecionada para `https://www.globo.com/`, tinha **2.344.238 bytes**. O limite anterior de 1 MiB não comportava essa resposta inteira.

O navegador agora aceita até **4 MiB recebidos por tentativa**, extraindo o conteúdo em fluxo. Isso não cria um buffer HTML de 4 MiB nem um DOM desse tamanho. O tempo total HTTP do perfil móvel passa a 180 segundos por tentativa; o prazo sem progresso permanece em 30 segundos. Downloads genéricos/feeds mantêm seus limites. Respostas truncadas/interrompidas continuam sendo erros e preservam o cache anterior.

Isso corrige a incompatibilidade de tamanho observada no host; não confirma a causa de toda interrupção TLS na rede do usuário. Falhas restantes registram bytes e diagnóstico em `/SymbianOS/logs/browser-network.txt`, sem gravar URL ou conteúdo da página.

## Identificação e ícone

A tela de endereço mostra **Navegador · 13/11 R1**. O script exige o identificador nativo `20261113-R1`; se o BIN for antigo, orienta atualizar BIN e cartão juntos. Um BIN novo também recusa o caminho de renderização antigo do navegador. `startup.txt` inclui `browser_build` e o leitor registra `build=20261113-R1` / `stage=native-reader`.

O navegador recebeu a chave de ícone `browser`, usando **`icons/browser.png`**, o globo azul/verde já fornecido no projeto. A imagem não foi redesenhada; seu raster existente foi acrescentado ao atlas. O gerador e `icon-sources.json` incluem o mapeamento/hash. Os direitos dos assets fornecidos continuam preservados.

## Verificação

- **45/45 testes de host e smoke da interface passaram**, em `build/browser-safe-final-host.log`.
- O extrator foi testado com todas as larguras de bloco da fixture, entidades/UTF-8 divididos, seleção de conteúdo principal, scripts/formulários/menus, script acima de 3 MiB, limites de texto/links, cache inválido e ícone do atlas.
- As capturas do Google, G1 e Globo passaram pela integração Runtime/Lua/cache/leitor nativo no host. O Globo de 2.344.238 bytes gerou três páginas de leitura; log em `build/browser-safe-pages.log`. As páginas remotas não são distribuídas no aplicativo.
- Build ESP32-P4: `build/browser-safe-final-p4.log`.

Os testes de host não demonstram ausência de reset físico. Não houve flash, monitor, erase ou abertura de serial nesta tarefa. Persistência nova somente no SD; nenhum NVS, OTA interno ou SISX foi acrescentado.

## Roteiro no Tab5

1. Pelo launcher, enviar **BIN e pasta SymbianOS juntos** de `sist/0.19.0/launcher/` e reiniciar.
2. Antes de abrir páginas, conferir **13/11 R1** na tela do navegador e o novo ícone no menu. Se aparecer aviso de revisões diferentes, atualizar os dois itens.
3. Abrir `https://www.google.com.br/`, `https://g1.globo.com/` e `https://globo.com/`. Conferir leitura de texto, Anterior/Próxima, rolagem e Links. Não esperar o layout original nem formulário de busca interativo.
4. Cancelar uma tentativa e reabrir uma página já concluída sem rede. Uma falha de download não deve substituir o cache anterior.
5. Se houver nova queda, copiar imediatamente `logs/browser-render.txt`, `logs/startup.txt` e `logs/last-action.txt`. Se ocorrer erro de rede, copiar também `logs/browser-network.txt`. O novo marcador permite distinguir o leitor nativo da implementação anterior.
