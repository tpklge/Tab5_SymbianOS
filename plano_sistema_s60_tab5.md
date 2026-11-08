# Plataforma para M5Stack Tab5 inspirada no S60 5th Edition

Plano revisado — 7 de novembro de 2026.

## 1. Objetivo e requisitos obrigatórios

Desenvolver uma plataforma própria para o M5Stack Tab5, com aparência, navegação, aplicativos e serviços semelhantes ao S60 5th Edition do Nokia 5230. A plataforma utilizará ESP-IDF/FreeRTOS e um runtime residente enxuto. O shell, a lógica dos aplicativos, os recursos visuais e todos os arquivos persistentes do sistema serão carregados do microSD.

Os seguintes requisitos prevalecem em toda a implementação:

1. **Raiz única:** a pasta física `/SymbianOS` do microSD será apresentada ao sistema como sua raiz lógica `/`.
2. **Leitura e gravação confinadas:** configurações, documentos, apps, temas, logs, caches, temporários, backups e atualizações permanecerão nessa pasta. Não criar arquivos irmãos na raiz do cartão.
3. **Limite total:** o conteúdo persistente de `/SymbianOS` terá quota de **4 GB = 4.000.000.000 bytes**, somando sistema e dados. A interpretação decimal está definida para evitar ambiguidade com 4 GiB.
4. **Binário mínimo operacional:** a flash conterá apenas infraestrutura nativa necessária para inicializar a placa, acessar o cartão, executar o runtime e oferecer as APIs do hardware e recuperação. Não incorporar home, agenda, notas, ícones, temas ou dados de usuário ao binário.
5. **Sistema externo desde o primeiro protótipo:** o shell e os aplicativos essenciais já devem vir do cartão. Não adiar essa separação para uma etapa posterior.
6. **Teclado oficial obrigatório:** suportar o M5Stack Tab5 Keyboard por I²C na Ext.Port1, além do touch. Teclados USB HID são suporte adicional, sem substituir esse requisito.
7. **Estado recuperável:** o sistema deve persistir dados no cartão, controlar espaço, atualizar arquivos de forma recuperável e ter procedimento de recuperação.
8. **Sem cartão, sem sistema completo:** o runtime exibe apenas diagnóstico/recuperação; não inicia uma cópia escondida do ambiente na flash.
9. **Separação de atualizações:** atualizar o conteúdo do sistema no cartão deve ser possível sem regravar o runtime, enquanto a versão da API nativa continuar compatível.
10. **Funcionalidade real:** agenda notifica, notas persistem, música continua fora do player e aplicativos externos realmente executam. Telas demonstrativas não equivalem a funções concluídas.

A quota é de armazenamento, não de RAM. O cartão pode ter capacidade maior e outras pastas utilizadas por outros projetos; o sistema não deve alterá-las.

Nenhum firmware foi compilado ou medido no Tab5 durante este planejamento. Orçamentos e latências são metas a validar fisicamente.

## 2. Referência histórica e documentação

### 2.1 Alvo correto

Referência principal: S60 5th Edition sobre Symbian OS 9.4, com a experiência do Nokia 5230. Não misturar automaticamente recursos exclusivos do N97, do Symbian Anna ou do Belle. A biblioteca v2.1 consultada contém também APIs específicas do N97; elas precisam ser identificadas antes de virar requisito [S1–S3].

O manual do 5230 registra, entre outras funções, contatos, mensagens, perfis, reprodução musical, câmera, galeria, relógio, calendário, arquivos, gravador, notas, calculadora, conversor e gerenciador de aplicativos. Também descreve alternância entre programas e música em segundo plano [S6]. Esse conjunto orienta a seleção funcional; serviços atuais serão implementações próprias.

### 2.2 O que aproveitar da arquitetura

A documentação separa o framework gráfico genérico Uikon da camada S60/Avkon. O modelo de aplicação distingue identidade, dados persistentes, controle da interface e vistas [S4]. Adotaremos essa separação conceitual, com APIs novas e adequadas ao Tab5.

Na interface, os documentos descrevem o painel superior de estado e navegação, a área central de conteúdo e os comandos inferiores. O painel superior admite variantes conforme o aplicativo [S5]. Recriaremos esses papéis em componentes compartilhados, sem exigir que cada aplicativo desenhe sua própria moldura.

O toque no S60 não tem uma regra universal: certas listas primeiro selecionam e depois ativam o item selecionado, enquanto menus de opções e outros controles ativam com um toque [S2]. A política será definida por componente; “segundo toque para ativar” não será confundido com um duplo toque sujeito a intervalo curto.

### 2.3 Referência do Macintosh no Tab5

Projeto localizado: **amcchord/M5Tab-Macintosh**, baseado no BasiliskII [S19]. Seu firmware contém o emulador, enquanto o cartão fornece ROM, disco do Macintosh e arquivos de configuração. Essa divisão orienta o nosso projeto: infraestrutura de execução residente e conteúdo do sistema externo.

O Macintosh emula uma máquina 68k. Aqui construiremos uma plataforma nova, com shell e apps interpretados pelo runtime. Não é necessário introduzir emulação ARM/68k ou uma imagem de disco para atender à raiz em pasta solicitada.

### 2.4 Base técnica

Recomendação: ESP-IDF/FreeRTOS, LVGL e Lua incorporado [S17], com drivers e APIs de hardware em C/C++. A demonstração oficial da M5Stack oferece referências de integração e compilação desktop [S8–S10].

O NuttX é uma alternativa a reavaliar se precisarmos de um ambiente POSIX mais amplo. A documentação do port do Tab5 ainda lista lacunas em rede, áudio, câmera, microSD e RTC [S11]. Isso aumenta o trabalho necessário para o primeiro sistema utilizável.

Não portar o kernel Symbian nem reproduzir sua ABI é uma decisão deste planejamento. Compatibilidade com programas SIS/SISX exigiria outro projeto.


## 3. Hardware-alvo e consequências

