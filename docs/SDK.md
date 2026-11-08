# SDK local — API 4

O exemplo `sdk/hello` é um aplicativo Lua original com dados próprios e recurso de texto externo. `sdk/theme-graphite` é um tema de paleta. Não precisa de compilador ESP para criar esses pacotes.

```sh
python3 tools/make_package.py sdk/hello dist/packages/hello-1.0.0.t5pkg
python3 tools/make_package.py sdk/theme-graphite dist/packages/graphite-1.0.0.t5pkg
```

Copie o `.t5pkg` para `/SymbianOS/update/packages/` com o sistema parado, usando os arquivos do launcher. Abra Instalador, selecione o pacote, confira autor/permissões e confirme. Abra Menu e avance a página para encontrar apps novos. Código e recursos vêm do SD; não há conteúdo do app de exemplo no firmware.

## Metadados

`package.json` é entrada da ferramenta host:

```json
{"id":"hello","version":"1.0.0","title":"Olá SDK","author":"Seu nome","kind":"app","permissions":[],"files":["main.lua","message.txt"]}
```

ID: letras minúsculas ASCII, números, `_` e `-`, até 32 caracteres. Versão: componentes numéricos com pontos. Título/autor sem quebras ou NUL. `kind`: `app` (exige `main.lua`) ou `theme` (exige `theme.txt`); não misturar os dois pontos de entrada. Arquivos são planos e ASCII; não há links, subpastas, bytecode ou caminhos absolutos. Preserve as licenças dos recursos que incluir.

A ferramenta gera `manifest.txt` com `format`, ID/versão/título/autor/tipo, API mínima/máxima 4, orientação `landscape`, memória 1048576, instruções 100000, permissões e hashes SHA-256. Essas políticas não são escolhidas livremente pelo pacote. A ZIP é determinística e usa somente entradas STORED.

Permissões: `[]` permite dados em `/data/<id>/` e leitura de configuração pública; `documents` permite `/home/Documents/`; `media` permite áudio/galeria e as pastas Music/Pictures/Recordings; `camera` permite captura e Pictures. APIs privilegiadas adicionais não podem ser habilitadas pelo pacote. Autor é uma declaração do pacote, sem assinatura criptográfica.

Limites detalhados e transações: [Fases 4/5](FASES_4_5.md). Instalações atualizadas mantêm os dados. Remover preserva dados por padrão; remoção com dados só afeta o diretório próprio. Não usar ID de app do sistema.

## Aplicativo

`event(action,value)` retorna uma tabela de vista. `sys.module('chrome')` e `sys.module('store')` fornecem os helpers da release; um módulo `.lua` próprio no pacote pode ser carregado pelo mesmo mecanismo, com hash verificado. `focus`, `suspend` e `checkpoint` fazem parte do ciclo de vida. Falhas ao salvar checkpoint devem sinalizar `navigation_blocked=true`.

`sys.asset('arquivo')` lê recurso próprio verificado, até 64 KiB para Lua. Para imagem JPEG/PNG do pacote, use `sys.media('image','asset:arquivo.png')` e uma vista `layout='image'`. Esse caminho não concede acesso às imagens de outros apps. A prévia é nativa e limitada; não há ponteiros LVGL ou acesso direto ao driver.

`sys.catalog()` lista metadados instalados. A API de instalação é reservada ao instalador do sistema. Para APIs de formulários, mídia e ciclo de vida, consulte [API](API.md).

## Temas

`theme.txt` aceita `background`, `accent`, `foreground` e `fieldColor`, como seis dígitos RGB hexadecimais. Depois de instalar, use “Aplicar tema” no Instalador. “Restaurar tema original” remove a preferência. Esta versão não carrega fontes ou código nativo de temas.

## Desenvolvimento e testes

Use uma cópia da árvore `/SymbianOS` no simulador. O SDK ZIP inclui o gerador, exemplos e documentação. Os arquivos em `dist/packages/` são exemplos construídos, não instalados automaticamente. O teste host usa dispositivos falsos para áudio/câmera; não confirma os periféricos da placa.
