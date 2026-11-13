> Documento histórico. Estado atual: [README](../README.md) e [limitações 1.0.0](LIMITACOES_1_0_0.md).

# Tab5 SymbianOS

Correção atual do envio web: pilha HTTP em PSRAM e liberação dos sockets quando a tarefa não inicia. [Diagnóstico e teste no Tab5](docs/HTTPD_TASK_ENVIO_WEB.md). Depois de atualizar, reinicie para limpar as tentativas do firmware anterior.

Ajustes atuais de instalador e envio: [lista SIS, rolagem e envio web](docs/INSTALADOR_ROLAGEM_ENVIO_WEB.md). Controles à direita, posição preservada e envio por token na porta 8080. A validação física SIS informada pelo usuário foi registrada; estes ajustes novos ainda aguardam teste no Tab5.

Distribuições ficam exclusivamente em `sist/`, com retenção das duas últimas versões do runtime. Atual: `sist/0.19.0/`; anterior: `sist/0.18.1/`. Referências a `dist` nos relatórios antigos são históricas.

## SIS sem assinatura — fases 5 e 6

Implementados atualização, dependências/ciclos, novas permissões revisadas, esquema de dados com migração declarativa, downgrade confirmado, primeira abertura com recuperação, rollback com snapshot e remoção transacional. SDK e kit de testes originais são gerados com `python3 -B tools/release_sis.py`. Contrato: [Ciclo de vida SIS](docs/SIS_CICLO_DE_VIDA.md). Validação física pendente: [Roteiro físico SIS](docs/ROTEIRO_FISICO_SIS.md). Esta seção substitui os estados históricos abaixo que indicam essas etapas como futuras. SISX e assinaturas continuam adiados para a fase final. Tudo persiste no SD; firmware continua pelo launcher.

## SIS sem assinatura — fases 1 a 4

Implementados contrato, ferramentas de host, inspeção e instalação transacional para aplicações Lua no [Perfil SIS 1](docs/PERFIL_SIS_1.md). A validação física no Tab5 está pendente. Este estado substitui a indicação anterior de SIS apenas planejado. Assinaturas/SISX ficam para a última fase; atualizações de firmware continuam exclusivamente pelo launcher.

## Decisão vigente: navegador e atualizações pelo launcher

Na revisão 20261113-R3, o navegador abre em leitura nativa, oferece HTML/CSS e imagens com limites explícitos, JavaScript inline ES5 básico, formulários GET/POST e cookies/sessões no SD. Os controles usam ícones em coluna estreita e os links aceitam toque; a compatibilidade com sites modernos permanece parcial. A tela PIN aceita OK por toque ou Enter no teclado físico oficial. Estado dos seis itens e roteiro físico: [Navegador, e-mail e feeds R3](docs/NAVEGADOR_EMAIL_FEEDS_20261113_R3.md). A causa exata da queda anterior e os testes físicos desta revisão permanecem pendentes.

Atualizações do runtime passam exclusivamente pelo launcher. Ficam abandonados o aplicativo e o fluxo de OTA interno, incluindo kits de atualização gerados para esse fluxo. A interface web autenticada por token continua para transferência de arquivos. Referências anteriores a OTA neste documento registram decisões históricas e não autorizam reintrodução desse mecanismo.

Pacotes SIS/SISX ficam somente planejados em [Plano de pacotes](docs/PLANO_PACOTES_SIS_SISX.md); o instalador atual não passa a reconhecer essas extensões. Wi-Fi foi confirmado funcionando pelo usuário; o teste físico de e-mail foi adiado para a próxima implementação.

O nome **SymbianOS é apenas uma referência e homenagem ao velho, excelente e lendário sistema operacional da Nokia**. Bons tempos — ainda tenho um Nokia N95 funcional em casa! Este é um projeto independente, escrito do zero para o M5Stack Tab5, com interface inspirada no S60 5th Edition.

Plataforma original para M5Stack Tab5. O [planejamento](plano_sistema_s60_tab5.md) é o documento normativo. Drivers, Lua e LVGL compõem o residente; interface, aplicativos e recursos vêm do SD, sob `/SymbianOS` e quota decimal de 4 GB.

## Versão local 0.19.0 / API 14

Revisão local das [oito falhas de nível alto](docs/CORRECOES_ALTA_0_19.md): isolamento de eventos, integridade de diretórios e uploads, preservação de rascunhos durante anexação, alocação HTML por tarefa e distribuição sem dados privados. Pareamentos BLE ficam em RAM, sem persistência em NVS; informe novamente o PIN após reiniciar. Para gerar somente o BIN e a pasta do cartão destinados à WebGUI do launcher: `python3 tools/release.py --launcher-only`.