O Tab5 documenta ESP32-P4 RISC-V, 32 MB de PSRAM, 16 MB de flash, ESP32-C6 para conectividade e tela touch de 1280 × 720. Inclui microSD, áudio com alto-falante/fone e microfones, câmera, IMU e RTC [S7].

Em retrato, 720 × 1280 corresponde exatamente a 2 × 360 × 640 por eixo. Essa relação permite começar com um espaço de desenho lógico de 360 × 640 e transformar dimensões e coordenadas de entrada. Não exige renderizar tudo em baixa resolução: fontes e imagens podem ser desenhadas diretamente na resolução física.

Antes da implementação, identificar a revisão do painel/touch e do SoC. Há variantes de display documentadas [S11]. Não presumir que a frequência anunciada para uma revisão nova se aplica à unidade física do usuário.

| Capacidade | Uso planejado | Condição |
|---|---|---|
| Display e touch | Interface principal em retrato; paisagem adaptativa | Validar driver, rotação e coordenadas nos quatro cantos. |
| Tab5 Keyboard I²C | Entrada principal física e navegação | Obrigatório desde a fase 0; testar reconexão, modificadores e acentos. |
| Teclados USB HID | Entrada física adicional | Adaptador separado, convergindo para os mesmos eventos de entrada. |
| microSD | Raiz completa `/SymbianOS` e todos os arquivos persistentes | Obrigatório para iniciar o ambiente; ausência deve abrir diagnóstico. |
| Áudio | Player, gravador, alarmes e avisos | Serviço central arbitra reprodução, gravação e interrupções. |
| Câmera | Captura e galeria | Validar sensor, formato, buffers e gravação. |
| ESP32-C6 | Wi-Fi via comunicação com o P4; BLE em etapa própria | Fixar versões compatíveis de host e firmware do C6 [S12]. |
| RTC | Hora offline e futura retomada por alarme | Validar persistência e caminho real de despertar. |
| IMU | Rotação automática | Histerese e bloqueio de orientação por aplicativo. |
| Bateria | Estado energético e políticas de consumo | Calibrar estimativa; não mostrar porcentagem fictícia. |
A documentação oficial confirma que o Tab5 Keyboard comunica por I²C, com interrupção própria [S22]. A validação incluirá o acessório real, não somente injeção de teclas no simulador.


## 4. Contrato visual e de navegação

### 4.1 Telas e controles comuns

1. **Tela inicial:** relógio, data, perfil, atalhos, próximos compromissos e avisos. Propor um tema inicial inspirado no 5230.
2. **Menu:** grade de aplicativos e pastas; opção de lista; organização e favoritos persistentes.
3. **Painel superior:** título, navegação, hora, bateria e conectividade reais. Exibir Wi-Fi no lugar de representar uma rede celular inexistente.
4. **Área principal:** listas, formulários, imagens e vistas específicas.
5. **Comandos inferiores:** “Opções” e “Voltar”, “Cancelar”, “Concluir” ou “Sair”, conforme o contexto.
6. **Controles compartilhados:** listas de uma/duas linhas, seleção múltipla, abas, campos, sliders, confirmação, progresso cancelável e notificações.
7. **Alternador:** aplicativos abertos, estado de execução e ação de fechar.
8. **Bloqueio:** proteção contra toque acidental; PIN opcional em etapa posterior, sem apresentar isso como criptografia dos dados.

Os assets serão próprios, com formas, cores e proporções inspiradas na época, e serão armazenados em `/SymbianOS`. Temas devem definir cores, dimensões, fontes, fundos e ícones. O sistema de skins é uma característica documentada do S60 [S16]; o formato do novo projeto será próprio.

### 4.2 Política de entrada

| Ação | Comportamento proposto |
|---|---|
| Toque em comando ou menu de opções | Executar imediatamente. |
| Toque em lista no modo clássico | Selecionar; toque no item selecionado executa a ação principal. |
| Modo simplificado opcional | Abrir listas com um toque; seleção múltipla por modo explícito. |
| Arrastar | Rolar; nunca disparar abertura ao terminar uma rolagem. |
| Enter | Ativar a seleção. |
| Esc | Voltar/cancelar, respeitando alterações pendentes. |
| Tab e Shift+Tab | Avançar/recuar foco. |
| Atalho Menu | Abrir o menu; acionamento prolongado ou atalho separado abre o alternador. |
| Teclado na tela | QWERTY com acentos, números, símbolos e seleção de texto. |

O Tab5 não reproduz o conjunto de teclas físicas do Nokia. Acesso à tela inicial, menu e alternador deve existir por touch em todas as telas comuns, com uma pequena área de navegação ou controle sobreposto. A posição final será validada para não conflitar com os dois comandos inferiores. Apps em tela cheia terão uma forma explícita de recuperar esses controles.

Retrato será a referência inicial. Paisagem exige reorganizar os controles; esticar a tela de retrato não é uma implementação aceitável. Durante a rotação, preservar formulário, seleção e posição de leitura. Cada aplicativo declara orientações suportadas.

Teclado numérico estilo telefone e previsão de palavras ficam para depois da entrada QWERTY confiável. Reconhecimento de escrita é uma extensão independente, sem compromisso de disponibilidade na primeira versão.
### 4.3 Teclado oficial do Tab5

O driver I²C do acessório pertence ao runtime. Mapeamento, atalhos e preferências pertencem aos arquivos do sistema no cartão. O gerenciador de entrada reúne touch, teclado I²C, teclado virtual e USB em um modelo comum.

| Requisito | Comportamento esperado |
|---|---|
| Eventos | Pressionar, soltar, repetição e estado dos modificadores. |
| Edição | Letras, números, símbolos, acentos e Unicode/UTF-8 nos campos. |
| Navegação | Setas, Tab, Shift+Tab, Enter e Esc. |
| Edição comum | Ctrl+A/C/X/V e Ctrl+S em contextos compatíveis. |
| Acesso ao sistema | Atalhos configuráveis usando teclas existentes; proposta inicial Ctrl+Esc para Menu e Ctrl+Tab para Alternador. |
| Troca de foco | Limpar estados de teclas presas e entregar entrada apenas ao destino correto. |
| Conectar/desconectar | Detectar o acessório e recuperar a entrada sem reiniciar. |
| Ausência | Touch e teclado na tela continuam disponíveis. |

