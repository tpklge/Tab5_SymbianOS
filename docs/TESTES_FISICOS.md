# Roteiro de bancada — ainda não executado

Este roteiro não autoriza flash, monitor nem serial. Usar cartão de teste e cópias dos documentos. Registrar revisão da unidade, runtime/release/hashes, fonte de alimentação e cartão em cada ensaio.

1. Identificar SoC e painel/touch. Conferir boot sem cartão e mensagem de recuperação. Fonte de emergência deve funcionar sem nenhum asset externo.
2. Inserir a pasta SymbianOS verificada em FAT32, sem alterar pastas irmãs. Conferir imagem em retrato e toque nos quatro cantos.
3. Conectar o teclado oficial pela Ext.Port1; conferir endereço 0x6D, SDA 0, SCL 1 e INT 50 na revisão física. Digitar texto com acentos e maiúsculas; verificar press/release, modificadores combinados, repetição, Tab/Shift+Tab, Enter, Esc e atalhos. Desconectar/reconectar durante uma edição; não pode restar tecla presa. Não substituir esse ensaio por teclado USB.
4. Abrir Notas, salvar, alternar para Calculadora e voltar. Reiniciar e conferir o documento. Digitar também pelo teclado virtual. Conferir seleção parcial e clipboard entre apps.
5. Com o sistema parado, modificar chrome.lua/notes.lua e regenerar hashes. Reiniciar com o mesmo runtime e demonstrar mudança. Retirar arquivo essencial ou adulterar seu hash e verificar recuperação, sem ambiente alternativo embutido.
6. Ensaiar cartão removido/reinserido, fisicamente cheio e somente leitura. Preparar cópia com quota excedida; deve recusar o shell sem apagar documentos. Conferir pastas irmãs antes/depois.
7. Em cartão descartável de teste, interromper alimentação nas etapas de gravação e reiniciar. Verificar documento anterior/final e identificação de transação incompleta. FAT pode sofrer corrupção; recuperar a partir de backup quando necessário.
8. Registrar heap interna, PSRAM, maior bloco livre, pilhas das tarefas, latência de entrada/SD e renderização em jornadas definidas. O tamanho estático do linker não substitui essas medições.
9. Conferir probes dos codecs/RTC. Fazer ensaio funcional de saída/captura de áudio e persistência do RTC com fixture de driver apropriada. O probe atual só mostra ACK/registradores; ainda não demonstra essas capacidades.
10. Integrar/ensaiar transporte C6 com versões host/slave identificadas antes de anunciar rede. Não embutir nem atualizar silenciosamente firmware do C6 e não persistir credenciais fora de SymbianOS.

Registrar resultados reais e falhas. Só então avaliar a passagem dos critérios físicos das fases 0 e 1 descritos no plano.

## Aceitação adicional da versão 0.3.0

- Confirmar paisagem 1280×720, sentido da rotação de 90° e toque nos quatro cantos com o teclado na montagem normal. Verificar fundo, campos e teclado virtual escuros, sem texto ilegível.
- Alternar três apps com textos diferentes, abrir um quarto para forçar checkpoint/LRU, fechar e retomar. Conferir cursor/rolagem e arquivos de sessão. Ensaiar falta de espaço durante o checkpoint: o app saudável deve permanecer em memória.
- Cadastrar/buscar/editar/excluir contatos e compromissos; importar/exportar vCard/agenda; conferir caracteres acentuados. Confirmar exclusão e verificar que cancelar não muda o registro.
- Ajustar relógio e fuso; conferir a informação de persistência do RTC, reiniciar e validar horário offline. O marcador UTC só pode ser usado após escrita bem-sucedida deste sistema. Não assumir a convenção herdada de outro firmware.
- Programar alarmes/avisos com Agenda/Relógio fora de foco. Confirmar som físico, perfis Normal/Silencioso/Offline, soneca e ausência de duplicação ao ajustar a hora. Ensaiar também durante uma cópia grande; a avaliação do prazo não deve esperar o filesystem.
- Verificar desativação física do recurso Wi-Fi no perfil Offline. Isso não valida associação à rede, transporte C6 ou credenciais.
- Copiar/mover/renomear e excluir arquivo/pasta; conferir propriedades, ocupação e saldo. Usar cópias descartáveis para testar destinos existentes, somente leitura, volume cheio e cancelamento.
- Criar backup, alterar documentos, restaurar com confirmação e reiniciar. Interromper energia em staging e depois do journal, com cartão de teste, para conferir recuperação e preservação dos arquivos anteriores. Hash não elimina o risco de corrupção do FAT.
- Medir pilhas, PSRAM/heap, latência de entrada e custo do áudio/rotação. O helper de teste injeta ENOSPC em host; isso não é um ensaio de cartão fisicamente cheio.

