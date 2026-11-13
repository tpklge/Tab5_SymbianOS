> Revisão atual: [20261113-R3](NAVEGADOR_EMAIL_FEEDS_20261113_R3.md). Os limites básicos abaixo permanecem; os controles e a preparação de e-mail/feed foram revisados.

# Navegador 20261113-R2 — controles, documentos e sessões

Entregar juntos o BIN e a pasta SymbianOS preparados em `sist/0.19.0/launcher`.
A tela de endereço identifica `13/11 R2`; a aplicação confere a revisão do runtime.
Não foi executado flash, monitor nem acesso serial nesta revisão.

## Situação dos seis itens

1. **Controles:** coluna direita de 188 px, botões de 180 × 38 px, paginação de
   160 × 44 px abaixo do conteúdo. Texto/imagem têm área própria. O teste LVGL
   verifica dimensões e ausência de sobreposição.
2. **Navegação:** testes de host para endereço/Enter, cache, Links, histórico,
   favoritos, cancelar, paginação e alternância entre leitura e HTML/CSS.
   Toque, rolagem e teclado I²C no aparelho ainda requerem o roteiro abaixo.
3. **Estabilidade:** Google, G1 e Globo foram exercitados com capturas reais em
   host, incluindo Globo de 2,34 MB e três ciclos de renderização/paginação por
   site. Isso não mede rede TLS, memória nem estabilidade física do P4.
4. **Queda anterior:** o registro fornecido terminava em `parse`, com heap e
   pilha ainda disponíveis. Não foi possível identificar a causa exata sem o
   panic/trace físico. O motor agora registra `gumbo-parse`, `dom-create`,
   `gumbo-destroy`, `css-master`, `dom-attributes`, `css-author`, `css-compute`
   e `render-tree`, além de layout/desenho. Um marcador no SD impede repetir
   automaticamente uma renderização interrompida do mesmo documento. A leitura
   alternativa preserva o último diagnóstico útil em `logs/browser-fault.txt`.
5. **HTML/CSS/imagens:** disponíveis pelo botão **HTML/CSS**. A abertura padrão
   permanece em leitura nativa, que o usuário confirmou funcionar. Uma camada
   incremental admite até 256 elementos, 24 níveis e cerca de 32 KiB de marcação,
   além de CSS limitado. Mantém cores, ênfase, alinhamento, margens/padding,
   listas, títulos, links e imagens. Imports, overlays, CSS complexo e scripts
   externos não são admitidos. Imagens são baixadas explicitamente por
   **Carregar imagens**, até 16 por rodada e 256 KiB por recurso; cache no SD.
6. **JavaScript/formulários/sessões:** implementação básica, descrita abaixo.
   Não equivale a compatibilidade completa com browsers modernos nem garante
   login nos sites Google/Globo.

## Recursos e limites

Duktape 2.7.0 executa scripts inline compatíveis com ES5; scripts externos,
modules, fetch/XHR, timers, eventos DOM completos e frameworks modernos não são
implementados. A ponte oferece `document.write/writeln`, `document.title`,
`document.getElementById(...).textContent` e `document.cookie`. Alterações no
texto e escrita aparecem no modo HTML/CSS. O leitor nativo preserva o texto
original como alternativa. Código incompatível é descartado e o erro fica no
cache/diagnóstico da página. Não há acesso JS ao filesystem, APIs Lua ou rede.

Limites JS: 32 KiB de código, heap de 1,5 MiB em PSRAM, prazo de 300 ms para
bytecode/RegExp, recursão e chamadas limitadas. Funções C do interpretador não
são preemptadas individualmente. Laços, falta de memória, recursão e recuperação
são testados em host; desempenho/stack do P4 requerem teste físico.

**Formulários** abre uma lista separada. Suporte a inputs text/password/email/
search/number/hidden/submit e URL encoding GET/POST. Até 8 formulários de 12
campos, quatro campos editáveis por tela; Enter envia. O destino aparece antes
do envio. Não há select, checkbox, textarea, uploads ou JavaScript de submit.
POST exige mesma origem; campo password exige HTTPS. POST não é repetido
automaticamente após falha. Redirecionamento 301/302/303 passa a GET; reenvio
307/308 é recusado. Senhas enviadas não entram no histórico nem em logs.

