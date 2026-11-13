# Correções 0.17.1 / API 14

Favoritos usam um ícone original de estrela no atlas do SD, sem depender do caractere ★ na fonte. Ajustes (app energy, aberto pelo menu) inclui Idioma, Tom do alarme e Testes e medições. Idioma oferece Português, English e Español. Voltar das subtelas funciona com textos traduzidos; a navegação não compara mais o título traduzido.

Leitura de e-mail: a mesma mensagem é carregada uma vez por evento de interface e compartilhada entre Lua e preparação HTML; o snapshot é descartado ao fim do evento. Antes, abrir uma mensagem podia reler seu cache até três vezes. Parágrafos simples usam labels em vez de spans. O leitor formatado agora limita o número total de spans a 96, blocos a 48 e texto a 16 KiB (até 4 KiB por trecho), com aviso e alternativa Texto. O original no cache não é alterado. Isso evita o crescimento de objetos e trabalho de layout provocado por mensagens com milhares de tags. Não se afirma que o flash azul físico esteja eliminado sem validação no Tab5; o lua-fault enviado era histórico (0.15.9).

HTML/CSS completo não está implementado. O leitor aceita estrutura básica, ênfase, alinhamento, links e imagens JPEG/PNG/GIF limitadas, sem executar scripts. Não equivale a um navegador: CSS externo, layout flex/grid, fontes web e tabelas complexas não têm compatibilidade integral.

O ZIP incremental e o SD completo incluem /SymbianOS/update/runtime.bin e runtime.sha256, gerados juntos do binário compilado. O kit runtime-OTA continua disponível. Extraia o ZIP preservando essa estrutura e use Atualizações → Revisar runtime → confirmar instalação. A verificação de SHA-256 permanece obrigatória; não se gera um hash do arquivo recebido para substituir o hash esperado. O hash verifica integridade, não é assinatura de autenticidade. Revisar apenas verifica, não instala nem reinicia.

Atualize BIN e conteúdo SD juntos. Não houve flash, serial nem publicação. Ensaios físicos de e-mail e OTA continuam pendentes.