## Aceitação 0.7.0

- Compare a estimativa e tensão do pack com instrumento apropriado e com o monitor do launcher. Teste leitura indisponível, bateria baixa e carga real (sinal da corrente). Porcentagem por tensão não demonstra capacidade/autonomia.
- Mesma fonte/cabo/bateria e carga de uso: compare corrente e evolução em Normal e QC; confira Pausar. Não há medição de carga rápida nesta entrega. Verifique a restauração da preferência ao reiniciar.
- Reprodução de um arquivo WAV/MP3 conhecido por pelo menos cinco minutos: volume 0/50/100, pausa, retomada, próxima, parar e Início. Repita com leitura do SD e rede em uso; registre modelo do cartão e falhas. Confirme que o volume persiste e vale para alarmes/gravações reproduzidas.
- Abra a câmera, confirme detecção e imagem, foto/vídeo/galeria, sair e reabrir. Se ainda falhar, registre o texto completo da etapa/erro e revisão do sensor. O driver SC202CS foi incluído; host não comprova sensor real.
- Rede: conferir C6/firmware identificado em logs/network.txt; associar, senha errada, sem C6 compatível, cancelar e desconectar. Falha opcional não deve reiniciar o shell.
- Transferência: abrir a página do IP, token errado/correto, enviar/baixar e conferir hash, cancelar, uploads paralelos, quota cheia, queda do Wi-Fi e remoção do SD. Com serviço encerrado o token antigo deve parar de funcionar. Configurações e pastas fora de home/Transfer não devem estar acessíveis.
- Downloader/feeds: HTTPS válido, certificado inválido, URL final/redirect, perda de rede e cancelamento. Feed inválido não pode substituir o snapshot anterior. Memória, desempenho conjunto e queda de energia continuam pendentes de medição.

## Aceitação 0.7.1

- Câmera: abrir, conferir imagem, fotografar, sair e reabrir; testar vídeo/galeria. O erro 22 em Configurar RGB565 foi tratado com requisição de captura explícita e preservação do padrão válido do CSI. Se persistir, registrar nova etapa/erro.
- Wi-Fi: Buscar redes, atualizar resultados, selecionar o SSID de 2,4 GHz e conectar ao roteador WPA2/WPA3. Conferir senha mascarada, cancelar busca, tentar senha incorreta, queda do AP e leitura de logs/network.txt. Confirmar se houve associação e IP DHCP, sem atribuir genericamente toda falha às credenciais/C6.

## Aceitação do residente 0.7.2

- Abrir Câmera em ambiente iluminado e observar a convergência de exposição/ganho. Alternar a direção para área mais clara e mais escura, conferindo luminosidade e cores. Registrar tempo de resposta e qualquer erro do ISP/sensor.
- Medir fps/latência reais da prévia, com áudio e sem áudio. Alvo de software até 20 fps não comprova esse desempenho na unidade.
- Fotografar e gravar vídeo com a prévia ativa; conferir exposição dos arquivos, cadência do AVI até 5 fps e responsividade de Voltar/Início. Sair/reabrir a câmera repetidamente e verificar memória/retomada do ISP.