Corrigidas também a inicialização do alocador antes do scheduler e a dependência de uma partição NVS ao conectar Wi-Fi. Toda persistência da aplicação fica em `/SymbianOS` no cartão; o driver Wi-Fi usa RAM no P4/C6. Não é necessário criar ou formatar NVS na flash.

O fluxo de eventos Wi-Fi agora filtra START/STOP repetidos antes dos handlers ESP-IDF, evitando o abort `netif already added` observado durante a inicialização com conexão automática. O diagnóstico informa quantos eventos repetidos foram descartados. Aceitação física desse novo BIN ainda pendente.

Leitor HTML/CSS nativo com litehtml, paginação e recursos no SD; pastas de e-mail em coluna de ícones, nova tentativa manual de SMTP rejeitado e correção do orçamento Lua nas medições. Slots OTA de **4 MiB**, preservando o mapa do launcher. [Detalhes e limites](docs/HTML_CSS_0_19.md). Atualize BIN e SD juntos.

[Entrega das fases 3, 4, 6, 7, 8 e 10](docs/FASES_3_4_6_7_8_10_0_17.md): favoritos, três tons de alarme, interfaces em português/inglês/espanhol, anexos de e-mail e exclusões locais persistentes. Novo app Testes e medições, coleta em segundo plano, verificação do SD e revisão OTA sem instalação. Testes de host/falhas e instrumentação não substituem consumo, áudio e OTA na unidade física. Atualize BIN e SD juntos.

[HTML e imagens 0.16.0](docs/HTML_EMAIL_RSS_0_16.md): leitor visual compartilhado para e-mail e RSS, imagens incorporadas/offline e carregamento explícito de imagens externas, texto alternativo e cache limitado. Recebimento de e-mail confirmado pelo usuário após a correção SDIO. Atualize BIN e SD juntos. [Pendências revisadas até a fase 10](docs/PENDENCIAS_ATE_FASE_10.md).

[Recepção SDIO 0.15.12](docs/SDIO_RX_0_15_12.md): corrige incompatibilidade entre o host em modo pacote e o C6 oficial 1.4.1 em streaming. Rejeição de lotes agregados reproduzida com funções reais do driver; streaming passa dados e controle. Build e empacotamento recusam configuração antiga. Atualize BIN e SD juntos. Validação física pendente.

[Transporte C6 0.15.11](docs/TRANSPORTE_C6_0_15_11.md): reconhece falha persistente de controle remoto, evita repetir tentativas sem progresso, oferece reinício confirmado e exporta avisos SDIO. Corrige aviso de recebimento após término. É uma saída para o estado travado; a causa da perda de tráfego continua em investigação física. Atualize BIN e SD juntos.

[Driver Wi-Fi 0.15.10](docs/DRIVER_WIFI_0_15_10.md): recuperação recria o driver após limpeza aceita, suspende buscas após falha e preserva a intenção de restaurar a OTA. Diagnóstico limpa os caracteres anteriores. Atualize BIN e SD juntos; validação física pendente.

[Recuperação de rede 0.15.9](docs/RECUPERACAO_REDE_0_15_9.md): usuário confirmou DNS (202) e página OTA inacessível no mesmo estado. Recuperação assíncrona da última conexão, limitada/protegida contra transferências, reinício do serviço OTA e botão Reconectar Wi-Fi. Diagnóstico inclui IP/gateway/DNS e resultado da recuperação. Backends de rede testados no host; recuperação física ainda pendente. Atualize BIN e SD juntos.

[Revisão de e-mail 0.15.8](docs/AUDITORIA_EMAIL_0_15_8.md): perfil TLS explícito e testado no ClientHello real, MFL 1 KiB, reconexão pré-autenticação, SMTP 334/251/252, caixas/mensagens IMAP grandes, ordem e isolamento por conta, cache compacto e MIME. O contorno exclusivo b/g/n não resolveu a foto e foi retirado; protocolos Wi-Fi normais restaurados. Falha física principal ainda requer reteste. Atualize BIN e SD juntos.

[Compatibilidade Wi-Fi 0.15.7](docs/WIFI_COMPAT_0_15_7.md): solicita b/g/n ao C6 antes de conectar, confere o bitmap por RPC e registra modo anterior/aplicado e firmware. Mitigação para investigar recepção parcial durante TLS; ainda não validada fisicamente. A foto 0.15.6 voltou a mostrar handshake incompleto. Atualize BIN e SD juntos e reconecte o Wi-Fi.

