# 0.10.0 / API 10 — experiência pessoal

Esta entrega implementa a fase 8 acrescentada ao planejamento e mantém o núcleo das fases anteriores. As extensões Meshtastic/RAK, BLE, e-mail offline e aplicativos próprios SIS/SISX/JAR foram planejadas; ainda não são provedores nem runtimes disponíveis.

## Instalação e validação

Atualize BIN e SD juntos pelo método já utilizado com o launcher. Extraia `SymbianOS-update-0.10.0.zip` preservando dados/configurações e use `SymbianOS-runtime.bin`. A release exige API 10; o runtime aceita manifestos de APIs 1–10. Recarregue a página de transferência. O usuário confirmou upload funcionando na 0.9.0; o protocolo de blocos de 64 KiB, o limite de 64 MiB e os testes de integridade foram preservados.

OTA e atualização própria permanecem para validação física na próxima versão, conforme solicitado. Nenhum flash, monitor, acesso serial ou publicação no GitHub foi executado pelo agente.

## Feed e rede

O relato evoluiu de conexão TLS para falha ao receber cabeçalhos HTTP. O cliente passa a receber em buffer de 16 KiB, declara RSS/Atom/XML em Accept, solicita conteúdo sem compressão, encerra a conexão explicitamente e usa identificação compatível com servidores web. Falhas de cabeçalho sem corpo recebido podem repetir a requisição uma vez. `/logs/network.txt` agora informa retorno da leitura dos cabeçalhos, errno, erro TLS e flags de certificado. As proteções de redirecionamento e validação de cadeia/hostname permanecem.

A coleta HTTP/1.1 no computador recebeu RSS do endereço Gizmodo com sucesso; isso não comprova funcionamento do cliente ESP32-P4. O teste físico desse caminho segue pendente. Erros HTTP, páginas de proteção e conteúdo que não seja RSS/Atom continuam visíveis; não são tratados como feed válido.

O app agora tem assinaturas nomeadas, lista paginada de artigos, leitor em largura completa, cache por assinatura e artigos salvos para leitura sem rede. Há até 16 assinaturas, 30 artigos por snapshot e 32 artigos salvos. O limite do RSS permanece 64 KiB. O leitor exibe o texto fornecido pelo feed, sem executar HTML ou JavaScript, e preserva o link original; não baixa automaticamente a página completa da matéria. Após baixar, “Atualizar lista” carrega os artigos. Arquivos de índice inválidos não são substituídos silenciosamente.

## Energia e suspensão

Energia deixou de apresentar uma longa coluna de comandos. A página principal possui Tela, Carregamento, Som e Suspender; cada categoria usa seletores e poucos botões. Brilho, tempo de tela, modo de carga e volume permanecem no SD. Configurações também dá acesso à aparência, energia, relógio e rede. Orientação continua fixa em paisagem.

O caminho anterior usava deep sleep, que reinicia a CPU e podia voltar ao boot padrão do launcher. A suspensão temporizada agora usa **light sleep com a aplicação preservada em memória**. Faz checkpoint, para mídia/rede, pausa os acessos ao armazenamento, apaga a iluminação e programa o temporizador. Ao retornar, reaudita o SD, retoma no mesmo runtime e permite conexão automática de redes salvas quando apropriado. Não altera seleção de boot, partições ou firmware do launcher. Erro/rejeição do sleep é apresentado ao usuário. Consumo, entrada/retomada e comportamento dos drivers ainda precisam de teste no Tab5.

Não equivale ao apagamento simples da tela: nesse último, música e serviços continuam. Na suspensão temporizada, mídia/rede param e alarmes são verificados ao retornar; não há promessa de despertar no horário de um alarme durante esse período.

## Home, Menu e temas

A Home traz relógio, perfil/notificações, até três atalhos de favoritos e próximos três compromissos. Menu, Configurações e Avisos permanecem acessíveis. O Menu mantém nove aplicativos por página, permite grade/lista, filtro por favoritos, pastas Pessoal/Multimídia/Ferramentas e ordenação alfabética ou por posição. “Organizar” salva pasta, favorito, posição e apresentação. As pastas são categorias predefinidas nesta versão; criação de nomes arbitrários é evolução posterior.

Três temas originais ficam inteiramente no SD:

- **Aurora:** diagonais em azul, violeta e verde oceânico.
- **Solar:** círculos solares e planos de horizonte em cores quentes.
- **Prisma:** mosaico de triângulos coloridos.

