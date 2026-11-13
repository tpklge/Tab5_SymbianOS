# G1 e documentos HTML complexos — 0.19.0

**Atualização:** a validação abaixo era apenas de host e se mostrou insuficiente para o Tab5. A correção de memória e leitura leve está em [NAVEGADOR_MEMORIA_NATIVA_E_LEITURA_LEVE.md](NAVEGADOR_MEMORIA_NATIVA_E_LEITURA_LEVE.md).

Correção de 12/11/2026. O endereço do portal usado no teste é `https://g1.globo.com/`.

## Problema e alteração

O limite anterior contava cada caractere `<`, incluindo fechamento de tags, comentários e texto de scripts, como elemento. A cópia do G1 obtida nesta sessão tinha 925.684 bytes e 8.823 desses caracteres, portanto era recusada antes da renderização. Uma contagem HTML independente encontrou 4.384 elementos de abertura, incluindo 47 scripts; aproximadamente 470 mil bytes eram texto de scripts que o motor não executa.

Agora o leitor prepara o documento antes de construir o DOM:

- remove scripts e comentários da cópia usada para renderizar; mantém o HTML original no cache do SD;
- conta somente tags de abertura, com limite de 8.192 elementos; fechamento e texto CSS/textarea/title não consomem esse limite;
- respeita aspas em atributos, nomes completos e fechamento de blocos raw-text/RCDATA, incluindo maiúsculas/minúsculas;
- preserva os estilos CSS e o conteúdo legível. JavaScript continua sem execução;
- verifica o DOM produzido pelo parser: no máximo 32.768 nós e profundidade de 128 antes das operações recursivas do motor;
- libera a árvore Gumbo por percurso iterativo sem alocação adicional, inclusive ao rejeitar uma árvore muito profunda.

Permanecem o limite de 1 MiB por resposta HTML, 256 páginas, o prazo de renderização e as proteções de PSRAM existentes. Uma página modificada pelo servidor que ultrapasse esses limites ainda será recusada com diagnóstico. Não se promete equivalência com um navegador desktop ou páginas cujo conteúdo depende de executar JavaScript.

As alterações locais em litehtml/Gumbo preservam os arquivos de licença das dependências. Nenhum conteúdo do G1 é distribuído no aplicativo ou no cartão modelo; a captura de teste ficou apenas em arquivo temporário no host.

## Validação realizada

- **42/42 testes de host e smoke da interface passaram**, em `build/g1-host.log`.
- Regressões do motor incluem 5.000 elementos irmãos, recusa acima de 8.192 aberturas, recusa e liberação de 5.000 níveis aninhados, tabela malformada, remoção de scripts/comentários, atributos com `>` e preservação de CSS/textarea/title.
- A captura real do G1 passou pelo mesmo motor: **925.684 bytes de entrada → 451.366 bytes preparados → 16 páginas**, com renderização da primeira e da última página. Resultado em `build/g1-snapshot.log`.
- SHA-256 da captura temporária: `ad0b95a97dd8efe6e20c2defae44695f20d871658d9ef8dff5e84ca008ae2e79`.
- Build ESP32-P4 concluído, em `build/g1-p4.log`.

O resultado com o G1 foi medido no host. Não foi executado no Tab5, nem houve flash ou abertura de serial. Tempo e consumo de memória reais no P4 ainda dependem do teste físico.

## Instalação e teste no Tab5

1. Enviar o BIN e a pasta SymbianOS de `sist/0.19.0/launcher/` pela WebGUI do launcher e reiniciar.
2. Conectar ao Wi-Fi, abrir o navegador e usar `https://g1.globo.com/`.
3. Conferir a renderização e percorrer as páginas com Anterior/Próxima. O teste de host da captura produziu 16 páginas; esse número pode variar com o conteúdo atual.
4. Abrir Links e visitar uma notícia. Carregar CSS/imagens pelo comando existente, se desejado; conteúdo que depende de JavaScript permanece indisponível.
5. Reabrir a página sem rede para conferir o cache e testar Voltar/Recarregar. Falha na transferência ou documento acima dos limites deve manter a cópia anterior, sem tela azul/reinício.