[Protocolo IMAP 0.15.6](docs/PROTOCOLO_IMAP_0_15_6.md): limite por inatividade em vez de 90 segundos fixos por sessão; timeout preserva comando/tag e diagnóstico TLS. Teste integrado executa LOGIN/LIST/SELECT/SEARCH/FETCH sobre TLS real e simula LOGIN sem resposta. A tentativa física 0.15.5 avançou até o protocolo; autenticação/recebimento da conta ainda pendentes. Atualize BIN e SD juntos.

[TLS real 0.15.5](docs/TLS_REAL_0_15_5.md): transporte de e-mail com mbedTLS direto e send/recv, compartilhado com testes de negociação real. TLS 1.2 e saudação IMAP Hostinger passaram no computador, sem credenciais. Testes cobrem certificado/nome inválidos, STARTTLS, tráfego fragmentado, cancelamento e prazo. Operação via C6 ainda precisa de aceitação; atualize BIN e SD juntos.

[Transição TLS 0.15.4](docs/TRANSICAO_TLS_0_15_4.md): contorna a espera com conjuntos de select esvaziados no ESP-TLS assíncrono instalado. Após confirmar o socket TCP, avança para TLS mantendo o descritor não bloqueante e as verificações do certificado. Regressão usa o transporte real compilado contra API ESP-TLS simulada. Aceitação no Tab5 pendente; atualize BIN e SD juntos.

[Conexão cancelável 0.15.3](docs/CONEXAO_CANCELAVEL_0_15_3.md): TLS assíncrono com pausas, Cancelar sem join na interface, progresso em tempo real, relógios UI/POSIX atuais no diagnóstico e aplicação/verificação da hora antes de conectar. Interface e conexão IMAP ainda exigem aceitação física. Atualize BIN e SD juntos.

[Relógio e IMAP 0.15.2](docs/RELOGIO_IMAP_0_15_2.md): hora da interface aplicada ao ESP-IDF, validade temporal de certificados e diagnóstico que distingue TCP de negociação TLS, com IP e data legível. Timeout relatado também no hotspot; conexão no Tab5 ainda em investigação. Atualize BIN e SD juntos.

[Conexão IMAP 0.15.1](docs/IMAP_0_15_1.md): IPv4 e tempo ampliado na negociação, diagnóstico completo TLS/certificado/memória em tela e no SD. Hostinger respondeu ao teste TLS no computador; aceitação no Tab5 ainda pendente. Atualize BIN e SD juntos.

[E-mail 0.15.0](docs/EMAIL_0_15.md): IMAP/POP3 e SMTP com portas configuráveis, senhas independentes, TLS direto/STARTTLS, leitura offline, rascunhos, respostas e fila de saída. Interface no padrão dos outros aplicativos. Sem OAuth por enquanto. Atualize BIN e SD juntos. Senhas ficam no SD sem criptografia; conexão com servidores reais ainda precisa ser validada no Tab5.

O usuário confirmou o diagnóstico e a estabilidade da 0.14.5. Meshtastic fica para outra ocasião.

[Diagnóstico 0.14.5](docs/DIAGNOSTICO_0_14_5.md): navegação durante trabalho foreground deixa de ser descartada; relatório usa painel rolável sem textarea, mantendo o label nas atualizações. Acrescenta firmware e suporte PKI anunciado pelo RAK. Estabilidade física e incompatibilidade privada bidirecional continuam pendentes.

[Diagnóstico 0.14.4](docs/DIAGNOSTICO_0_14_4.md): relatório atualizado no mesmo campo, sem reconstruir a tela a cada contador; código e origem em linhas separadas, região 6 identificada como ANZ e botão para exportar diagnóstico ao SD. Flashes físicos e envio privado ainda pendentes de confirmação/diagnóstico.

[Estabilidade 0.14.3](docs/ESTABILIDADE_0_14_3.md): corrigido estouro de orçamento Lua reproduzido em bursts com banco cheio. Cache e gravações divididas em ciclos; diagnóstico começa por código/origem e destino. Falhas Lua deixam registro classificado no SD. Estabilidade física e erro 6 continuam pendentes de confirmação/investigação.

[Chaves e RAK 0.14.2](docs/CHAVES_RAK_0_14_2.md): criptografia delegada ao rádio, sem forçar PKI/chave pública/canal zero. Diagnóstico de disponibilidade das chaves, modo licenciado, região e último erro sem expor valores. A 0.14.1 não resolveu o envio privado segundo o usuário; a entrega na 0.14.2 ainda precisa ser validada fisicamente.