Ler eventos fora da rotina de interrupção, sem bloquear a UI. Validar anti-repetição indevida, composição de acentos, seleção de texto e modificadores combinados. Teclas que o acessório não possui não podem ser a única forma de acionar uma função.


## 5. Matriz de equivalência funcional

As prioridades a seguir são propostas do projeto, não afirmações de que as implementações já existem.

| Função de referência | Entrega no Tab5 | Prioridade e limite |
|---|---|---|
| Inicial, menu, atalhos e pastas | Shell persistente com navegação comum | P0: fundação. |
| Alternância de aplicativos | Gerenciador de ciclo de vida e restauração de estado | P0: parte essencial da plataforma. |
| Configurações e perfis | Brilho, volume, idioma, hora, conectividade e perfis normal/silencioso/offline | P1. Alarmes têm política explícita, separada de avisos comuns. |
| Relógio e alarmes | Repetição, soneca, tom e hora offline | P1; despertar após suspensão profunda depende do ensaio de energia. |
| Agenda | Vistas mensal/diária, compromissos e tarefas; vista semanal depois | P1. |
| Contatos | Cadastro, busca, favoritos, importação/exportação vCard | P1; uso posterior por provedores de mensagens. |
| Notas | Editor UTF-8, busca, salvar e exportar TXT | P1. |
| Calculadora e conversor | Cálculos usuais e conversão de unidades | P1. |
| Arquivos | Navegar, abrir por tipo, copiar, mover, renomear, excluir e ver propriedades | P1; operações longas canceláveis. |
| Temas e papel de parede | Pacotes externos com prévia, aplicar e restaurar | P1. |
| Música | WAV/MP3 inicialmente, pastas, fila, playlists e reprodução em segundo plano | P2; formatos dependem dos decodificadores integrados. |
| Gravador | Captura de voz em WAV com salvar e reproduzir | P2. |
| Galeria | Miniaturas, JPEG/PNG, visualização e zoom limitado | P2; processamento por demanda. |
| Câmera | Prévia e fotografia com integração à galeria | P2; buffers e estabilidade são critérios de entrada. |
| Instalação de aplicativos | Apps Lua externos desde a base; instalador e SDK em fase posterior | Sem equivalência binária com Symbian. |
| Web e feeds | Downloader, favoritos e leitor de RSS/texto; HTML limitado é investigação posterior | P2/P3; não equivale ao navegador completo. |
| Mensagens | Interface e armazenamento comuns, conectados a um provedor real | P3; escolher transporte antes de implementar envio. |
| E-mail | Cliente próprio como módulo opcional | P3; autenticação e protocolos precisam de um escopo específico. |
| Mapas e localização | Visualizador de mapas offline e integração opcional com GNSS externo | P3; navegação com rotas não está incluída inicialmente. |
| Chamadas e SMS/MMS | Exigem modem apropriado e integração de voz/dados/operadora | Extensão de hardware; uma tela de discagem sozinha não entrega a função. |
| Rádio FM | Receptor externo; rádio pela internet é outra função | Extensão de hardware ou app de streaming. |
| Vibração | Depende de atuador adicional | Não prometer feedback tátil no hardware básico. |
| Bluetooth do Nokia | BLE para funções específicas; sem equivalência com Bluetooth clássico | C6 documenta Bluetooth LE [S13]; fones A2DP clássicos e OBEX não devem ser prometidos. |
| SIS/SISX, JAR e widgets antigos | Não suportados pela plataforma proposta | Exigiriam emulação/runtime e compatibilidade de APIs separados. |
| Lojas e serviços Nokia antigos | Catálogo próprio opcional | Não depender da infraestrutura histórica. |

Para mensagens, um cliente via Wi-Fi ou integração Meshtastic por módulo externo são possibilidades. Isso não significa SMS nem garante que toda mensagem tenha confirmação de entrega. O provedor deve informar capacidades e estados reais: pendente, enviado, confirmado quando houver protocolo para isso, falhou e recebido.

## 6. Arquitetura: runtime residente e sistema no cartão

### 6.1 Separação obrigatória

| Componente | Local de origem | Motivo |
|---|---|---|
| Bootloader, tabela de partições e metadados OTA | Flash | Inicialização e seleção do firmware. |
| FreeRTOS, drivers e adaptador do Tab5 | Runtime na flash | Acesso nativo ao hardware. |
| FatFs/VFS e serviço de quota | Runtime na flash | Montar e mediar o acesso à raiz. |
| Interpretador Lua, renderizador LVGL e API do sistema | Runtime na flash | Executar o conteúdo externo com recursos controlados. |
| Diagnóstico mínimo, texto/fonte de emergência | Runtime na flash | Informar falhas mesmo sem cartão ou arquivos válidos. |
| Shell, home, menu e alternador | `/SymbianOS/system/releases/<versao>/` | Carregados do cartão em cada inicialização. |
| Lógica dos aplicativos essenciais | Release do sistema no cartão | Não exigir regravação para mudar agenda, notas ou calculadora. |
| Definições de telas, temas, fontes e ícones | Release, temas e fontes no cartão | Reduzir o conteúdo incorporado ao binário. |
| Preferências, documentos e dados de apps | Diretórios próprios dentro da raiz | Persistência única e portabilidade. |
| Cache, logs, backups e atualização | Dentro da raiz | Contabilização e recuperação coerentes. |

O ESP32 inicia o firmware pela infraestrutura de boot do chip. Não tratar um `.bin` copiado ao cartão como um executável que pode receber um salto direto. Na primeira arquitetura, os arquivos de código do sistema externo são scripts Lua e descrições de interface executados pelo runtime.

O residente será mais que um carregador de arquivos: precisa incluir interpretador, renderização e APIs nativas. Ainda assim, não conterá a lógica do ambiente e dos aplicativos. Código nativo adicional carregável do SD só será admitido após uma prova própria de formato, relocação, memória executável e ABI; não é dependência da primeira versão.