Cookies ficam em `/SymbianOS/data/browser/cookies.tsv`, exclusivamente no SD.
Até 64 cookies, host exato, caminho, Secure, HttpOnly, SameSite, Max-Age/Expires
básicos. `Domain=` é rejeitado: sessões que dependem de cookies entre subdomínios
não funcionam nesta versão. Cookies de sessão são retomados do SD até logout ou
**Limpar sessões**. Esse comando remove cookies, páginas e imagens do navegador;
histórico/favoritos permanecem. Imagens privadas usam cache separado em
`data/browser/assets`. A persistência respeita a geração do cartão.

A proteção de renderização fica em `data/browser/render-pending.txt`.
Para repetir manualmente um documento bloqueado, fechar o navegador e remover
esse arquivo pela WebGUI do launcher. Copiar os logs antes. Não é necessário
apagar a flash nem criar NVS.

## Roteiro físico

Anotar resultado, duração, revisão e eventual reinício para cada passo.

1. Enviar **ambos** os artefatos pelo launcher, iniciar e conferir `13/11 R2`.
   Abrir uma página pelo Enter do teclado oficial. Confirmar que texto ocupa a
   área esquerda, controles ficam à direita e nenhuma linha cobre o conteúdo.
2. Rolar com toque e teclado; usar Anterior/Próxima no documento. Abrir Links,
   visitar um link, voltar/avançar no histórico, recarregar, favoritar e reabrir.
   Repetir em HTML/CSS e voltar à Leitura.
3. Na mesma rede, no computador executar:
   `python3 tools/browser_test_server.py --bind 0.0.0.0 --port 8765`.
   No Tab5 abrir `http://IP_DO_COMPUTADOR:8765/`. Usar dados fictícios.
4. Em HTML/CSS conferir título verde, texto azul e **JavaScript básico OK**.
   Clicar **Carregar imagens**, aguardar e conferir a imagem azul. Verificar
   resposta da interface durante o carregamento e retorno à Leitura.
5. Em Formulários enviar a busca com `ação & teste`: o resultado deve mostrar
   GET OK com o texto preservado. Enviar o formulário de nome fictício: POST
   deve redirecionar para **Sessão OK**. Recarregar e reiniciar o aplicativo;
   conferir que o cookie permanece. Usar logout e depois Limpar sessões;
   conferir **Sem sessão** em novo carregamento.
6. Abrir os links Página de 2 MiB, Estrutura profunda, Laço JS e Memória JS.
   O conteúdo deve continuar disponível; erros JS não devem reiniciar o Tab5.
   Depois abrir a página inicial novamente para comprovar recuperação.
7. Abrir `https://www.google.com.br/`, `https://g1.globo.com/` e
   `https://www.globo.com/` cinco vezes cada, alternando os modos. Anotar bytes
   recebidos, erro HTTP/TLS e funcionamento de Links. O conteúdo pode ser um
   trecho; scripts e login modernos não são critério de compatibilidade.
8. Cancelar uma transferência, desligar/religar Wi-Fi e repetir. Se houver tela
   azul, antes de tentar várias vezes copiar pelo launcher os arquivos
   `logs/browser-fault.txt`, `logs/browser-render.txt`, `logs/browser-network.txt`,
   `logs/startup.txt` e `logs/lua-fault.txt`, quando existirem. A falha de TLS é
   uma condição separada da renderização; o diagnóstico não deve ser tratado
   como prova de correção física.

## Validação da revisão

Logs de host e P4 ficam em `build/browser-r2-validation.log` e
`build/browser-r2-p4.log`. Integração com capturas públicas em
`build/browser-r2-pages.log`. Nenhuma captura de site é distribuída no SD.
Os resultados finais destes comandos devem ser consultados junto dos logs;
o roteiro físico permanece pendente até execução no Tab5.

Resultado final: **47/47 testes de host e smoke LVGL aprovados**; build
ESP32-P4 aprovado, BIN de 3592432 bytes. SHA-256:
`b95aacc2321958e3d9dfa0f8eb4554a33b9c86c616ef2a8f27c62269b7c16d69`. Os testes físicos acima não foram executados.
