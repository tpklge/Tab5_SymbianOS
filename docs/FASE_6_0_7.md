# Versão 0.7.0 / API 6 — energia, áudio e fase 6

## Bateria e carregamento

A barra superior mostra um ícone de bateria e porcentagem perto do relógio. `+` indica corrente de carga detectada; `--%` indica leitura indisponível. A leitura do INA226 (I²C 0x41, shunt de 5 mΩ) ocorre fora da tarefa gráfica, a cada dois segundos. O percentual é estimado pela tensão de um pack 2S, não é um contador de carga calibrado; carga e consumo alteram a estimativa.

Menu → Energia e volume permite carga Normal, Rápida/QC e Pausar. O residente controla CHG_EN (expansor 0x44, P7) e nCHG_QC_EN ativo baixo (P5). A escolha fica no SD, em `/config/public/charge`. No boot, só restaura carga com monitor disponível e tensão na faixa de bateria; sem preferência prévia solicita modo normal. O ícone de carga depende da corrente medida, e não apenas do comando enviado. Não há detecção definitiva de presença do pack pelo monitor de tensão.

QC solicita o modo rápido do circuito existente; não aumenta limites elétricos por software. Fonte, cabo, temperatura, carga do sistema e estado da bateria determinam o ganho real. A [documentação do fabricante](https://docs.m5stack.com/en/core/Tab5) informa que a carga depende de o aparelho estar ligado/inicializado. Não foi medida velocidade de carga nesta entrega.

## Áudio e câmera

Música e Gravador têm Vol - / Vol +; Energia também controla o mesmo volume mestre de 0 a 100%, aplicado a reprodução e alarmes e persistido em `/config/public/volume`. É volume de saída, não ganho de gravação. O driver agora desacopla a decodificação da saída I²S com uma fila limitada a 12 pacotes PCM. Ela absorve pequenas pausas do SD; não comprova a causa dos engasgos nem elimina pausas maiores que sua capacidade. Parar limpa a fila, Pausar suspende seu consumo e a troca de faixa invalida dados antigos. O final da faixa aguarda esvaziamento antes de avançar.

O build anterior estava com todos os sensores de câmera desabilitados. Agora inclui SC202CS com autodetecção MIPI e perfil RAW8 1280×720, como na configuração de referência do Tab5. Isso corrige uma causa concreta de ausência de `/dev/video0`/erro 2. Uma abertura sem sensor detectado reinicializa o subsistema para nova tentativa. Não há alegação de imagem física validada nem garantia para outra revisão de sensor. Prévia 640×360, foto JPEG e vídeo AVI/MJPEG até 5 fps sem áudio permanecem.

## Fase 6 implementada

- Rede: conexão manual a SSID de 2,4 GHz, senha mascarada e usada em RAM, desconexão, cancelamento e estado real de associação/IP. Não salva senha no SD/NVS do aplicativo.
- Transporte: ESP-Hosted 1.4.0 + Wi-Fi Remote 0.8.5, fixados no lock, para ESP32-C6/SDIO. Pinos Tab5: CLK12, CMD13, D0=11, D1=10, D2=9, D3=8, reset15; clock inicial de 20 MHz. Não usa os pinos padrão da placa EV da Espressif.
- O rádio é iniciado sob solicitação, fora do boot. A integração exclui o construtor automático do componente e compila um overlay local que propaga falha do transporte sem `abort`, limita tentativas a 15 ciclos e verifica cancelamento. O código original/licenças permanecem em `managed_components`; alterações ficam explícitas no CMake e em `build/tab5-hosted`.
- Transferência local: Rede → Ativar transferência. Abra o IP no computador e informe o token temporário exibido. A página externa do SD oferece listar/enviar/baixar, com arquivos limitados a 64 MiB. Pare o serviço para invalidar o token. HTTP local sem TLS: use uma rede de confiança.
- Todos os endpoints de arquivos exigem `X-Tab5-Token` de 128 bits. Só expõem arquivos diretos de `/home/Transfer`, nunca a raiz inteira ou configurações. Sem substituição de arquivos existentes. Uma operação de escrita de rede por vez; quota e espaço físico são conferidos pelo Storage compartilhado. Interrupções apagam os temporários dessa operação, sem publicá-los como arquivos completos. Corte de energia pode deixar `.part` dentro da raiz, contando na quota.
- Downloader: URL final HTTP(S), até 64 MiB, resultado em `/home/Transfer`. TLS usa bundle de autoridades; não há opção de ignorar validação. Redirecionamentos, autenticação HTTP e retomada parcial não são implementados; informe a URL final.
- Feeds: download RSS/Atom de até 64 KiB e leitura de até 20 títulos, com snapshot no SD. Recusa DTD/declarações de entidades e preserva o snapshot anterior em falha. É um leitor limitado de títulos, sem navegador, HTML, XML genérico ou sincronização de contas.

O firmware do C6 precisa ser compatível. Nada o atualiza automaticamente, e nenhum firmware de C6 foi embutido ou gravado. A referência oficial arquivada neste projeto usa ESP32C6-WiFi-SDIO-Interface V1.4.1; a compatibilidade da unidade com o host precisa de teste físico. Sem resposta do C6, o sistema deve continuar utilizável com diagnóstico de rede. Não implementa mensagens, BLE, e-mail ou mapas; o transporte de mensagens continua a ser escolhido separadamente, como prevê o plano.

## Validação e uso

Host: núcleo, organizador, mídia, instalador e novos testes de energia/rede, incluindo limites de volume, estados, máscaras de senha, cancelamento, caminhos, arquivos existentes, falhas/parciais, feeds e transferência por streaming. ASan/UBSan cobre as suites sem SDL. Build ESP32-P4 e relatório de tamanho são separados. Os testes de backend usam dispositivos/transportes simulados; não provam rádio, áudio, sensor, autonomia, corrente ou carregamento físicos.

Atualize BIN e SD juntos: a release 0.7.0 requer API 6. Preserve `/data`, `/home`, `/config` e versões anteriores. [Atualização pelo launcher](ATUALIZACAO_LAUNCHER.md). O usuário confirmou funcionamento do gravador na revisão anterior; player e câmera tiveram falhas físicas relatadas. A aceitação final destas correções e da fase 6 ainda depende do Tab5. Nenhum flash, monitor ou acesso serial foi executado.

Prévias sintéticas do simulador: [Música](preview-music-0.7.png), [Energia](preview-energy-0.7.png), [Wi-Fi](preview-network-wifi-0.7.png), [Rede](preview-network-0.7.png), [Feeds](preview-feeds-0.7.png), [Câmera](preview-camera-0.7.png).