### 6.2 Regra de minimização do binário

Todo componente residente deve justificar por que não pode ser um script, descrição ou recurso externo. Ativar somente drivers e bibliotecas utilizados, remover assets demonstrativos, usar eliminação de seções não referenciadas e avaliar LTO.

Não embutir o firmware do C6 como um grande array por conveniência. Quando for necessário distribuí-lo, armazená-lo dentro de `/SymbianOS/system/firmware/c6/`, com versão compatível e atualização explicitamente controlada. O firmware já gravado no C6 continua sendo infraestrutura do dispositivo.

Bibliotecas nativas de áudio, imagem ou protocolos podem ser necessárias para desempenho. Sua inclusão deve ser medida e registrada como capacidade do runtime; mover apenas o arquivo de um codec para o SD não torna esse código automaticamente carregável.

O tamanho máximo final do binário será definido com medições na fase 0, sem prometer um tamanho arbitrário. Cada build deve publicar tamanho do aplicativo executável, imagem de distribuição, uso de flash/RAM e maiores componentes. O arquivo merged pode conter lacunas de endereçamento; seu tamanho sozinho não mede o código residente.

### 6.3 APIs e concorrência

Drivers não serão acessíveis diretamente aos apps. A API versionada expõe entrada, gráficos, arquivos, notificações, hora/alarmes, áudio, rede, energia e câmera, conforme capacidades disponíveis.

Uma tarefa é proprietária do LVGL. Scripts e serviços enviam comandos por filas limitadas; operações demoradas são assíncronas e canceláveis. O LVGL exige serialização de seus acessos [S14]. Não fixar afinidade de cores antes de medir concorrência, áudio e acesso ao SD.

O runtime impõe limites de memória, tamanho de mensagem e duração das operações. Uma chamada C bloqueada não pode ser interrompida pelo contador de instruções do interpretador; por isso, funções nativas expostas devem retornar rapidamente ou oferecer operação assíncrona.

Todos os arquivos persistentes são mediados pelo VFS enraizado e pelo controle de quota, inclusive arquivos escritos pelos próprios serviços nativos.

## 7. Raiz `/SymbianOS` e quota de 4 GB

### 7.1 Mapeamento de caminhos

FAT32 será o formato inicial suportado. Habilitar e testar nomes longos e o tratamento de nomes Unicode na configuração escolhida do FatFs. exFAT somente será anunciado após validação específica; montagem malsucedida nunca autoriza formatação automática [S20].

Se o driver montar o cartão em `/sdcard`, o caminho físico de implementação será `/sdcard/SymbianOS`. Para o sistema e apps, esse prefixo corresponde a `/`. O ponto de montagem físico não deve aparecer como um volume adicional navegável.

| Caminho lógico | Conteúdo físico no cartão |
|---|---|
| `/boot/` | `/SymbianOS/boot/`: manifestos, seleção de release e estado de boot. |
| `/system/releases/<versao>/` | Shell, módulos Lua, apps essenciais, telas e recursos daquela release. |
| `/system/firmware/c6/` | Firmware externo do coprocessador, quando distribuído. |
| `/apps/<id>/versions/<versao>/` | Aplicativos adicionais e seus recursos. |
| `/config/` | Idioma, fuso, rede, perfis, atalhos e preferências do sistema. |
| `/data/<id>/` | Dados persistentes e estado de cada app/serviço. |
| `/home/Documents/` | Documentos e exportações do usuário. |
| `/home/Music/`, `/home/Pictures/`, `/home/Recordings/` | Mídia do usuário. |
| `/home/Downloads/` | Downloads concluídos. |
| `/themes/`, `/fonts/` | Personalizações externas instaladas. |
| `/cache/`, `/tmp/`, `/logs/` | Arquivos descartáveis, transações temporárias e diagnóstico. |
| `/backups/` | Backups guardados dentro da instalação. |
| `/update/` | Pacotes recebidos e arquivos de preparação de atualizações. |
| `/meta/` | Índices reconstruíveis e registro de contabilização da quota. |

A árvore acima define diretórios reais, não uma imagem de disco de 4 GB. Será possível copiar e inspecionar os arquivos no computador sem ferramentas de montagem de imagem.

O gerenciador de arquivos inicia nessa raiz. Se houver uma apresentação visual como `C:`, ela será apenas um alias da mesma instalação. Não criar um segundo disco interno para dados do sistema nem expor o restante do microSD aos apps por um caminho alternativo.

### 7.2 Confinamento

Normalizar caminhos, tratar separadores e caixa conforme o filesystem e rejeitar tentativas de sair da raiz por `..`, caminhos absolutos do host, aliases ou nomes inválidos. Arquivos recebidos por rede ou pacotes passam pela mesma validação. Rejeitar links e referências externas nos pacotes.

O runtime nativo é uma base confiável e precisa cumprir essa regra em seus próprios drivers de persistência. Um prefixo de texto adicionado ao nome não equivale a isolamento de processos; o confinamento depende de nenhuma API expor acesso bruto ao cartão para os scripts.

### 7.3 Definição da quota

**Limite fixo: 4.000.000.000 bytes para a soma dos tamanhos lógicos dos arquivos em toda a árvore `/SymbianOS`.** Incluir arquivos ocultos, versões antigas, temporários, logs, recursos e backups. Arquivos fora da pasta não entram nessa conta e não podem ser modificados pelo sistema.

Usar contadores de 64 bits e verificar também o espaço real livre do cartão. Tamanhos lógicos não incluem toda a sobrecarga dos clusters e diretórios; o cartão pode ficar sem espaço físico antes da quota lógica. Nessa situação, falhar de forma controlada mesmo que a quota ainda indique saldo.

Adotar uma reserva operacional inicial de **64 MiB dentro dos 4 GB**, para metadados, recuperação e operações essenciais. Escritas comuns serão bloqueadas antes de consumir essa margem; a reserva não autoriza ultrapassar o limite total. Ajustar seu tamanho somente com evidência e sem alterar o teto solicitado.

