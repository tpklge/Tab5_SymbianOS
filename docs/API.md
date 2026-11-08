# API 8 — runtime 0.8.0

O runtime também aceita manifestos de API 1/2/3/4/5/6/7 para releases anteriores. A release 0.8.0 declara `api_min=8` e `api_max=8`, impedindo a carga parcial no residente antigo. Esquema global de instalação permanece 1; registros e sessões usam cabeçalho `T5DATA1` e são introduzidos sem converter destrutivamente o documento TXT de Notas.

## Ciclo de vida e vistas

Cada aplicativo tem estado Lua separado, até 1 MiB, dentro de 4 MiB totais. Shell/serviços ficam residentes; até três apps comuns ficam em memória. LRU faz checkpoint antes de descarregar. `event(action,value)` retorna uma vista; callbacks de serviço/checkpoint podem ter o retorno ignorado. A vista aceita `title`, `body`, `field`, `text`, `readonly`, `classic`, `list`, `buttons` (até 32 entradas com `id`/`label` e `subtitle`/`icon` opcionais), cores e labels dos comandos comuns. API 3 acrescenta `layout`: `grid`, `home`, `list`, `editor`, `calculator`, `contacts` ou `form`; `fields` contém os rótulos dos campos, separados por nova linha. O renderer agrega seus valores na mesma ordem em `value`, separados por nova linha, preservando o formato das sessões e registros. `calculator` é apenas um layout de teclado: teclas e cálculo continuam definidos no Lua do SD. `icon` identifica uma imagem original do atlas externo `icons.bin`.

`focus` abre/retoma. `suspend` entrega o editor antes de sair. `checkpoint` salva estado para descarregar/backup. `next_app='id'` solicita navegação. `navigation_blocked=true` impede abandonar/descarregar estado que não pôde ser persistido. Cada callback tem 100.000 instruções, verificadas em lotes de 1.000; cancelamento é cooperativo e chamadas C não são interrompidas por esse hook.

Erros mantêm a saída para o shell disponível. Fechar um app com erro não volta a executar um checkpoint defeituoso; tenta preservar o texto em `recovered-draft.txt` e devolve aviso. Uma VM reaberta recebe nova geração; eventos da geração anterior são descartados. Isso não é isolamento de processos do Symbian nem ABI para apps antigos.

## Arquivos e permissões

`app.<id>=arquivo.lua`, `grant.<id>=prefixo/,outro/` e `cap.<id>=capacidade,...` são declarações do manifesto verificado. Apps acessam seu próprio `/data/<id>/`; leitura de `/config/public/` é comum. Concessões extras são explícitas. Alterações comuns em boot, sistema, apps e meta são recusadas, inclusive pelo gerenciador de arquivos; restauração tem capacidade separada.

- `sys.read(path)`: string ou `nil,motivo`, até 64 KiB. Ausência é `not found`.
- `sys.write(path,text)`: `true` ou `false,motivo`, até 64 KiB, com quota e temporário sincronizado.
- `sys.fs('list',path,'',offset)`: até 12 entradas e total; cada entrada tem `path`, `size`, `directory`.
- `sys.fs('stat',path)`: propriedades básicas.
- `sys.fs('mkdir'|'remove',path)`: criação/exclusão; diretórios só são removidos se vazios.
- `sys.fs('copy'|'move',src,dst)`: destinos existentes são recusados. Cópia faz streaming cancelável.
- `sys.fs('backup'|'restore',path)`: capacidade `backup`, apenas nomes em `/backups/`. Restore valida integridade, reserva staging completo e publica journal antes de trocar destinos.
- `sys.usage()`: uso, teto, reserva, livre físico e categorias, com reconciliação real.
- `sys.new_id()`: sequência persistente no meta, sujeita à mesma quota; não reutiliza intencionalmente IDs removidos.

A API não expõe handle bruto, caminhos físicos ou escrita alternativa em flash. Hashes verificam integridade, não procedência. O manifesto de desenvolvimento e seus grants são uma base confiável, não um instalador de terceiros com assinaturas.

## Apps, eventos e notificações

