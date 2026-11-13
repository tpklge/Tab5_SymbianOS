# 0.8.1 — feeds, limites de upload e tecla Sym

## Feeds e downloads

O transporte HTTP agora acompanha respostas 301, 302, 303, 307 e 308, inclusive Location absoluto, relativo, com query ou outro domínio. Há limite de cinco saltos e detecção de ciclos. Cada conexão anterior é fechada antes de abrir a próxima; só o corpo da resposta final HTTP 200 é entregue ao gravador do SD. Continua usando o bundle de certificados e recusa redirecionamento de HTTPS para HTTP, userinfo e controles em URLs.

Foi consultado o endpoint público `https://gizmodo.com.br/feed/`: ele redirecionou para `https://www.gizmodo.com.br/feed/` e depois para `https://www.gizmodo.com.br/feed`. Na consulta de 08/11/2026, a resposta final era RSS com 25.120 bytes e 21 elementos title, dentro do limite atual de 64 KiB. O conteúdo pode variar; não é medição do Tab5. A cadeia correspondente é exercitada pelo transporte simulado nos testes. Isso não é um navegador nem suporte a redirecionamentos em JavaScript/HTML.

Cancelamento, falha de HTTP/TLS, ciclo, limite excedido ou resposta que não é RSS/Atom preservam o snapshot anterior. Mensagens distinguem limite de feed (64 KiB), erro de gravação e resposta inválida. Downloads comuns também ganham suporte a redirecionamentos e continuam limitados a 64 MiB.

## Upload de arquivos

O limite permanece **64 MiB por arquivo (67.108.864 bytes)**. Um arquivo de 100 MB excede esse limite.

A página valida ao selecionar e novamente antes de enviar. Arquivo excessivo exibe tamanho/limite, informa que nada foi enviado e desabilita Enviar. Nome inválido e arquivo vazio têm mensagens próprias. Selecionar um arquivo válido reabilita o botão. O tamanho excedido não dispara fetch nem aloca o arquivo inteiro no Tab5.

O servidor faz sua própria validação de Content-Length antes de receber ou criar temporário e responde **HTTP 413 Content Too Large** com o limite em português. Depois encerra essa conexão sem tentar descartar uma carga enorme não lida. Arquivo vazio recebe 400. Isso também protege clientes que não usam a página. Uma conexão interrompida recebe mensagem útil no navegador, sem presumir que a causa foi o tamanho.

Recarregue a página após atualizar BIN/SD para usar o novo JavaScript. A página é servida com Cache-Control: no-store. Permanecem o token de oito caracteres, confinamento a home/Transfer, buffer de 4 KiB no heap e pilha HTTP de 32 KiB.

## Barra no teclado físico

O mapa anterior não tinha barra na camada Sym. O novo mapa define **Sym + (** como `/`, conforme o uso relatado, e também **Sym + *** como alias. Sym pode ser mantido durante a tecla ou tocado/solto antes dela; nesse segundo caso vale para o próximo caractere. Tocar Sym novamente cancela a seleção pendente. O modo pendente é limpo no reset/desconexão e não muda Ctrl/Alt; acentos e repetição são preservados.

A combinação é interpretada pelo tradutor comum do teclado, com símbolos no mapa externo do SD. Não foi copiado código do aplicativo Meshtastic. Os testes exercitam a matriz real do tradutor, mas não substituem o ensaio físico da revisão do teclado do usuário.

## Instalação e validação

Residente e SD **0.8.1**, API 8. Atualize BIN e SD juntos, usando o ZIP de atualização para preservar redes conhecidas, documentos e configurações.

Testes de host cobrem resolução de Location, cinco códigos de redirecionamento, cadeia do Gizmodo, ciclo, excesso de saltos, cancelamento, snapshot RSS e recusa de 100 MB antes de ler/criar arquivo. O teste da página executa o JavaScript real com DOM/fetch simulados, verificando que o tamanho excessivo não inicia requisição e produz mensagem clara. Roda em Node ou JavaScriptCore/osascript no macOS. Testes de teclado verificam Sym mantido/tocado, repetição, consumo da seleção, reset, alias e acentos.

Build ESP32-P4 e sanitizadores são separados da validação física: ainda é preciso testar o feed, a mensagem no navegador conectado ao Tab5 e a tecla física. O desligamento da tela por inatividade permanece pendente para a fase 7. Nenhum flash, monitor ou acesso serial foi executado.