A quota não é o limite de tamanho de arquivo do FAT32 e não é uma exigência de particionar o cartão em 4 GB. O limite por arquivo do filesystem continua sendo uma restrição separada.

### 7.4 Todas as escritas devem respeitar a quota

1. Na inicialização, auditar a árvore e reconciliar o contador com os arquivos reais. Até concluir a verificação, manter escritas comuns suspensas.
2. Antes de criar, estender, baixar, gravar áudio ou extrair um pacote, reservar o crescimento máximo conhecido.
3. Para streams de tamanho desconhecido, reservar espaço por blocos antes de cada crescimento.
4. Serializar reservas concorrentes para impedir que dois escritores gastem o mesmo saldo.
5. Atualizar contador e reservas em sucesso, cancelamento, exclusão, truncamento e falha.
6. Considerar o pico de espaço: arquivo antigo, novo temporário e backup coexistem durante uma substituição.
7. Confirmar um download ou instalação somente quando a versão final estiver validada e sincronizada.
8. Nenhum serviço, inclusive logs e gravação de voz, poderá continuar crescendo ao atingir seu limite.

O contador persistido é uma otimização, não a fonte definitiva da verdade. O cartão pode ser alterado no computador. Se a árvore vier com mais de 4 GB, iniciar em modo de manutenção com leitura e exclusão/movimentação interna que reduza uso, sem aceitar novas escritas que aumentem a ocupação. Não apagar documentos automaticamente.

Caches e logs podem ser descartados conforme política declarada; arquivos de usuário e backups não podem ser tratados como cache. A interface deve mostrar uso total da instalação, saldo, reserva e ocupação por categoria.

### 7.5 Quedas de energia e remoção do cartão

Gravar substituições em temporários dentro da própria raiz, sincronizar e validar antes de trocar a versão ativa. Manter cópia anterior ou journal quando necessário. FAT não oferece garantia absoluta contra interrupção de energia, mesmo com renomeação e `fsync`; detectar transações incompletas no próximo boot.

Nunca formatar automaticamente um cartão quando a montagem falhar. Não criar uma instalação vazia por cima de arquivos existentes.

Se o cartão for removido em execução, cancelar serviços, interromper novas escritas, marcar handles inválidos e abrir a tela de recuperação residente. A reinserção exigirá revalidação da raiz antes de retomar. Não fazer gravações silenciosas em NVS ou em outro volume como fallback.

## 8. Boot, runtime e aplicativos externos

### 8.1 Sequência de inicialização

1. Iniciar o runtime e os drivers mínimos de alimentação, display, entrada e SD.
2. Procurar exatamente a pasta `/SymbianOS` e o manifesto de boot do formato suportado.
3. Validar estrutura, limites, integridade dos arquivos essenciais e compatibilidade da API.
4. Reconciliar quota e transações pendentes, inicialmente sem escritas comuns.
5. Selecionar uma release confirmada ou uma atualização pendente válida.
6. Carregar configuração e scripts de inicialização dessa release.
7. Iniciar shell, gerenciador de apps e serviços definidos no cartão.
8. Confirmar saúde dentro de um prazo limitado e liberar uso normal.

Não carregar 4 GB em RAM. Ler scripts, fontes, imagens e documentos por demanda, com caches limitados. Configurações inválidas devem gerar diagnóstico e opção de restauração; não substituir dados do usuário sem indicação.

Sem cartão, com raiz ausente ou release inválida, exibir apenas versão do runtime, motivo da falha e instrução para preparar/restaurar a pasta. Touch e teclado oficial devem funcionar nessa recuperação. Pode haver console serial de desenvolvimento; não incluir uma segunda interface S60 completa na flash.

### 8.2 Compatibilidade e isolamento

O manifesto deve identificar versão da release, esquema de dados, versão mínima/máxima da API nativa e pontos de entrada. Scripts serão carregados como texto; não aceitar bytecode externo inicialmente.

Cada app terá estado Lua separado, alocador com quota e limite de instruções por callback. Não expor bibliotecas de comandos, carregamento de bibliotecas nativas ou acesso direto ao filesystem do host. O toolkit recebe comandos validados, sem ponteiros brutos do LVGL expostos aos apps.

Os limites de runtime reduzem falhas de scripts, mas não equivalem ao isolamento de processos do Symbian. Erros nas funções C ou no interpretador ainda podem derrubar o firmware.

### 8.3 Instalação

Pacote provisório `.t5pkg`: arquivo ZIP com manifesto, scripts e recursos. Informar ID, versão, autor, API exigida, permissões, orientações, limites e hashes.

Extrair para `/tmp/` ou `/update/`, validando caminhos, quantidade de arquivos e tamanho descompactado antes de ativar. O limite de 4 GB vale durante toda a operação, inclusive enquanto o pacote compactado e a extração coexistirem.

Aplicativos fornecidos com o sistema também residem na release do cartão. O instalador completo virá depois; o primeiro protótipo poderá distribuir uma pasta pronta para copiar, sem compilar esses apps no runtime.

Hash verifica integridade. Se houver catálogo remoto, definir assinatura e política de confiança. Manter um modo de desenvolvimento local explícito.

## 9. Serviços, multitarefa e persistência

O residente fornece primitivas nativas. Orquestração, políticas e interface dos serviços que puderem ser expressas em Lua ficam na release do cartão. Não tentar executar processamento de áudio ou drivers em callbacks longos de script.

| Serviço | Execução e persistência |
|---|---|
| Configuração | API de leitura/validação; arquivos em `/config/`. |
| Tempo e alarmes | Temporizadores/RTC nativos; agenda persistida em `/data/` e política na release. |
| Áudio | Buffers/codec nativos; fila, playlists e preferências no cartão. |
| Arquivos | VFS e quota nativos; operações e interface usando caminhos lógicos. |
| Rede | Transporte nativo; credenciais e preferências em `/config/`, requisições com limites. |
| Notificações | Entrega por eventos; regras e histórico no cartão. |
| Energia | Controle físico nativo; políticas e configuração carregadas do cartão. |
| Pacotes | Validação por APIs controladas; registro e arquivos dentro da raiz. |
| Diagnóstico | Buffer de RAM antes da montagem; logs limitados em `/logs/` após validar a raiz. |

