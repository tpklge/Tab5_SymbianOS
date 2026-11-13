# Revisão 20261113-R3 — e-mail, feed e navegação

Enviar juntos o BIN e a pasta SymbianOS de `sist/0.19.0/launcher`.
O navegador identifica `13/11 R3`. Esta revisão substitui a R2.

## Evidência recebida

O usuário relatou tela azul sem texto ao abrir e-mail e recuperação ao alternar
para Leitura no Google. O registro de layout mostrava HTML de 10.077 bytes,
15.951 bytes internos livres, maior bloco de 9.216 bytes, 22.062.416 bytes de
PSRAM e 83.308 bytes de pilha livres. Isso demonstra pressão na memória interna;
não identifica sozinho a instrução que causou o reset. O registro Lua recebido
pertence a `services`, com `kind=lua-error`, e não prova erro Lua do navegador.

## Alterações

- A preparação de documentos, metadados retidos das views e processamento de
  rede passam a alocar objetos C++ em PSRAM, além do DOM que já usava PSRAM.
  As pilhas dos workers de rede e e-mail passam de memória interna para PSRAM:
  32 KiB cada. A reserva efetiva deve ser medida no aparelho.
- Alternar para Leitura libera o DOM e o cache de rasterização do documento.
  Falhas de renderização usam texto paginado; um erro de aplicativo não exige
  restaurar uma release anterior. Falhas de alocação na invocação da aplicação
  são capturadas e relatadas.
- E-mail, RSS e navegador usam o mesmo litehtml, com a mesma admissão limitada
  de HTML/CSS. E-mail/RSS agora também passam pela admissão incremental antes
  de Gumbo. Permanecem os limites da R2, os CIDs e o carregamento explícito de
  imagens. Não há execução de JavaScript recebido por e-mail/feed.
- E-mail e feed recebem marcador de renderização interrompida, assim como o
  navegador. Uma abertura interrompida usa leitura alternativa na tentativa
  seguinte. Com menos de 48 KiB internos livres, ou maior bloco abaixo de
  16 KiB, a abertura formatada usa leitura alternativa antes do layout.
- Links HTML têm áreas de toque calculadas sobre os fragmentos visíveis de
  texto/imagem e ajustadas à escala da página. O modo Leitura oferece uma
  lista de links acionáveis abaixo do texto. A navegação usa o mesmo histórico,
  validação de URL e cache. Os controles laterais passam a duas colunas de
  ícones: botões de 54 × 50 px e ícones de 36 px, sem legendas visíveis.
- Resposta interrompida de página GET recebe uma segunda tentativa completa em
  TLS 1.2. Certificado continua validado; respostas chunked sem terminador
  continuam rejeitadas. POST continua sem repetição automática. O diagnóstico
  informa a versão solicitada de TLS. A tentativa não garante que o servidor
  G1 nem o transporte P4/C6 deixem de interromper conexões.
- Validação dos dados do serviço rejeita recorrência inválida de alarme,
  identificadores excessivos e títulos UTF-8 inválidos antes das APIs Lua.
  Arquivos são preservados. Novos registros de falha Lua incluem linha e
  categoria conhecida, sem gravar conteúdo de e-mails, senhas ou erro bruto.

## Conferência física

1. Conferir R3 e abrir um e-mail que reiniciava. Alternar Texto/Formatado dez
   vezes e abrir outra mensagem. Se houver pressão de memória, a leitura
   alternativa deve permanecer utilizável. Conferir imagens CID e anexos.
2. Abrir feed, selecionar artigo, alternar Texto/Formatado e carregar imagens.
   Salvar offline, desligar Wi-Fi e reabrir o artigo.
3. No Google, alternar HTML/CSS/Leitura repetidamente. Os links da lista em
   Leitura e os links sobre o HTML devem navegar por toque. Conferir Voltar,
   Avançar e foco pelo teclado. Verificar ícones e ausência de sobreposição.
4. Abrir `https://g1.com.br/` e `https://g1.globo.com/`, anotando bytes e resultado
   das tentativas. Em nova interrupção, copiar `logs/browser-network.txt` antes
   de tentar outro endereço. A presença de `tls_version=1.2` confirma o fallback.
5. Copiar `logs/browser-render.txt`, `logs/browser-fault.txt`,
   `logs/lua-fault.txt` e `logs/startup.txt`, quando existirem, se houver queda.
   Marcadores de e-mail/feed ficam em `logs/html-render-pending-email.txt` e
   `logs/html-render-pending-feeds.txt`; o do navegador permanece em
   `data/browser/render-pending.txt`.

Nenhum flash, acesso serial ou teste físico foi executado pelo agente.
Testes de host, integração LVGL e build P4 são reportados separadamente.

Validação final: 47/47 testes de host aprovados (`build/browser-r3-validation.log`);
smoke LVGL final aprovado, incluindo clique em links dos dois modos e geometria
dos ícones (`build/browser-r3-ui.log`). Capturas Google/G1/Globo renderizadas
repetidamente em host (`build/browser-r3-pages.log`). Build ESP32-P4 aprovado
(`build/browser-r3-p4.log`), BIN de 3601152 bytes, SHA-256:
`92088e6efd94d7ad5c7a2c98fbe6fafbfbd91032a3e8bac8e7273f33c70c2490`. Validação física pendente.
