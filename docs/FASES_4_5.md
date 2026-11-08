# Fases 4 e 5 — implementação 0.5.0 / API 4

Interfaces e lógica dos cinco novos apps residem no SD: Música, Gravador, Galeria, Câmera e Instalador. Os critérios de bancada do planejamento permanecem pendentes; testes de host e build não substituem som, microfone, câmera ou cartão físicos.

## Interface, relógio e alarme

Conversor usa dropdowns com nomes de unidades, entrada decimal brasileira e botão para inverter origem/destino. Data e hora são campos separados, com máscaras `DD/MM/AAAA` e `HH:MM`, formato de 24 horas e validação de datas reais. O mesmo vale para criar/editar alarmes; repetição tem menu. Os registros persistentes continuam usando timestamps; sessões antigas de formulário ISO são convertidas em memória.

Arquivos usa lista com ícones, nomes/subtítulos, campo de caminho e ferramentas laterais em duas colunas, com botões de 46 px e fonte menor. Copiar/mover abre formulário de origem e destino; criar pasta abre campo próprio. Voltar sai da operação ou sobe uma pasta. Diretórios e documentos existentes são preservados.

O caminho antigo de notificação abria/fechava o codec para cada sinal em mono 16 kHz. Agora áudio compartilha configuração estéreo de 48 kHz, mantém o codec aberto e aplica amplificador, desmute e volume explicitamente. Sinais usam envelope para reduzir estalos. Isso corrige a implementação, **não prova que o alto-falante físico tocou**.

Relógio tem “Testar som do alarme”. Falhas retornadas pelo driver são mostradas e registradas em `/logs/last-audio.txt`; o sucesso do driver também não mede pressão sonora. Alarmes interrompem a alimentação de PCM do player e a retomam após o sinal. Uma música pausada pelo usuário permanece pausada. Perfil silencioso continua com política explícita de alarmes.

## Multimídia

- Música: WAV PCM16 mono/estéreo, 8–48 kHz, e MP3 via minimp3, incluindo tags ID3v2. Leitura em blocos de até 16 KiB, sem carregar a música inteira. O backend converte para saída estéreo 48 kHz. Pausar/retomar, parar e playlist persistente da página atual. Fila nativa até 64 faixas, com avanço no EOF. Pasta `/home/Music`; o navegador pagina 12 entradas.
- Gravador: entrada do ES7210, WAV mono 16 kHz em `/home/Recordings`, gravação em blocos, limite de 16 MiB de PCM. Parada, falha de entrada e falta de espaço finalizam o cabeçalho e preservam o que já foi gravado. Arquivos `.part` interrompidos são recuperados no próximo boot quando possível, sem sobrescrever uma gravação existente.
- Galeria: JPEG/PNG em `/home/Pictures`, paginação, prévia RGB565 sob demanda e ampliação limitada com rolagem. Entrada até 4 MiB, até 2 megapixels e orçamento de 8 MiB para alocações dos codecs; nenhuma decodificação irrestrita de fotos grandes. Uma prévia por app residente é mantida, liberada com a VM, sem cache crescente. O formato de imagem preparado é limitado a menos de 2 MiB por prévia. Zoom normal limita a imagem a 640×400; ampliado a 960×600.
- Câmera: adaptador original sobre BSP/ESP Video, RGB565 até 1280×720, duas filas MMAP, timeout de dequeue de 1 segundo, liberação dos buffers e salvamento JPEG qualidade 85. “Atualizar prévia” faz uma captura; **não há vídeo ao vivo**. A orientação/compatibilidade do sensor e latência ainda precisam de ensaio no Tab5.

Reprodução/gravação usa executor próprio, separado do Lua/LVGL. Não bloqueia o editor em chamadas de reprodução. Operações de imagem/câmera ocorrem no worker de interface, nunca na tarefa proprietária do LVGL. Valores de memória e timeout são limites configurados, não medições físicas. Remoção do cartão encerra o fluxo; temporários ficam confinados na raiz.

Toda escrita, inclusive PCM, JPEG, pacotes e logs, passa pelo Storage com quota decimal de 4.000.000.000 bytes e reserva operacional. Reservas/verificações e contadores são protegidos do acesso concorrente. A fonte de erro é consultada por cópia sob mutex nos serviços externos.

