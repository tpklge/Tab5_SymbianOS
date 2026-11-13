# Atualizar pelo launcher — versão local 0.10.0 / API 10

Entrega vigente: `sist/0.19.0/launcher/SymbianOS-runtime.bin` e `sist/0.19.0/launcher/SymbianOS/`, preparados por `python3 tools/release.py`. Atualizações são feitas somente na WebGUI do launcher; o runtime não oferece OTA interno. O envio web por token dentro do SymbianOS serve apenas para arquivos. Preserve dados existentes no cartão ao mesclar a pasta.

Versão local **0.10.0/API 10**, [recursos e limites](EXPERIENCIA_0_10.md): atualize `dist/SymbianOS-runtime.bin` e extraia `dist/SymbianOS-update-0.10.0.zip` juntos, preservando dados/configurações. Recarregue a página web de transferência. Depois dessa migração, há também [kits para o atualizador no SD](FASE_7_0_9.md); eles não atualizam o bootloader do launcher.


Esta atualização exige **BIN e SD juntos**. Inclui redes conhecidas, reconexão automática, token curto e correção do upload HTTP. O ZIP de atualização não substitui documentos/configurações existentes. [Detalhes](REDES_SALVAS_0_8.md).

1. Abra WUI/WebUI no launcher e acesse, pelo computador, o IP mostrado no Tab5.
2. Faça backup da pasta atual `/SymbianOS`, especialmente `/data`, `/home` e `/config`, com o sistema parado.
3. Extraia `dist/SymbianOS-update-0.10.0.zip` no computador. O pacote contém somente a nova release, o manifesto e entradas de diretório. **Não contém documentos, sessões ou preferências para substituir os seus.**
4. Pela área Files, envie `/SymbianOS/system/releases/0.10.0/` completo. O runtime cria somente as pastas de dados/configuração ausentes, depois de validar a instalação. Documentos e preferências existentes não são substituídos; colisão com um arquivo gera diagnóstico.
5. Envie `/SymbianOS/boot/manifest.txt` por último. Preserve as pastas antigas e todos os documentos.
6. Pela área OTA da WebUI, selecione `dist/SymbianOS-runtime.bin` e confirme a instalação em uma partição de aplicativo que comporte o tamanho indicado em `dist/release.json`. Esse binário é somente a aplicação ESP32-P4.
7. Inicie o aplicativo pelo launcher. Confirme fundo/campos escuros, texto claro, orientação com teclado, toque nos cantos e navegação.

Como alternativa ao ZIP, `sist/0.19.0/launcher/` já contém o BIN e a árvore de atualização organizados para envio manual. O pacote completo `dist/SymbianOS-sdcard.zip` fica disponível para uma instalação inicial; não o use para substituir indiscriminadamente uma instalação com dados.

Não envie `bootloader.bin`, `partition-table.bin` nem `ota_data_initial.bin` pelo fluxo de aplicativo do launcher. Os arquivos de `runtime-installation/` descrevem apenas uma instalação independente. Nenhuma operação de flash foi executada pelas ferramentas de preparação.

Depois da atualização, menu e alternador dão acesso a Notas, Calculadora, Contatos, Agenda, Relógio/alarmes, Conversor, Arquivos, Backup e Perfis. No primeiro ajuste de data/hora, Relógio tenta salvar o RTC como UTC e informa se só foi possível ajustar a sessão atual. Não interpreta silenciosamente a convenção de horário que outro firmware deixou no RTC.

Se a paisagem estiver invertida em relação à montagem física do teclado, registrar esse resultado para ajustar o sentido da rotação. Não há promessa de validação dessa montagem sem o ensaio real.

Se ainda ocorrer tela azul/reinício, preserve `/SymbianOS/logs/last-action.txt` e `/SymbianOS/logs/boot.txt`. Eles registram etapa, memória/pilha e motivo numérico do reset, sem conteúdo dos documentos. São pistas de diagnóstico, não um backtrace.

Após atualizar, abra Relógio → Testar som do alarme. Se o driver falhar, preserve `/SymbianOS/logs/last-audio.txt`. Em seguida teste um alarme real fora de foco, os dropdowns, os campos DD/MM/AAAA e HH:MM e a cópia no gerenciador. Novos apps ficam nas páginas seguintes do Menu.

Arquivos pessoais de mídia vão em `/SymbianOS/home/Music`, `/home/Recordings` e `/home/Pictures` (todos sob SymbianOS). O runtime cria pastas ausentes. Pacotes de exemplo em `sist/0.19.0/packages/` são opcionais: copie-os para `/SymbianOS/update/packages` e instale pela interface. O SDK está em `dist/SymbianOS-SDK-0.10.0.zip`.

Na 0.7, teste Parar/Anterior/Próxima durante música e Início durante reprodução. Grave alguns segundos de voz, confirme contador/nível e reproduza o AU salvo. Na câmera, teste fotografia e vídeo; em caso de falha, preserve a mensagem exibida. Vídeo AVI/MJPEG 640×360, até 5 fps e sem áudio. Veja [correções e limites](CORRECOES_0_6.md).

Na 0.7 teste também Energia → Normal/QC/Pausar, corrente e porcentagem, Vol -/+ durante reprodução e após reiniciar. Rede → Conectar usa somente o C6 já existente; não grava firmware nele. Uma incompatibilidade requer diagnóstico separado. Teste compartilhar, cancelar upload, Wi-Fi desligado e snapshot dos feeds.

Na 0.8.0, reabra Câmera para testar a correção de Configurar RGB565/erro 22. Rede → Buscar redes Wi-Fi → Atualizar resultados → selecione o SSID e informe a senha. Em caso de falha, consulte logs/network.txt, agora com etapa/motivo detalhado. [Detalhes da correção](CORRECOES_0_7_1.md).

Depois de atualizar, Rede → Buscar redes → selecione SSID → Conectar e salvar. Confira Redes salvas, reinicie e teste a conexão automática. No serviço de transferência, use o novo token de oito caracteres. Prefira Conectar sem salvar em redes temporárias. As credenciais salvas ficam no SD e não são criptografadas.