[Privadas 0.14.1](docs/PRIVADAS_0_14_1.md): solicitação explícita PKI, chave pública recebida do nó, falhas legíveis e ACK local separado de confirmação remota. O usuário validou o restante da interface 0.14.0; entrega privada da nova versão ainda requer teste físico.

[Meshtastic 0.14.0](docs/MESHTASTIC_0_14.md): composição na conversa, seleção de canal/destinatário, mensagens privadas, nós com detalhes laterais, paginação inferior e nome do nó local. Criação/edição/desativação de canais com confirmação por releitura do rádio. Atualize BIN e SD juntos.

[Inicialização e RAK 0.13.1](docs/INICIALIZACAO_0_13_1.md): tela cinza com progresso antes de carregar o SD, UART2 alinhada ao cliente de referência, loopback interno desligado e diagnóstico de eco/IDs. Envio e recebimento RAK confirmados pelo usuário na 0.13.3.

[Nova interface 0.13.0](docs/INTERFACE_0_13.md): ícones fornecidos em icons, aparência escura inspirada no S60, Home com widgets, Menu com controles centralizados, histórico em balões e paisagem preservada. Escritas pequenas atualizam a quota sem varrer todo o SD; RSS recebe em RAM antes de salvar. O comando RAK recebe confirmação de execução e o indicador não pisca no tick automático. Atualize BIN/API 13 e SD juntos; aceitação de tempo/rádio/RSS no Tab5 ainda pendente.

[Correções 0.12.1](docs/CORRECOES_0_12_1.md): recepção contínua do RAK/Grove e conexão em um toque, melhor uso de PSRAM, detalhes completos do erro RSS, indicador de sinal Wi-Fi, Menu com quatro colunas na grade/duas na lista e PIN com entrada rápida por teclado físico e touch.

[Home e Meshtastic](docs/MESHTASTIC_0_12.md): Home com relógio, compromissos, notificações e três atalhos configuráveis; processamento junto à bateria e linhas espaçadas em Abertos. Novos apps Meshtastic BLE/C6 e RAK3172H/Grove, com fila offline, histórico, nós e canais. RSS com espera maior e leitura por blocos menores. Mantém os refinamentos da [0.11](docs/INTERFACE_0_11.md).

Atualize BIN e SD juntos. Testes de host, sanitizadores e build são separados da validação física: BLE/RAK, coexistência, RSS no Tab5 e OTA própria ainda precisam de aceitação. Artefatos locais em dist/; links públicos continuam na 0.8.0. E-mail offline e SIS/SISX/JAR próprios permanecem planejados.

## Última versão pública: 0.8.0 / API 8