Cada app declara identidade, versão, vistas, orientações e callbacks de criação, foco, salvamento, suspensão e encerramento. Aplicativos fora de foco não redesenham. Operações de fundo usam serviços com limites explícitos.

Sob pressão de memória, descartar caches, salvar estado em `/data/<id>/` e descarregar vistas inativas. O player pode perder a vista sem parar o serviço de áudio. Resultados atrasados devem trazer identificador de operação e ser descartados se a vista já foi fechada.

Notas, contatos e compromissos terão registros versionados e índices reconstruíveis. Evitar introduzir um banco complexo antes de existir demanda; avaliar bibliotecas existentes conforme volume e buscas necessários.

Hora civil deve usar fuso configurável, inicialmente `America/Cuiaba`, e temporizações internas devem usar relógio monotônico. Sincronização e ajustes manuais não podem duplicar alarmes.

NVS da flash não será usada para dados do sistema, preferências ou credenciais. Metadados técnicos inevitáveis de boot, OTA e calibração do hardware são a exceção de infraestrutura; não formam um disco de usuário. Configurar as bibliotecas de rede para não persistirem preferências automaticamente fora da raiz e auditar efeitos no P4 e no C6.

## 10. Memória, energia, transferência e atualizações

### 10.1 Memória e desempenho

A quota de 4 GB não muda os 32 MB de PSRAM do Tab5. Um framebuffer RGB565 de 720 × 1280 ocupa aproximadamente 1,76 MiB; dois ocupam 3,52 MiB.

| Uso em PSRAM | Envelope preliminar |
|---|---:|
| Dois framebuffers, se necessários | 3,52 MiB |
| Objetos UI e fontes em cache | 3 MiB |
| Imagens e miniaturas em cache | 3 MiB |
| Estados de scripts e apps | 4 MiB |
| Índices e operações de arquivos | 2 MiB |
| Buffers de áudio elegíveis | 1 MiB |
| Rede/TLS | 3 MiB |
| Margem desejada para picos | 8 MiB |
| Total de referência | 27,52 MiB |

A disponibilidade real e as exigências dos buffers precisam ser medidas. SRAM interna terá orçamento separado para pilhas e buffers que não puderem usar PSRAM. Câmera e decodificação consumirão parte da margem, com limites de concorrência.

Não usar o SD como swap geral. Virtualizar listas, indexar em lotes e limitar miniaturas. Medir latência do cartão, maior bloco livre e fragmentação. Assets na raiz não serão todos pré-carregados no boot.

Meta inicial: feedback visual local em até 100 ms no percentil 95 dos cenários definidos e 30 fps em rolagens/transições comuns. Essas metas ainda não foram demonstradas na placa.

### 10.2 Energia

Distinguir brilho reduzido, tela apagada com serviços ativos e suspensão profunda. Música pode continuar com tela apagada; deep sleep é outra situação.

Persistir alarmes no cartão. Despertar via RTC exige ensaio do caminho real de alimentação e retomada. Não prometer alarmes com o cartão removido, arquivos inacessíveis ou dispositivo sem alimentação. A retomada deve revalidar montagem e versões antes de reabrir os arquivos.

### 10.3 Transferência e backup

Priorizar transferência web local, acionada pelo usuário e com credencial temporária. Todos os uploads e downloads usam a raiz lógica e a mesma quota; não criar temporários fora de `/SymbianOS`.

Backup da instalação pode ser feito pelo computador copiando a pasta com o sistema parado. Preservar estrutura, arquivos ocultos e manifestos. Portabilidade pressupõe runtime/hardware compatíveis.

Backups criados pelo próprio sistema dentro de `/backups/` também contam nos 4 GB. Evitar que um backup inclua a si mesmo, outros backups ou temporários. Se não houver espaço, oferecer exportação por streaming ao computador, sem ignorar a quota.

Não exportar por USB MSC um filesystem simultaneamente montado para escrita pelo sistema. Acesso bruto do computador ao cartão exige pausar/desmontar o ambiente. Como MSC expõe blocos, não afirmar que ele fica confinado a uma única pasta; usar o serviço de arquivos para transferência restrita à raiz.

### 10.4 Duas classes de atualização

**Sistema no cartão:** preparar nova release em `/system/releases/`, preservar a última confirmada e registrar inicialização pendente. Só confirmar após validar shell e serviços essenciais. Se falhar, voltar à release anterior. Ambas, os pacotes e os temporários contam na quota. Falta de espaço interrompe a atualização antes da ativação.

**Runtime na flash:** atualizar apenas quando drivers, APIs nativas, renderizador, runtime ou bibliotecas residentes precisarem mudar. Usar dois slots OTA e rollback; definir partições somente após medir o binário. Reservar espaço para metadados e recuperação conforme o custo medido de cada componente.

Manter compatibilidade entre runtime, release, esquema dos dados e firmware do C6. Rollback precisa preservar documentos e formatos recuperáveis. Autoteste não deve falhar por falta de rede opcional. Testar também atualização recebida por arquivos no cartão, sem conexão externa.

Modo seguro residente deve permitir ignorar a atualização pendente e selecionar uma release anterior compatível. Se o cartão estiver ilegível, limitar-se à recuperação; não reformatar, apagar a pasta ou fabricar uma instalação substituta.

## 11. Plano de execução e critérios de passagem

### Fase 0 — Runtime, raiz, quota e teclado

**Entregas:** projeto reproduzível, identificação da placa, drivers mínimos, Lua/LVGL, montagem do cartão e VFS confinado. Teclado oficial I²C obrigatório; USB separado. Manifesto de boot e relatório inicial do tamanho residente.