- `sys.module('nome')`: módulo de texto verificado daquela release; até quatro níveis de chamadas.
- `sys.apps()`: IDs dos apps comuns realmente abertos.
- `sys.app_info(id)`: bytes do alocador Lua, estado ativo/falha; não mede toda a memória nativa do app.
- `sys.close(id)`: checkpoint/fechamento de app inativo, com motivo/aviso quando necessário. Shell/serviços não podem ser fechados assim.
- `sys.send(target,event,payload)`: fila de até 32 mensagens, payload até 512 bytes, presa à geração da VM destinatária.
- `sys.notices()`: até 32 avisos nativos.
- `sys.notify(id,text,alarm,sound,due)`: capacidade `notify`; entrega visual e sinaliza, evitando repetir som de um vencimento já atendido pelo agendador nativo.
- `sys.dismiss(id)`: mesma capacidade; o serviço externo persiste a remoção do histórico.
- `sys.schedule('begin')`, `sys.schedule('add',id,text,epoch,repeatSeconds,alarm,sound,lastDelivered)`, `sys.schedule('commit')`: substituição do plano nativo, até 512 registros, repetição 0 ou 86400. A política e o ledger são do serviço externo.

O LVGL consulta prazos nativos sem pegar o lock do filesystem nem executar Lua. Um worker de áudio serializa o sinal sem prender a tarefa gráfica. O serviço confirma a entrega no SD ao recuperar seu worker. Uma interrupção de energia antes dessa confirmação pode repetir o aviso; FAT e saída física de áudio não formam uma transação única.

## Tempo e perfis

- `sys.epoch()`: relógio interno, avançado monotonicamente. `sys.epoch('AAAA-MM-DD[ HH:MM[:SS]]')` valida a data civil e aplica o fuso configurado.
- `sys.time()`: data/hora legível; indica ausência de ajuste quando o relógio não é confiável.
- `sys.date(epoch,mode)`: `datetime`, `day`, `month`, `clock` ou `weekday`.
- `sys.set_clock(epoch)`: capacidade `clock`; retorna `(aplicado, persistidoNoRTC)`; o horário do runtime pode ser ajustado mesmo quando o adaptador não consegue persistir no RTC. O simulador não altera o relógio do computador.
- `sys.zone(posixTZ)`: mesma capacidade; a preferência é salva em `/config/public/timezone` pelo app.
- `sys.offline(bool)`: capacidade `profile`; solicita o controle físico Wi-Fi do BSP. O simulador aceita um adaptador de teste. Não oferece conectividade.

Não são expostos `io`, `os`, `package`, `debug`, `coroutine`, carregadores nativos, `pcall`, `xpcall` ou metatables. O contador de instruções não pode ser contornado por capturas repetidas do erro. Código C, drivers e interpretador continuam sendo base confiável.


## Formulários — API 4

`controls` é uma lista de até seis descritores `{label,type,options,keys}`. `type` é `text`, `date`, `time` ou `select`. Campos date/time usam máscaras brasileiras `DD/MM/AAAA` e `HH:MM` com apenas dígitos e separadores fixos. O app valida o calendário/intervalos antes de persistir. Select usa opções e chaves separados por `\n`, até 64 entradas, com a mesma contagem; `keys` vazio usa as opções como valores. O `value` continua sendo os campos na ordem, separados por nova linha. Enter avança, Tab troca foco, setas em um dropdown alteram a seleção.

`layout='files'` distribui entradas (`dir:`, `file:` ou `play:`) em lista e ferramentas em painel compacto; `image` mostra a prévia preparada no worker e `image_zoom` permite rolagem na ampliação. `mediaStatus=true` solicita atualização do status da mídia em background, sem filesystem/driver na tarefa LVGL. Apenas apps autorizados podem consultar o estado global ou mostrar uma imagem que não lhes pertença.

## Multimídia — API 4

`sys.media(op,path,zoom)` retorna `ok,status`; `status` isolado retorna uma string. Requer capacidade `media`, ou `camera` para prévia/captura. Caminhos também exigem permissão VFS.

- `play`, `pause`, `stop`: player em tarefa própria.
- `queue`: segundo argumento contém até 64 caminhos em linhas; todos são validados antes de iniciar.
- `record`: destino `.wav` em `/home/Recordings`, sem substituir arquivo existente.
- `image`: JPEG/PNG com limite de entrada/pixels, zoom 1/2. Recurso próprio pode ser `asset:nome.png`; nesse caso não exige acesso global às fotos.
- `preview`, `capture`: prévia por captura e JPEG em `/home/Pictures`.
- `test`: sinal de alarme, com resultado do driver quando disponível; esse resultado não é medição acústica.

`sys.asset(name)` retorna recurso do pacote próprio com hash verificado, até 64 KiB, ou `nil,motivo`. Não fornece caminho físico nem acesso a recursos de outro pacote. Os codecs/limites estão em [Fases 4/5](FASES_4_5.md).

## Pacotes e temas — API 4

`sys.catalog()` retorna até 32 metadados instalados: `id`, `title`, `version`, `permissions`, `kind`. Não executa o pacote.