## Instalador e SDK

`.t5pkg` é ZIP em perfil estrito **STORED**, sem compressão, criptografia, descritores, comentários, ZIP64 ou subpastas. Limites: 4 MiB de arquivo/extração, 32 recursos mais manifesto, 1 MiB por recurso e 64 KiB por fonte Lua. Recusa nomes duplicados por caixa, caminhos inválidos, bytecode, API incompatível, permissões desconhecidas, CRC ou SHA-256 incorretos.

O usuário inspeciona ID/autor/versão/tamanho/permissões e confirma. O hash do arquivo revisado é comparado ao arquivo efetivamente instalado. Não há assinatura nem catálogo remoto: o modo é local e explicitamente sem assinatura. Permissões suportadas: dados próprios por padrão; `documents`, `media` e `camera` são concessões adicionais. Não é possível conceder instalação, sistema ou clock por esse formato.

Payloads ficam em `/apps/<id>/<versão>/`. O registro `/apps/registry.tsv` é o ponto único de ativação, atualizado pela transação recuperável do Storage. Uma falha antes do commit mantém a versão anterior ativa. Versões inativas ficam disponíveis até remoção/reinstalação; resíduos da mesma versão inativa são limpos ao repetir a instalação. A ocupação é verificada com pacote e payload coexistindo; cada escrita confere a quota novamente.

Remover preserva `/data/<id>` por padrão. Apagar dados exige comando separado e explícito na tela de confirmação. Não apaga documentos em `/home`. Apps do sistema não podem ser substituídos pelo instalador. Pacotes de tema aplicam paleta externa e permitem restaurar o padrão, sem atualizar o BIN. Não há ainda pacotes de fontes/layouts arbitrários.

[SDK e formato](SDK.md) inclui app Lua com recurso externo, tema de exemplo e ferramenta reproduzível de criação. `sys.asset` verifica recursos próprios; módulos próprios também são verificados. Imagens próprias usam `sys.media('image','asset:arquivo.png')`, sem conceder acesso global às fotos. Limites de VM/instruções continuam valendo; não se trata de isolamento de processos nem compatibilidade SIS/SISX.

## Evidências e aceitação

As suites core/organizer/media_installer e a jornada LVGL/SDL são executadas separadamente do build ESP32-P4. As novas verificações incluem máscara pelo teclado, backspace, dropdown por setas, conversão decimal, calendário inválido, operação visual de cópia, tamanho dos botões, PCM/MP3, playlist, interrupção por alarme, WAV interrompido/ENOSPC, PNG/JPEG e câmera simulada, CRC/hash/caminhos/permissões, atualização cancelada, dados preservados e temas.

ASan/UBSan é executado nas suites sem SDL. A GUI instrumentada continua limitada pelo problema de inicialização SDL2 do host já documentado; a jornada gráfica normal é a evidência disponível.

Antes de considerar os critérios físicos das fases 4/5 aprovados, testar: “Testar som do alarme” e alarme real fora de foco; música durante Notas; retomar após alarme; microfone e WAV salvo; prévia/foto do sensor; remoção/lotações reais; teclas e touch dos dropdowns; cortes de energia na ativação do pacote. Não foi executado flash, monitor ou acesso serial pelo agente.

## Custo residente dos codecs

Objetos separados para atribuição do linker, no build 0.5.0. Valores da ferramenta `esp_idf_size`, não medidas de heap/pilha. Tabelas, auxiliares compartilhados e TLS seguem a atribuição do linker; detalhes em `size-files-0.5.json`.

| Biblioteca | Flash atribuída (bytes) | RAM estática atribuída (bytes) |
|---|---:|---:|
| minimp3 — MP3 | 17550 | 0 |
| stb_image — JPEG/PNG | 20125 | 37 |
| stb_image_write — JPEG | 8171 | 4 |

Justificativa: decodificação por amostra/pixel e JPEG precisam de backend nativo para streaming e memória controlada. Interfaces, filas/playlists, seleção de arquivos e recursos permanecem no SD. Os codecs de imagem têm orçamento combinado de 8 MiB por worker; esse teto não representa consumo contínuo nem inclui buffers de entrada/saída.
