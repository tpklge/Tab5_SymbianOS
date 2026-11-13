# HTML e imagens no e-mail e RSS — 0.16.0 / API 14

O recebimento de e-mails após a correção SDIO 0.15.12 foi confirmado pelo usuário. Esta entrega acrescenta leitura visual adaptada ao Tab5. Não altera o modo streaming nem a validação TLS.

## E-mail

Abra uma mensagem com HTML para ver o documento em painel rolável. Há parágrafos, títulos, listas, citações, destaques, sublinhados, identificação visual de links e alinhamento simples. Tabelas são linearizadas por linha/células para caber na tela; layouts de newsletter não são reproduzidos pixel a pixel. Texto alterna para a leitura textual.

MIME preserva a alternativa HTML e até seis imagens incorporadas por Content-ID (`cid:`), além do corpo textual alternativo. Essas imagens são salvas junto à mensagem e aparecem offline. Suporta JPEG, PNG e primeiro quadro GIF; data URLs base64 desses formatos também podem ser exibidas dentro dos limites. Imagens externas não são solicitadas ao abrir a mensagem: use Imagens externas para carregá-las. O carregamento é assíncrono, sem enviar credenciais IMAP/SMTP ao servidor de imagens.

Mensagens recebidas pelas versões anteriores não possuem o HTML/imagens que foram descartados. Abra a mensagem e toque em **Baixar novamente**; ao terminar, abra-a pela lista. A atualização IMAP usa a identidade UID/UIDVALIDITY da mensagem, inclusive fora do lote recente, sem criar uma cópia local. Uma mudança de geração da caixa é recusada em vez de buscar outro UID com o mesmo número. POP usa UIDL. Mensagens antigas no formato de cache de 13 campos continuam legíveis.

## RSS/Atom

Artigos preservam conteúdo HTML, CDATA ou conteúdo escapado em XML; imagens `img` e media:thumbnail/media:content são identificadas. URLs relativas são resolvidas contra o link do artigo. Use Carregar imagens para buscar as imagens externas. O modo Texto continua disponível. Salvar offline grava o HTML e a versão textual; artigos antigos salvos apenas como texto continuam abrindo.

O leitor mostra o conteúdo fornecido pelo feed. Quando o provedor só fornece um resumo, não baixa automaticamente a página completa do site. O mesmo visualizador é usado nos dois apps, com botões no rodapé. Não há navegador externo nem execução de JavaScript.

## Limites explícitos

- Mensagem bruta IMAP/POP até 1 MiB; acima disso, IMAP guarda cabeçalhos/aviso. Texto alternativo até 48 KiB, HTML até 96 KiB e composição textual até 16 KiB.
- Até seis imagens MIME, com 512 KiB somados em dados dos arquivos após decodificar base64; imagens externas até seis URLs por comando e 256 KiB por imagem.
- Imagens passam pelos limites de dimensões do decoder existente (até 2 megapixels), são reduzidas a no máximo 640×400 e respeitam 2 MiB de pixels simultâneos por documento. GIF não é animado; WebP e SVG não são renderizados.
- Cache compartilhado em `/cache/web-images`, até 48 arquivos. As imagens externas já carregadas funcionam offline enquanto permanecerem no cache; podem ser removidas pela rotação do cache. Imagens MIME incorporadas permanecem com a mensagem. Tudo conta na quota de `/SymbianOS`.
- HTML visual é limitado a 96 blocos, com spans por parágrafo e estilos básicos. CSS completo, fontes web, iframes, formulários, scripts e interatividade web não são implementados. URLs de links são identificadas, mas não abrem um navegador.
- Arquivos anexos gerais, envio de anexos e espelhamento completo de estado no servidor continuam fora desta entrega.

## Validação

Testes reais de protocolos/TLS locais continuam separados dos testes físicos. A nova suíte cobre MIME related/alternative, CID/PNG, entidades numéricas, tags ocultas, limites do parser, cache/reabertura offline, atualização de mensagem sem duplicar, imagens externas explícitas com URL relativa e HTML/RSS salvo offline. O smoke LVGL verifica spans, altura/espaçamento, imagem base64, troca para texto e rodapé sem sobreposição ao documento. Prévia visual inspecionada no simulador. Build e sanitizadores registrados em BUILD.md. Não foi executado flash, serial, publicação ou teste físico desta interface.
