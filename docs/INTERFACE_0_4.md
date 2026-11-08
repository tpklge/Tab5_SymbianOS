# Interface e estabilidade — 0.4.0

A interface mantém paisagem 1280×720 e cinza escuro, com estrutura inspirada no S60v5: faixa de status, título com ícone, grade de aplicativos, seleção azul e três comandos inferiores. Ícones são originais MIT; não há assets ou código Nokia/Symbian.

- Calculadora: visor e 20 teclas touch; operações, parênteses, limpar, apagar e igual. Enter no teclado físico calcula. Expressões inválidas e divisão por zero mostram erro recuperável; o próximo cálculo continua funcionando.
- Contatos: lista de nomes com telefone/e-mail, ordenada alfabeticamente antes da paginação, ignorando caixa e os acentos portugueses comuns. Busca, edição, importação/exportação vCard e exclusão preservados. Nome, telefone e e-mail têm campos separados.
- Relógio: lista de alarmes e formulário para ajustar data/hora ou fuso. Voltar da lista retorna ao início; de um formulário retorna à lista.
- Conversor: campos separados para valor, unidade de origem e unidade de destino.
- Menu, alternador e demais apps compartilham o mesmo acabamento e navegação.

## Correções concretas

O sdkconfig anterior usava `CONFIG_LV_USE_BUILTIN_MALLOC=y`, com **64 KiB fixos para todo o LVGL**. O simulador já usava malloc do host; portanto seu teste anterior não exercitava esse limite. O renderer também construía a próxima árvore mantendo a anterior viva. Isso cria uma condição de esgotamento compatível com os reinícios relatados; sem backtrace físico não é possível atribuir todos eles definitivamente a essa causa.

A versão nova usa um alocador LVGL explícito em PSRAM, inclusive fontes/widgets. Os buffers RGB565 de desenho e rotação também usam PSRAM DMA no ESP32-P4. A árvore anterior é liberada antes da construção da nova. O build recusa configuração sem o alocador esperado.

As operações Lua usam um executor FreeRTOS permanente, com pilha de 64 KiB, em vez de criar uma pthread de 24 KiB a cada toque/tick. Áudio tem executor separado de 16 KiB; a tarefa LVGL usa 16 KiB. Falha de criação do executor retorna diagnóstico. Esses valores são configuração, não medições de consumo físico.

A última ação foreground é registrada atomicamente em `/logs/last-action.txt`, com etapa, app, heap interno livre/maior bloco, PSRAM livre e mínimo de pilha restante do worker. `/logs/boot.txt` registra o motivo numérico do reset. Não registram o texto digitado. Escrita desses diagnósticos é limitada a arquivos curtos; falha de gravação não bloqueia o app. Serviços de fundo não gravam esse log a cada tick. Após reinício, preservar os arquivos antes de executar outras ações.

## Verificação

237 verificações de host (62 núcleo e 175 organizador) passaram. A jornada LVGL/SDL executa 20 ciclos de ajustar relógio → voltar → Abertos, converter → Abertos → voltar e criar contato → voltar → início. Também ativa as teclas da calculadora por eventos LVGL e confere 2 + 3 = 5, além de clipboard UTF-8, teclado virtual por ponteiro, alarmes e recuperação do cartão.

ASan/UBSan passou nos testes de núcleo/organizador. O executável gráfico instrumentado não completou: a biblioteca SDL2 do host abortou em `dllinit`, durante a inicialização do loader, antes de `main`; a tentativa mínima também não iniciou dentro do prazo. Portanto não há aprovação sanitizada da GUI. A jornada gráfica normal passou.

O build ESP32-P4 é separado dos testes do host. A versão ainda precisa ser instalada e exercitada no Tab5 para confirmar a correção dos reinícios, RTC, toque, teclado e memória real. Nenhum flash, monitor ou acesso serial foi executado.

## Atualização

Instalar **BIN 0.4.0 e release SD 0.4.0**: os novos layouts exigem API 3. O ZIP de atualização preserva documentos e configurações existentes; a release 0.3.0 permanece disponível no cartão como arquivo histórico. Ver [instruções do launcher](ATUALIZACAO_LAUNCHER.md).

Prévias de host: [Menu](preview-menu-0.4.png), [Calculadora](preview-calculator-0.4.png), [Contatos](preview-contacts-0.4.png). Os contatos da prévia são fictícios e não fazem parte do pacote de dados.

Para regenerar ícones: instalar `requirements-artwork.txt` em um venv e Cairo no host; executar `python tools/make_icons.py` e depois `python3 tools/package_sd.py`. O script gera SVGs originais e atlas RGB565+A8 de 96×96, com cabeçalho `T5IC`, dimensão/contagem uint16 LE e entradas nome[16] + RGB565 LE + alpha8. Recursos ficam exclusivamente no SD.
