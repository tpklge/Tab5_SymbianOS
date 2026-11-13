# API 14 — runtime 0.17.0

O runtime também aceita manifestos de API 1–13 para releases anteriores. A release 0.13.0 declara `api_min=13` e `api_max=13`, impedindo a carga parcial no residente antigo. Esquema global de instalação permanece 1; registros e sessões usam cabeçalho `T5DATA1` e são introduzidos sem converter destrutivamente o documento TXT de Notas.

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
- `sys.send(target,event,payload)`: fila de até 32 mensagens, payload até 512 bytes, presa à geração da VM destinatária. Apps podem enviar para si mesmos. Para `services`, somente `mesh` com capacidade `mesh`, `bench:start`/`bench:stop` com `diagnostics` e `ack`/`snooze` originados pelo shell. Outros envios entre apps retornam `false`.
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


## API 9 — energia e versões

`sys.power("screen_timeout")` retorna minutos; com segundo argumento inteiro 0/1/2/5/10/30 persiste e aplica o tempo (0 = sempre ligada), retornando booleano. `sys.power("screen_off")` solicita apagamento manual. `sys.power("sleep", minutos)` aceita 1/5/15 e retorna booleano/mensagem; depende do backend físico e só deve ser chamado após confirmação. Requer capacidade `power`.

`sys.update(op)` requer capacidade `update`, retorna booleano/mensagem. `stage` verifica `/update/manifest.txt` e prepara tentativa de release SD; `rollback` restaura o manifesto anterior; `inspect` revisa `/update/runtime.bin` e `runtime.sha256`; `install` exige revisão da mesma imagem e confirmação de UI; `runtime_previous` seleciona outro slot confirmado compatível; `reboot` salva checkpoints e reinicia; `status` informa runtime/slot. Flash e partições continuam inacessíveis como arquivos Lua. Operações lentas usam o worker de UI, sem driver direto no app.

Tela apagada mantém serviços; suspensão profunda para serviços e depende de temporizador/reset para retorno. Rollback automático nativo depende do bootloader instalado. Ver [procedimentos e limites](FASE_7_0_9.md).


## API 10 — aparência e bloqueio

`sys.power("brightness")` retorna brilho 10–100; com segundo argumento inteiro persiste o valor. O tempo de tela permanece separado. `sys.power("sleep", minutos)` usa agora suspensão em memória (light sleep), não reinício por deep sleep; respeita confirmação e retorna diagnóstico. Teste físico pendente.

`sys.security(op, ...)` exige capacidade `security`. `status` retorna enabled/wait; `set(oldPin,newPin)` configura quatro dígitos (newPin vazio desativa); `lock` solicita bloqueio; `unlock(pin)` verifica e limita tentativas; `return` identifica o aplicativo anterior. Somente a tela de bloqueio é permitida no estado bloqueado. O verificador e as tentativas ficam em /config/security/. Nenhum segredo de PIN deve ir em sessões ou logs.

`sys.theme()` acrescenta builtin; o módulo de política no SD escolhe paleta e wallpaper. Views aceitam wallpaper como nome de recurso verificado T5BG/RGB565, layout calendar com deslocamento inicial em result e até 64 botões. Tipo de campo pin é numérico, mascarado e limitado a quatro caracteres. Layout lock omite navegação comum; reader usa texto em largura completa. Photo aceita result zoom2/zoom4 para imagem deslocável.

`sys.media("mode", repeatMode, shuffle)` aceita none/all/one e flag 0/1; aplica à próxima fila. Fila até 256 caminhos confinados e 64 KiB de argumentos. O perfil de pacotes externos do instalador permanece API 4; as capacidades novas do sistema são declaradas no manifesto de release. Existe o perfil SIS sem assinatura descrito em docs/PERFIL_SIS_1.md; SISX e JAR não são suportados.

## API 11 — controles e diagnóstico

Views aceitam `footer` (até quatro ações), `statusFields` (até oito campos com label/value e percent opcional), `optionAction`, `profile` e `refreshMs` (500–5000 ms, processado no tick do worker). Formulários aceitam `slider` de brightness (power) e volume (media). O toolkit aplica prévia e devolve valores numéricos; scripts persistem a confirmação. `sys.memory()` requer diagnostics e informa heap/PSRAM disponíveis e memória Lua. `sys.theme()` inclui app_id e perfil atual. Ícones externos podem ocupar até 2 MiB.

Layouts options, status, choices, clock, packages e switcher permitem opções não editáveis, indicadores, relógio e linhas compactas. Botões do alternador podem declarar close para a ação X. As APIs antigas permanecem disponíveis.

## API 12 — transporte externo