## Aceitação 0.8.0

O usuário confirmou envio de arquivo bem-sucedido após a correção do upload. Ainda testar: conectar e salvar, reiniciar com AP presente/ausente, reconectar após queda, desligar conexão automática por rede, perfil Offline, desconexão manual, conectar sem salvar e esquecer. Conferir que nenhuma senha aparece na interface/diagnóstico e que a preferência persiste no SD. Testar token de oito caracteres, renovação ao reabrir o serviço e invalidação ao encerrar.


## Aceitação 0.9.0 — pendente

Conferir feed HTTPS e redirects com rede real; em falha, obter `/logs/network.txt` (código TLS/flags/heap). Enviar 30 MB pela página recarregada; verificar hash no SD, repetição após perda de Wi-Fi, cancelamento e limite 64 MiB. Diagnóstico: `/logs/last-upload.txt`. Testar brilho/tela apagada com música e despertar por touch/teclado sem executar a primeira tecla. Medir corrente e comparar health.txt em ensaio prolongado.

Em cartão e dados de teste, conferir retomada temporizada de deep sleep, RTC, persistência, remoção/reinserção e instalação acima da quota. Validar boot interrompido e fallback de release; OTA no slot inativo, SHA inválido, interrupção antes/depois de selecionar boot e confirmação após 15 s. Identificar se o bootloader realmente instalado pelo launcher suporta rollback nativo. Nenhum teste físico desses foi executado pelo agente.


## Aceitação 0.10.0 — pendente

O usuário confirmou upload funcionando. Testar novo feed/cabeçalhos; se falhar, registrar network.txt com errno/TLS. Conferir três temas, Home, pastas/favoritos/lista e persistência do Menu; energia compacta; PIN por touch e teclado, rascunho antes de bloquear e retomada do app. Conferir calendário, playlists, zoom/arraste e cache de artigos sem rede. Testar suspensão em memória sem retornar ao launcher, com verificação do SD e reconexão de redes salvas. OTA e atualização própria ficam para a próxima validação física, por solicitação do autor.


## Bancada 0.17.0 — preparada, execução física pendente

O layout permanece em paisagem. Configurações permite idiomas PT/EN/ES e tons do alarme. Testes e medições registra em /SymbianOS/data/bench; anotar cartão/fonte/cabo, revisão da placa e caso executado. Cada segmento informa runtime/release, intervalo, origem e colunas. `host` não equivale a medição física.

- Multimídia: começar um registro de 5 s, usar música + Notas e disparar alarme, registrar pausa/retomada e áudio observado. Iniciar vídeo e comparar contadores/bytes/fps do intervalo; anotar resolução e temperatura. Repetir com monitor desligado para avaliar sua interferência.
- Rede: registrar 30 s durante downloads/uploads/feeds/e-mail, cancelar e interromper o AP; conferir resultado/arquivo e espaço. Não coletar senhas ou conteúdo de mensagens no relatório.
- Energia: registrar pack por 30 s; comparar condições idênticas e usar instrumento externo para potência USB/autonomia. Não inferir consumo só da porcentagem.
- OTA: Revisar OTA somente verifica arquivos; usar Atualizações para a aplicação autorizada pelo usuário. Em cópia de SD, validar hash incorreto, arquivo incompleto, rollback e corte de energia em cada etapa. Conferir suporte do bootloader efetivamente instalado.
- E-mail: arquivo binário conhecido com hash antes/depois, exportação sem sobrescrever, cancelamento SMTP e conferência de envio incerto no provedor; exclusão local após reiniciar; recebimento em pasta IMAP, restauração e troca de UIDVALIDITY. Testar falta de espaço preservando uma mensagem já cacheada.
- Contatos/idiomas: favorito A–Z, backup/restore, três tons reais, salvar idioma/reiniciar, conteúdo digitado e nomes de arquivos inalterados; PIN e teclado oficial sob carga.

O agente não executou flash/monitor/serial nem cortes de energia nesta entrega.