`sys.package(op,arg)` é exclusivo da capacidade `install` do instalador do sistema. `inspect` retorna metadados do arquivo em `/update/packages` e registra seu hash para revisão. `install` exige a mesma revisão/hash e ativa a versão após todas as gravações. `remove` preserva dados; `remove_data` remove somente os dados próprios além do payload. `theme` aplica uma paleta instalada e `theme_default` restaura o padrão. O formato não concede essa capacidade a terceiros.

`sys.theme()` devolve a paleta validada do tema ativo; `chrome.lua` a aplica aos controles. Não executa código Lua do tema. [Formato e exemplo](SDK.md).

## Extensões da API 5

Vistas aceitam `result` (até 512 bytes), apresentado em painel destacado no conversor, e `tools` (até 16 botões com id/label/icon). Novos layouts: `player`, `recorder`, `camera`, `gallery`, `photo`, `video_view` e `notebook`. Botões da galeria podem declarar `thumbnail`, sujeito à permissão de leitura; miniaturas são limitadas e mantidas em cache. Câmera exige capacidade `camera`; reprodução de vídeo exige `media`.

`sys.fs('list',path,'',offset,filter)` aceita filtros `audio`, `visual` ou `recordings`, aplicados antes da paginação.

`sys.media` acrescenta operações `camera_on`, `camera_off`, `camera_status`, `video_active`, `video_record(path)`, `video_play(path)`, `video_stop`, `next_track` e `prev_track`. A operação `queue` aceita índice inicial (base 1) no terceiro argumento. Prévia e execução de mídia usam workers separados; `status` informa tempo, nível e progresso sem executar I/O do dispositivo sob o lock de estado.

Gravações `.au` usam G.711 μ-law, mono 8 kHz. WAV PCM permanece compatível. Vídeo usa perfil AVI/MJPEG 640×360, até 5 fps, sem áudio; não há suporte genérico a qualquer arquivo AVI. Pacotes SDK anteriores de API 4 continuam compatíveis.

## API 6 — energia, volume e rede

`sys.power('status')` retorna snapshot com available, percent (-1 se desconhecido), millivolts, milliamps, charging, enabled, fast e error. Status não executa I²C no renderer. `sys.power('normal'|'fast'|'off')` exige capacidade `power`, aplica o driver e retorna booleano; política/persistência continuam no app do SD.

`sys.media('volume')` consulta volume mestre; `sys.media('volume','',n)` ajusta de 0 a 100 com capacidade `media`. Preferência é salva pelos apps em config/public/volume.

`sys.net` exige capacidade `network`. Operações: `status`, `connect(ssid,password)`, `disconnect`, `cancel`, `share('on'|'off')`, `download(url,path)` e `feed(url,'/data/feeds/latest.xml')`. Operações lentas usam worker próprio, retornam aceitação e são acompanhadas por `status`: busy, connected, sharing, bytes, message, address e token. Destinos precisam de permissão de escrita; downloads comuns ficam em /home/Transfer. O perfil Offline recusa conexão/transferência. Connect não persiste senha.

Controles de formulário aceitam tipo `password`, com máscara gráfica. Não colocar credenciais em `body`, logs, sessões ou metadados. O SDK de exemplo e instalador de pacotes API 4 permanecem compatíveis; as novas capacidades desta release são declaradas no manifesto verificado do sistema.

## API 7 — busca de redes

`sys.net("scan")` agenda busca assíncrona, com capacidade network e perfil fora de Offline. `sys.net("status").networks` retorna até 32 entradas `{ssid, security, signal, channel}`, sem SSIDs vazios ou com NUL/quebra de linha, ordenadas pelo sinal e deduplicadas pelo SSID. O cache só é substituído ao concluir uma busca válida. `cancel` interrompe a busca; status mantém o motivo detalhado fornecido pelo backend. Não persiste credenciais.

## API 8 — redes conhecidas

`sys.net("connect",ssid,password,remember)` recebe booleano opcional para persistir após sucesso. `saved` retorna lista `{ssid, automatic}`, sem senha; `connect_saved(ssid)` reutiliza a credencial nativa; `forget(ssid)` remove; `automatic(ssid,"on"|"off")` altera a preferência. `status` inclui `ssid` conectado. `autoconnect` agenda a tentativa com backoff e é chamado pelo serviço do SD em tick; todas as operações exigem network. Perfil Offline é respeitado e a desconexão manual suspende tentativas na sessão. Credenciais em config/network são nativas e não exigem concessão de leitura/escrita bruta ao app.

O token de transferência passa a 8 caracteres. Upload continua com buffer limitado no heap, pilha HTTP de 32 KiB e logs/last-upload.txt. [Formato de dados e limites](REDES_SALVAS_0_8.md).