**Passagem:** carregar do cartão um script que desenha uma tela, recebe texto do teclado oficial e salva somente dentro da raiz. Bloquear fuga de caminho e escrita acima da quota. Reiniciar e recuperar o arquivo. Sem cartão, abrir diagnóstico. Validar ainda áudio, rede e RTC em ensaios de driver antes de assumir compatibilidade futura.

### Fase 1 — Primeiro ambiente carregado do SD

**Entregas:** home, menu, moldura S60, teclado virtual, Notas e Calculadora como arquivos externos. Simulador desktop usando a mesma árvore de instalação e políticas de armazenamento.

**Passagem:** modificar a aparência e o comportamento de uma nota substituindo arquivos do SD, sem gerar outro binário. Digitar acentos, navegar por setas/Tab/Enter/Esc e salvar. Confirmar que o runtime não contém uma segunda cópia desses aplicativos.

### Fase 2 — Multitarefa e persistência

**Entregas:** gerenciador de apps, alternador, eventos, notificações, perfis e limites de execução/memória. Estado em `/data/` e preferências em `/config/`.

**Passagem:** alternar três apps preservando estado; erro de script e limite de instruções não congelam indefinidamente a interface; nenhuma preferência é gravada na flash. Testar concorrência de reservas de espaço e cancelamentos.

### Fase 3 — Organizador e gerenciador de arquivos

**Entregas:** contatos, agenda, relógio/alarmes, conversor, arquivos e backup. Indicador de uso da instalação de 4 GB.

**Passagem:** CRUD, busca, importação/exportação e restauração; alarmes disparam fora da agenda; arquivos nunca escapam da raiz. Testar ausência de espaço físico e lógico, inclusive alteração da pasta pelo computador.

### Fase 4 — Multimídia

**Entregas:** player WAV/MP3, playlists, gravador, galeria e fotografia, com interfaces/lógica no cartão e backend nativo estritamente necessário.

**Passagem:** música continua durante edição de nota; alarme interrompe/atenua e permite retomada; gravação para de forma controlada ao acabar o espaço; fotos e miniaturas respeitam orçamento. Cada biblioteca residente adicionada informa seu custo no binário.

Vídeo permanece uma investigação posterior com benchmark próprio.

### Fase 5 — Instalador e SDK

**Entregas:** pacote próprio, instalação/remoção/atualização, permissões, documentação e app de exemplo externo. Temas e apps atualizáveis sem regravar o runtime.

**Passagem:** validar tamanho descompactado e pico de ocupação, recusar pacotes incompatíveis e caminhos inválidos, atualizar de forma recuperável e remover sem apagar documentos sem escolha explícita.

### Fase 6 — Rede e extensões escolhidas

**Entregas:** Wi-Fi, transferência local confinada, downloader e feeds. Escolher depois um transporte real para mensagens. BLE, e-mail e mapas serão tarefas próprias.

**Passagem:** quedas de rede e cancelamentos não travam; arquivos parciais ficam somente na raiz; uploads paralelos não excedem a quota. Estado offline e falhas são reais e visíveis.

### Fase 7 — Energia, versões e distribuição

Tratamento mínimo de falhas existe desde a fase 0; esta fase conclui sua validação.

**Entregas:** suspensão/retomada, OTA do runtime, atualização de releases no cartão, rollback, ferramentas de preparação e recuperação, relatório de desempenho/consumo.

**Passagem:** recuperar atualização interrompida e release que falha no boot; tratar remoção/reinserção; iniciar em manutenção quando a pasta exceder o limite; ensaio prolongado sem crescimento contínuo de memória. Demonstrar o teclado oficial em todas as jornadas relevantes.

## 12. Testes obrigatórios e definição de pronto

### 12.1 Armazenamento e quota

- Montar uma instalação válida sem tocar em arquivos fora de `/SymbianOS`.
- Recusar nomes e arquivos de pacote que tentem escapar da raiz.
- Preencher até próximo do limite e testar criação, extensão, cópia, sobrescrita, instalação, gravação e downloads concorrentes.
- Testar reserva operacional, pico de extração e liberação de reservas após falhas.
- Alterar a pasta no computador e reconciliar a contagem no boot seguinte.
- Com mais de 4 GB, permitir recuperação sem aceitar crescimento nem apagar documentos automaticamente.
- Testar cartão sem espaço físico, somente leitura, ausente, removido e com transação incompleta.
- Confirmar que cache, logs, backups e versões anteriores entram na conta.
- Confirmar que não há persistência alternativa em flash ou em outra pasta do cartão.

### 12.2 Interface e teclado

- Teclado oficial I²C: pressionar/soltar, repetição, modificadores, acentos, seleção e atalhos.
- Reconectar teclado sem reiniciar e sem gerar teclas presas.
- Operar home, menu, Notas, arquivos, alternador e recuperação com o teclado.
- Verificar touch e teclado virtual com e sem acessório físico.
- USB HID não pode ser utilizado como evidência substituta do teclado oficial.

### 12.3 Sistema externo

- Iniciar a mesma instalação em runtime compatível e verificar os documentos.
- Mudar shell/app no SD sem recompilar o binário e demonstrar o novo comportamento.
- Remover arquivos essenciais e obter diagnóstico, sem iniciar cópia embutida.
- Recusar release incompatível e recuperar a última confirmada.
- Atualizar sistema e runtime por caminhos separados, mantendo dados compatíveis.
- Medir binário, memória, latência de SD, entrada, renderização, áudio e consumo.

Testes automatizados cobrem caminhos, quota, manifesto, instalação, migrações e ciclo de vida. Simulador valida UI e políticas; drivers, I²C real, falhas de energia e latências exigem a placa. Ensaios destrutivos usam cópias de dados e cartão de teste.

Uma fase só termina com funcionalidade utilizável, limitações registradas e evidência de validação. Compilar e executar no desktop não comprova funcionamento físico.

## 13. Entregáveis e organização do desenvolvimento