Cada tema inclui paleta e papel de parede RGB565 de 320×180, escalado para a tela. O runtime carrega somente o papel selecionado; não contém os assets na flash. `tools/make_themes.py` reproduz os desenhos originais; PNGs de referência ficam em artwork/. Escolher um tema integrado restaura sua paleta; temas de pacotes continuam podendo ser reaplicados pelo Instalador. As prévias em docs/preview-*-0.10.png são do simulador, com bateria/driver simulados, não fotografias da placa.

## Bloqueio por PIN

Configurações → Segurança permite criar, alterar e desativar PIN numérico de quatro dígitos. A tela de bloqueio possui teclado numérico touch e aceita o teclado físico. O bloqueio é exigido no boot e ao apagar a tela; também há “Bloquear agora”. O runtime impede abertura de outros apps enquanto bloqueado. Antes de trocar para o bloqueio, preserva o texto ainda não salvo do editor.

O cartão guarda verificador SHA-256 iterado com sal aleatório, sem PIN em texto. Cinco erros aplicam espera de 30 segundos; tentativas persistidas impedem eliminar essa espera somente reiniciando. Campos e ações de segurança não entram nos logs como texto de PIN. Esse bloqueio protege a interface; o cartão removível não é criptografado e outro firmware não fica sujeito à política deste runtime.

Para recuperar um PIN esquecido, desligue/pare o sistema e remova `/SymbianOS/config/security/pin` e `attempts` no computador. Não apague documentos. Uma release antiga sem tela de bloqueio deve ser usada após desativar o PIN; o runtime novo recusa abrir uma release sem o componente de bloqueio se o PIN estiver configurado.

## Agenda

A vista mensal tem dias selecionáveis e marcador de compromissos; a semanal lista os sete dias com contagem/resumo. Há navegação de dias/meses e acesso ao dia selecionado. O editor usa campos separados de título, data DD/MM/AAAA, hora HH:MM, descrição e tipo. Registros/importação anteriores são preservados. Exportação TSV, tarefas, notificações e alarmes existentes continuam disponíveis.

## Música

Biblioteca paginada até 1024 arquivos na pasta Music e busca por nome. Playlists nomeadas podem ser criadas, renomeadas, preenchidas, reordenadas e excluídas sem excluir músicas. Até 32 playlists, com 256 faixas por lista/fila nativa. Repetição desligada/toda a fila/uma faixa e ordem aleatória ficam persistidas. Aleatório embaralha a fila preservando a faixa selecionada como primeira; sem repetição, termina ao percorrê-la. Volume e controles compactos existentes permanecem. Subpastas recursivas e biblioteca de tags/álbuns não fazem parte desta entrega.

## Galeria

Fotos podem ser ajustadas à tela ou ampliadas em 2×/4×, com área deslocável por arraste. Há anterior/próxima mídia e exclusão confirmada. Vídeo mantém o perfil AVI/MJPEG anterior. O zoom amplia uma imagem decodificada com limite de resolução; não é editor fotográfico nem zoom ilimitado no sensor. A navegação sequencial é limitada aos primeiros 516 arquivos visuais da pasta; a grade continua paginada.

## Evidência e limites

Treze suites de host cobrem as políticas existentes e novos fluxos de temas, Home/Menu, agenda, playlists, galeria, assinaturas/leitura offline e PIN com bloqueio/reboot/espera. A jornada LVGL/SDL verifica teclado touch do PIN e preservação de rascunho ao bloquear. ASan/UBSan cobre o núcleo sem SDL; não instrumenta o motor JavaScript dos testes de upload. Compilação ESP32-P4 e tamanho são registrados em BUILD.md.

Para caber no slot de 2 MiB, o perfil residente preserva logs WARN/ERROR e os diagnósticos em arquivo, sem logs INFO gerais, e remove métodos TLS legados sem uso. Mantém TLS 1.2 com ECDHE-RSA/ECDHE-ECDSA e conjunto completo de CAs. Não altera a tabela de partições. A alternativa LTO foi descartada por incompatibilidade do toolchain fixado; não é necessária para reproduzir o build.

Os testes não substituem a aceitação física de cabeçalhos HTTP, suspensão, consumo, touch/teclado, áudio concorrente e uso prolongado. OTA/rollback próprios não foram exercitados na placa nesta versão.