`sys.radio(op,arg)` requer mesh, retorna string (inclusive vazia) no sucesso ou nil,motivo na falha. Operações: state, link, scan, devices, connect_ble (MAC,tipo,PIN), connect_uart, read, send (até 512 bytes) e disconnect. UART usa framing 94C3 e tamanho big-endian; BLE recebe/envia protobuf sem framing. Filas nativas limitadas e sem acesso à UI. Perfil offline bloqueia comandos de rádio; estado/link/leitura/desconexão permanecem disponíveis. Somente apps com mesh podem encaminhar evento mesh ao serviço. O serviço controla o protocolo, persistência e tentativa de envio; não emitir confirmação de entrega por sucesso local.

0.12.1 mantém API 12: columns opcional (1–4) em views; grid/list aceitam colunas. sys.net() inclui signal (RSSI em dBm) e signal_valid, lidos de snapshot. sys.radio("diagnostic") retorna contadores limitados, sem conteúdo de mensagens. Campos/operações anteriores permanecem disponíveis. BIN e SD 0.12.1 juntos são necessários para aplicar os novos controles nativos e de memória.

## API 13 — apresentação e confirmação de comandos

Views aceitam centerTools para centralizar ferramentas na borda inferior; layout home usa statusFields para os cartões de agenda/avisos; chat renderiza botões com corpo completo e outgoing booleano em balões. Bibliotecas genéricas de código permanecem no residente; conteúdo/layout/regra de apps ficam no SD. sys.send para services/mesh é uma ponte síncrona autorizada pela capacidade mesh; falha de execução pode retornar false,motivo. Os demais eventos autorizados permanecem em fila; envios não autorizados entre apps retornam false. O diagnóstico de rádio identifica runtime e pedidos UART. A quota em uso montado é contabilizada por mutações serializadas, com auditoria de remontagem/recuperação.

0.13.1 mantém API 13. O host pode registrar bootProgress(percent) para integridade; o callback não depende de aplicativos. Diagnóstico de rádio expõe contadores de eco e IDs públicos de sincronização.

## Meshtastic 0.14.0

`layout="chat"` com `field=true` e três controles cria composição inline: destinatário (select), canal (select), mensagem (text). `text` serializa os três valores separados por LF. A ação `send` é enviada a partir do botão junto ao editor; seletores e rascunho acompanham refresh. `outgoing` alinha balão e texto conforme a direção.

`layout="mesh_nodes"` usa botões em lista à esquerda e statusFields à direita. O botão marcado outgoing identifica a seleção da lista neste layout. `centerTools` mantém paginação na base. Novos layouts exigem atualizar BIN/SD juntos.

O módulo SD mesh_channels implementa administração local, correlaciona request_id/origem, usa passkey só em memória e confirma escritas por releitura. Schema nodes.tsv acrescenta apelido, last_heard, SNR, saltos, bateria, tensão, latitude, longitude, altitude e modelo. history.tsv acrescenta destino na coluna 7; registros legados continuam legíveis. channels.tsv acrescenta role, PSK hexadecimal e settings brutos em hexadecimal para preservar campos não editados.

Na 0.14.1, nodes.tsv acrescenta canal ouvido (13) e chave pública hexadecimal (14). DM solicita PKI explicitamente, usa canal de transporte zero e encaminha a chave pública quando conhecida. A filtragem privada depende de participante; confirmação local e remota são distintas.

Na 0.14.2 o envio volta a delegar criptografia ao rádio, sem flag/chave pública no pacote e preservando o canal solicitado. status.tsv acrescenta indicadores de presença de chaves (10/11), modo licenciado (12), região (13), último roteamento (14) e destino (15). Não contém valores de chaves privadas.

Na 0.14.3 nós/histórico Meshtastic usam cache limitado e persistência cooperativa: um banco por invocação. A leitura de arquivos pode refletir o lote após ciclos adicionais. O watchdog Lua permanece em 100000 instruções por evento; falhas deixam classificação sem valores em /logs/lua-fault.txt.

Na 0.14.4, refresh de reader readonly com alteração apenas em text mantém a textarea e preserva cursor/rolagem. Meshtastic exporta relatório sem valores de chaves privadas para /data/mesh/diagnostic.txt mediante ação save_diagnostic.

Na 0.14.5, layout diagnostic readonly usa painel/label rolável, com atualização no mesmo objeto e teclas de rolagem. status.tsv acrescenta firmware (16) e suporte PKI anunciado em DeviceMetadata.hasPKC (17). Toques de navegação durante foreground entram na fila limitada; não são aplicados a um app diferente do dono original.


## API 14 — e-mail

Capacidade `mail` exclusiva do cliente E-mail: `sys.mail(op, ...)`. Caminhos de credenciais não são concedidos aos aplicativos.