| Entregável | Conteúdo |
|---|---|
| `SymbianOS-runtime.bin` | Aplicativo residente para o método de instalação validado, com offset/formato documentados. |
| Imagem merged de instalação, quando necessária | Bootloader, partições e runtime; identificação inequívoca da placa/revisão. |
| `SymbianOS-sdcard.zip` | Pasta `SymbianOS` completa, pronta para extrair na raiz de um microSD compatível. |
| Ferramenta de verificação | Validação da estrutura, hashes, versões e tamanho total antes de copiar. |
| SDK e exemplos | Aplicativos externos, documentação da API e empacotamento. |
| Relatório de build | Tamanho residente, dependências, custos por componente e memória. |
| Guia de recuperação | Preparar cartão, restaurar pasta, selecionar release anterior e recuperar runtime. |

Não embutir automaticamente o ZIP do sistema como array no runtime. A distribuição deve manter binário e instalação no SD como artefatos independentes e compatíveis por versão.

Separar código do residente, adaptador de hardware, bridge Lua, sistema em scripts, apps, temas, ferramentas e simulador. Tarefas pequenas devem descrever jornadas completas, com critérios observáveis.

Antes de publicar, verificar que não existem ícones, fontes completas, temas e aplicativos incorporados sem justificativa. A fonte mínima de diagnóstico é exceção deliberada. Fixar commits/versões e inventariar licenças.

Não definir prazo total antes da prova de runtime/teclado/SD. O primeiro benchmark mostrará o custo real da API gráfica, do interpretador e da concorrência no cartão.

## 14. Primeiro marco utilizável

Primeiro marco: ligar o Tab5, localizar `/SymbianOS`, validar quota/manifesto, carregar home e menu do cartão, abrir Notas, digitar pelo teclado oficial, salvar, alternar para Calculadora e voltar sem perder o texto. Reiniciar e encontrar os dados preservados.

Alterar a nota ou o tema no computador deve mudar o ambiente sem novo `.bin`. Retirar o cartão deve levar à recuperação residente, e não a um sistema alternativo interno.

Depois dessa demonstração, ampliar serviços e aplicativos. Esse marco comprova os três requisitos centrais antes de aumentar o escopo: **sistema externo em pasta, runtime enxuto e teclado oficial funcional**.

## 15. Fontes consultadas

As fontes históricas abaixo são documentação original Nokia/Symbian preservada por terceiros. As fontes de hardware e ferramentas são dos respectivos fabricantes/projetos. Alguns links legados da Nokia/Microsoft foram localizados, mas falharam na abertura; o manual efetivamente consultado foi o espelho S6.

- **S1 — Nokia, S60 5th Edition C++ Developer’s Library v2.1:** https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/
- **S2 — Nokia, Touch support for common S60 components:** https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-BB8B3A11-0BBC-4759-A7F4-A28C9E70573F.html
- **S3 — Nokia, New features in S60 5th Edition:** https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-CA617A81-53B3-4B19-A69E-32DF920A26C5.html
- **S4 — Symbian Developer Library, The filesystem browser application, seção Introducing the application framework:** https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-35228542-8C95-4849-A73F-2B4F082F0C44/sdk/doc_source/NewStarter/4-uicode.html
- **S5 — Nokia, Status pane:** https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-771AF721-E2D7-41C8-BF97-5BD30D1A28AD.html
- **S6 — Nokia 5230 User Guide, Issue 1, 2009, espelho:** https://i.smartphone.ua/docs/instr/instr_nokia-5230_eng.pdf — funções gerais no sumário; atalhos na página impressa 33; player em segundo plano na 69; utilitários e gerenciador nas páginas 123–131.
- **S7 — M5Stack, Tab5:** https://docs.m5stack.com/en/core/Tab5
- **S8 — M5Stack, M5Tab5-UserDemo:** https://github.com/m5stack/M5Tab5-UserDemo
- **S9 — Espressif, FreeRTOS IDF:** https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/system/freertos_idf.html
- **S10 — Espressif, esp_lvgl_port:** https://github.com/espressif/esp-bsp/blob/master/components/esp_lvgl_port/README.md
- **S11 — Apache NuttX, M5Stack Tab5:** https://nuttx.apache.org/docs/latest/platforms/risc-v/esp32p4/boards/esp32p4-tab5/index.html
- **S12 — Espressif, ESP-Hosted:** https://github.com/espressif/esp-hosted-mcu
- **S13 — Espressif, ESP32-C6:** https://www.espressif.com/en/products/socs/esp32-c6
- **S14 — LVGL, Integration overview, seção Operating Systems and Threads:** https://github.com/lvgl/lvgl/blob/master/docs/src/integration/overview.mdx
- **S15 — Espressif, OTA, estados e rollback:** https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32p4/api-reference/system/ota.html
- **S16 — Nokia, Skins API:** https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-759FBC7F-5384-4487-8457-A8D4B76F6AA6/html/Skins_API3.html
- **S17 — Lua 5.4 Reference Manual:** https://www.lua.org/manual/5.4/manual.html
- **S18 — Espressif, Partition Tables:** https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/partition-tables.html — a página exibe aviso de atualização incompleta para P4; usar como referência conceitual e validar a tabela no SDK/revisão escolhidos.

- **S19 — amcchord, M5Tab-Macintosh, README do projeto:** https://github.com/amcchord/M5Tab-Macintosh — divisão entre firmware, ROM, imagem de disco e configurações no cartão.
- **S20 — Espressif, FAT Filesystem Support:** https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/api-reference/storage/fatfs.html
- **S21 — Espressif, Virtual Filesystem Component:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/storage/vfs.html
- **S22 — M5Stack, Tab5 Keyboard:** https://docs.m5stack.com/en/tab5/Tab5_Keyboard
- **S23 — M5Stack, Tab5 Keyboard UserDemo:** https://github.com/m5stack/M5Tab5-Keyboard-UserDemo

FatFs/VFS [S20–S21] fornecem integração com o cartão; confinamento por pasta, quota e transações descritos neste plano são funcionalidades a desenvolver sobre essa base. Não presumir uma quota de diretório pronta no FatFs.

As versões `stable`, `latest` e `master` podem mudar. Na Fase 0, arquivar os identificadores exatos das versões e commits utilizados, além dos URLs.