[Baixe a release 0.8.0](https://github.com/tpklge/Tab5_SymbianOS/releases/tag/v0.8.0).

| Arquivo | Uso |
|---|---|
| [SymbianOS-runtime.bin](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SymbianOS-runtime.bin) | Aplicação ESP32-P4 para OTA pelo launcher. |
| [SymbianOS-sdcard.zip](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SymbianOS-sdcard.zip) | Conteúdo completo do cartão para instalação inicial. |
| [SymbianOS-update-0.8.0.zip](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SymbianOS-update-0.8.0.zip) | Atualização do SD preservando documentos e configurações. |
| [SHA256SUMS](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SHA256SUMS) | Verificação dos arquivos publicados. |

**Atualize BIN e SD juntos** para 0.8.0. Faça backup da instalação existente e mantenha `/data`, `/home` e `/config`. [Procedimento pelo launcher](docs/ATUALIZACAO_LAUNCHER.md).

Rede agora tem **redes salvas, conexão automática, conectar sem salvar e esquecer rede**. Conecte e salve uma vez para que a rede seja reconhecida nas próximas inicializações. A transferência web usa token temporário de **8 caracteres**. [Uso e armazenamento das credenciais](docs/REDES_SALVAS_0_8.md).

Mantém os aplicativos Lua no SD, organizador, multimídia, câmera com exposição automática e alvo de prévia até 20 fps, vídeo MJPEG até 5 fps sem áudio, bateria/carga QC, volume, busca Wi-Fi, downloads e feeds. Inclui a correção de upload da 0.7.3; o autor confirmou arquivo enviado com sucesso. Também relatou câmera funcionando, reprodução de um set de DJ de 133 MB e carga QC chegando a 12 W nas revisões anteriores.

Oito suites de host, jornada gráfica e ASan/UBSan validam a lógica; build ESP32-P4 verifica compilação/tamanho. Reconexão automática e token curto desta versão ainda precisam de aceitação na unidade física. Relatos do autor não equivalem a medições exaustivas de fps, consumo ou recuperação de falhas.

## Compilar a versão local

Use ESP-IDF **5.4.4** para ESP32-P4; as versões dos componentes estão fixadas em `dependencies.lock`. Lua e o código original estão neste repositório; os componentes gerenciados são baixados pelo IDF na configuração. Para reproduzir a configuração da release:

```sh
cp sdkconfig.release sdkconfig
idf.py build
```

Execute isso em um terminal com o ambiente do ESP-IDF 5.4.4 ativado. O build gera `build/tab5symbian_runtime.bin`; não acessa a placa. Artefatos da instalação independente e avisos de licenças também estão na release. Para o OTA de aplicativo do launcher, use `SymbianOS-runtime.bin`.

## Desenvolvimento

```sh
./tools/idf.sh build
./tools/host.sh test
python3 tools/release.py
```

O primeiro comando compila para ESP32-P4. O segundo verifica a instalação, executa testes de núcleo/organizador e uma jornada gráfica headless. O terceiro gera artefatos locais; não faz upload nem acessa a placa. Consulte [validação e tamanhos](docs/BUILD.md).

O host requer CMake, pkg-config, SDL2 e OpenSSL. O helper seleciona o SDK macOS 15.4 com deployment target 26.0 para contornar a incompatibilidade local SDK/linker. Os helpers aceitam o ambiente do ESP-IDF instalado; `TAB5_IDF_PATH` e `TAB5_IDF_PYTHON_ENV` permitem ajustar os caminhos.

Para executar uma cópia de trabalho:

```sh
mkdir -p build/simulator-data
cp -R sdcard/SymbianOS build/simulator-data/
./tools/host.sh run build/simulator-data/SymbianOS
```

Ctrl+S salva; Ctrl+A/C/X/V edita; Shift+setas seleciona; Tab/Shift+Tab move foco; Enter ativa; Esc volta ou cancela uma operação em andamento; Ctrl+Esc abre Menu; Ctrl+Tab abre o alternador. O teclado virtual permite entrada por toque. No menu clássico, primeiro toque seleciona e o seguinte abre, sem prazo de duplo clique.

## Artefatos

- `dist/SymbianOS-runtime.bin`: somente a aplicação, para OTA pelo launcher.
- `dist/SymbianOS-update-0.8.0.zip`: nova release/manifesto/diretórios, sem documentos ou configurações para sobrescrever os existentes.
- `dist/SymbianOS-SDK-0.8.0.zip` e `sist/0.19.0/packages/`: SDK e exemplos de app/tema.
- `sist/0.19.0/launcher/`: os mesmos arquivos organizados para envio manual pela WebUI.
- `dist/SymbianOS-sdcard.zip`: árvore completa para instalação inicial.
- `dist/runtime-installation/`: partes de uma instalação independente; não enviar pelo OTA de aplicativo do launcher.
- `dist/SHA256SUMS` e `dist/release.json`: hashes e identificação dos artefatos.

Scripts/tema podem ser alterados no SD sem recompilar o residente enquanto sua API for compatível. Execute `python3 tools/package_sd.py --zip dist/SymbianOS-sdcard.zip` após editar a release para regenerar hashes. Faça a troca com o sistema parado e preserve os dados.

[API](docs/API.md), [recuperação](docs/RECUPERACAO.md), [bancada física](docs/TESTES_FISICOS.md) e [licenças](docs/THIRD_PARTY.md). Sem SIS/SISX, OTA próprio ou deep sleep com despertar por alarme nesta entrega. Nenhum flash, monitor, erase-flash ou acesso serial foi executado pelo agente.

## Licença

Código original sob [MIT](LICENSE); bibliotecas, fontes e demais dependências mantêm suas próprias licenças, descritas em [THIRD_PARTY](docs/THIRD_PARTY.md). O texto GPL que existia sozinho no commit inicial do repositório foi preservado em [docs/legacy](docs/legacy/GPL-3.0-initial-repository.txt) como registro histórico.

A 0.13.2 reinicializa os GPIOs Grove antes da UART para eliminar uma possível saída herdada no RX. [Detalhes e limite da validação](docs/RAK_0_13_2.md).

A 0.13.3 habilita a alimentação Grove como no Tab5-Meshtastic, transmite quadros em uma escrita e amplia as tentativas UART. [Investigação e validação](docs/RAK_0_13_3.md).