- `account` retorna tabela address/name/protocol/inHost/inPort/inSecurity/inUser/inPassword/outHost/outPort/outSecurity/outUser/outPassword/automatic; `configure(tabela)` valida e persiste. protocol é `imap` ou `pop`, security `tls` ou `starttls`.
- `list(pasta)` retorna metadados (id/from/to/subject/date/folder/status/seen), `read(id)` inclui body e marca leitura local, `folders` lista pastas IMAP.
- `draft(id,to,subject,body)` e `queue(id,to,subject,body)` retornam ID; ID vazio cria mensagem. `move(id, destino)` aceita `trash`, `delete` (somente da lixeira) e `outbox` (reenvio autorizado).
- `sync(pastaIMAP)` e `flush` iniciam trabalho nativo assíncrono; `status` retorna busy/message/queued; `cancel` interrompe. Rede, perfil e relógio são conferidos antes de iniciar.

Layout `mail_compose` com controle `multiline`, além dos formulários com senha mascarada. Limites, semântica de confirmação e segurança do cartão em [E-mail](EMAIL_0_15.md).


0.15.8 mantém API14. Estado `headers` indica cache somente dos cabeçalhos de mensagem acima de 128 KiB. read/list/folders/status filtram a conta ativa. Diagnóstico mantém operação e contexto TLS sem credenciais. Detalhes em [Revisão de e-mail](AUDITORIA_EMAIL_0_15_8.md).


0.15.9: sys.net("recover") agenda recuperação da última conexão; sys.mail("recover") cancela a tentativa de e-mail e agenda a mesma recuperação, respeitando perfil offline. Não aguarda o RPC na interface. A versão/status de rede aparecem no diagnóstico. Detalhes em [Recuperação](RECUPERACAO_REDE_0_15_9.md).

0.15.10: mesma API Lua; recuperação usa limpeza do driver e suspende buscas após falha. [Detalhes](DRIVER_WIFI_0_15_10.md).

0.15.11: sys.net() e sys.mail("status") acrescentam restart_required; status de mail também inclui network_message. O sinal indica falha persistente de controle remoto que requer reinício; não dispara reboot. [Detalhes](TRANSPORTE_C6_0_15_11.md).

0.15.12: API Lua preservada; configuração nativa SDIO RX corrigida para streaming e identificada no diagnóstico. [Detalhes](SDIO_RX_0_15_12.md).

0.16.0: `sys.mail("status").html_supported` e `sys.net().html_supported` anunciam o leitor visual. `sys.mail("read",id).has_html` indica HTML disponível. `sys.mail("reload",id)` baixa novamente uma mensagem recebida, e `sys.mail("images",id)` agenda imagens externas sem expor conteúdo/credenciais à capacidade network do app. `sys.net("images",html,baseUrl)` agenda até seis imagens HTTP(S), respeitando perfil offline e 256 KiB por imagem; status/cancelamento usam o serviço de rede existente. Limites de argumentos ampliados somente para essa operação.

Layouts `mail_html` (mailId, exige mail) e `html` (html/baseUrl, exige network) produzem documento limitado em View.document; não executam scripts. Ausência do sinal html_supported mantém os apps no modo textual. Detalhes em [HTML/e-mail/RSS](HTML_EMAIL_RSS_0_16.md).

0.17.0: `sys.theme().language` retorna pt/en/es. `chrome.view` aplica os catálogos, preservando IDs/chaves e conteúdo. `sys.memory("time")` retorna monotônico em ms; status de memory acrescenta device/runtime/release, vídeo/áudio, energia e conectividade para instrumentação. `device` identifica build físico, independentemente de MemoryInfo.available.

`sys.mail("status").attachments_supported` anuncia anexos. `read` acrescenta attachments (name/type/bytes). `files,path` lista até 128 entradas em /home para o seletor; `attach,id,path` copia para um rascunho; `detach,id,index` remove uma cópia; `export,id,index` agenda cópia para Downloads. Índices são 1..8. IDs e caminhos privados não são fornecidos pelo app de origem; operações validam dono e VFS. Capacidades permanecem mail/diagnostics, sem permitir leitura de configuração privada por seletores.

Metadados de anexos usam marcador @T5ATT2 nos registros cacheados, com leitura dos formatos anteriores. Tombstones por conta/UID ficam em /data/mail/deleted. Rodapé aceita cinco ações sem options ou quatro com options, deixando espaço para voltar. [Escopo/limites](FASES_3_4_6_7_8_10_0_17.md).

## Alterações vigentes: navegador e launcher

`sys.update` foi removida. `sys.power("reboot")` exige a capacidade power e usa o checkpoint nativo. `sys.net("page", url)` inicia download de até 1 MiB de HTML para o cache do navegador; exige network e grant de escrita em /data/browser/cache/. `sys.net("page_path", url)` retorna caminho e URL final do registro existente no cache somente com grant de leitura (nil se ausente). O registro contém URL final, newline e HTML; a gravação é atômica pelo Storage. `sys.html().links` lista até 128 pares url/label extraídos do documento. Views de formulário podem declarar `submit` para a ação do Enter.

