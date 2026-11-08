# Correções 0.6.0 — API 5

O player segurava o lock de estado durante leitura, decodificação e escrita no dispositivo. Isso impedia comandos e navegação enquanto o dispositivo demorava. O worker agora executa I/O fora desse lock, usa cache de 64 KiB e descarta conclusões de operações antigas após Parar/Próxima. O driver deixa de reconfigurar volume e amplificador em cada pacote. A redução das interrupções foi validada com dispositivo simulado bloqueado; qualidade sonora real depende do Tab5.

- Conversor: resultado grande abaixo de Converter e Inverter, com decimal brasileiro.
- Música: controles pequenos com símbolos; playlist, pausa, parada e mudança de faixa. A biblioteca não é relida para cada comando.
- Gravador: contador com décimos, nível e lista de gravações. Voz AU/G.711 μ-law mono 8 kHz, cerca de 480 KB/minuto, quatro vezes menor que WAV PCM16 mono 16 kHz, com qualidade de voz telefônica. WAV antigo continua reproduzível. Escrita em lotes e fechamento pelo worker; arquivo vazio não é publicado. Entrada nativa passa a combinar os dois canais do microfone.
- Câmera: removida a rejeição prematura da resolução nativa 1600×1200. Captura RGB565 persistente, redução para 640×360 e prévia contínua independente da UI. Imagem ocupa a tela com obturador touch e troca foto/vídeo. Erros indicam a etapa do driver e permitem reiniciar a câmera.
- Vídeo: gravação e reprodução AVI/MJPEG 640×360, até 5 fps, sem áudio. Limite de 64 MiB de dados de quadros; duração deriva do tempo efetivo. Arquivos `.part` de vídeo interrompido são preservados, sem recuperação automática nesta versão. Não é suporte genérico a formatos de vídeo externos.
- Galeria: grade de oito miniaturas de fotos/vídeos, paginação e visualização em tela cheia.
- Menu: nove aplicativos por página, com navegação pequena separada.
- Notas: Salvar, Nova, Lista e Renomear por ícones. Documentos independentes e índice no SD; a nota antiga `document.txt` é preservada.

As implementações Lua, ícones e documentos continuam no SD. O residente oferece drivers, codecs e componentes gráficos genéricos. Nenhum conteúdo de demonstração é instalado nos documentos pessoais.

## Prévias de host

[Conversor](preview-converter-0.6.png), [player](preview-player-0.6.png), [gravador](preview-recorder-0.6.png), [câmera](preview-camera-0.6.png), [galeria](preview-gallery-0.6.png), [menu](preview-menu-0.6.png), [notas](preview-notes-0.6.png). Imagens e faixas de demonstração são sintéticas; a prévia da câmera não é captura física.

## Validação e atualização

Testes de host exercitam comandos com dispositivo bloqueado, avanço de contador/nível, AU com cabeçalho/duração válidos, geração/reprodução de AVI, notas independentes e modelo das novas interfaces. FFmpeg reconheceu e decodificou os AU e AVI gerados. ASan/UBSan verifica as suites sem SDL. O build ESP32-P4 verifica compilação e tamanho, sem comprovar periféricos físicos.

Som contínuo, microfone audível, sensor da câmera, desempenho de vídeo, latências e recuperação após corte de energia precisam de ensaio real no Tab5. Nenhum flash, monitor ou acesso serial foi executado. Atualize BIN e SD juntos conforme [o procedimento do launcher](ATUALIZACAO_LAUNCHER.md); o ZIP de atualização preserva os documentos existentes.