## SIS perfil 1

`sys.package("inspect",path)` também aceita pacotes em `/home/Transfer/`, valida o perfil SIS v9 sem assinatura e retorna `uid`, `format`, `trust`, `inspection_ms` além dos metadados existentes. A inspeção é cancelável pelo controle nativo/Escape. `install` exige o hash revisado, UID/ID livres e destino novo; usa journal e staging recuperáveis antes de ativar o catálogo. Aplicativos SIS têm apenas acesso aos próprios dados, ABI de aplicações 4 e runtime API 14. SISX, assinaturas, Deflate e dependências são rejeitados.

## Ciclo de vida SIS sem assinatura

O perfil 2 declara dependências UID/faixa, permissões documents/media/camera e esquema/migração declarativa. `sys.package("inspect",path)` acrescenta old_version, is_update, schema, migration, new_permissions, dependencies, compatible, compatibility e sha256. `install` mantém o vínculo da revisão; `install_downgrade` exige a segunda confirmação da UI e usa o mesmo hash revisto. `sys.package("pending","")` retorna id/version da tentativa. `rollback` recebe ID, cancela uma tentativa pendente ou restaura versão/dados do snapshot anterior. O catálogo inclui uid, author, schema, can_rollback e previous_version. Remoção SIS é transacional e respeita dependentes; remove preserva dados, remove_data apaga apenas dados privados e snapshots pertencentes ao pacote. Todas essas operações continuam exclusivas do instalador do sistema. O runtime persiste início/confirmação da primeira abertura e restaura uma tentativa interrompida no boot. Assinaturas e SISX continuam rejeitados.

## Lista de pacotes e envio web

`sys.fs("list",folder,"",offset,"packages")` filtra arquivos SIS/T5PKG antes da paginação nativa de 12 registros e retorna a contagem filtrada. O layout packages usa `scrollKey` para reter posição/foco por página e controles fixos à direita. `sys.net().transfer_address` é o endpoint HTTP completo, separado de address (IP); o backend nativo de envio usa HTTP 8080 e expõe diagnóstico específico de falha do serviço.

A view `web_document` pode fornecer `htmlPath` com um registro em `/data/browser/cache/`, sujeito ao grant de leitura. O código nativo lê até 1 MiB de HTML e extrai a URL base do cabeçalho do registro, evitando duplicar o documento no heap do Lua.

`sys.net("page_status", url)` retorna `{busy, ok, bytes, message}` para o último pedido de página correspondente, ou nil. Esse resultado permanece independente da mensagem geral da rede. O navegador usa esse resultado para concluir sua navegação. Páginas grandes podem ser representadas em leitura leve para limitar o DOM.

O cache de `page_path` identifica o perfil móvel do navegador; não construa o hash do caminho na aplicação. A implementação nativa de recursos do navegador usa o mesmo perfil móvel e cache separado dos recursos de e-mail/feeds.

Revisão 20261113-R1: `sys.net("browser_profile")` fornece `build` e `wireLimit`. O navegador exige o build correspondente e lê cache estruturado `T5B2`; o HTML remoto não passa pelo renderizador. `sys.html().engine` informa `leitor nativo` nesse aplicativo; Links e paginação permanecem disponíveis. Recursos CSS/imagens do navegador estão suspensos.


Revisão 20261113-R2 (substitui a suspensão de HTML/CSS da R1): no navegador,
`htmlMode="rich"` usa marcação admitida com limites; outro valor mantém leitura
nativa. `sys.html().engine` informa `litehtml limitado` ou `leitor nativo`.
`sys.html("forms", cachePath)` retorna action/method/fields dos formulários;
`sys.html("submit", cachePath, index, values)` valida novamente o registro e
retorna boolean/URL do pedido iniciado. `sys.html("clear_sessions")` limpa
cookies, páginas e imagens do navegador quando não há operação de rede ativa.
Essas três operações são exclusivas do aplicativo browser. `sys.net("page",
url, initiator)` leva a origem da navegação ao controle de cookies. Cache móvel
usa uma nova revisão; recursos privados ficam em `/data/browser/assets/`.
Detalhes e limitações: [Navegador R2](NAVEGADOR_20261113_R2.md).


Revisão 20261113-R3: views preparadas pelo motor podem conter `htmlHotspots`
(coordenadas da página e URL, somente metadados nativos). A UI converte esses
retângulos à escala e envia `navigate_url` com a URL no valor do evento. No modo
Leitura, links ficam em uma lista acionável. A admissão limitada e a recuperação
em texto passam a ser compartilhadas por navegador, e-mail e feeds.
