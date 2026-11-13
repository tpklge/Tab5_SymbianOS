# Navegador móvel — 0.19.0

**Revisão atual 13/11 R1:** o navegador usa extração em fluxo e leitor nativo, com CSS/layout remoto suspensos. As descrições anteriores abaixo são históricas. Veja [NAVEGADOR_20261113_R1.md](NAVEGADOR_20261113_R1.md).

Perfil padrão implementado em 12/11/2026.

- Páginas do navegador usam um User-Agent de compatibilidade móvel, `Sec-CH-UA-Mobile: ?1`, `Save-Data: on` e `Viewport-Width: 592`.
- O mesmo perfil acompanha redirecionamentos, repetições e o carregamento explícito de CSS/imagens do navegador. Downloads genéricos, feeds e recursos do e-mail mantêm o perfil anterior.
- O HTML/CSS do navegador usa viewport lógico de **592 × 176**, ampliado duas vezes na área de conteúdo do Tab5 em paisagem. Consultas CSS como `max-width: 600px` passam a corresponder ao viewport móvel. A interface mostra **Móvel**.
- O cache das páginas e dos recursos móveis usa uma chave própria. Uma cópia desktop antiga não impede a primeira requisição móvel; não é necessário apagar manualmente o cache. A primeira abertura dessa nova variante precisa de rede; depois ela pode ser reaberta pelo cache móvel no SD.
- Permanecem leitura leve automática para páginas grandes, limite de 1 MiB, proteção de memória/pilha, token da WebUI e persistência exclusivamente no cartão. O perfil de compatibilidade não acrescenta Android, JavaScript, cookies de sessão ou envio de formulários ao motor.

O servidor pode ignorar os indicadores ou entregar uma página maior. Nesta rodada, requisições HTTP/1.1 do host com o novo perfil receberam HTTP 200 do Google (226.427 bytes) e do G1 (890.705 bytes). Esses resultados são observações das respostas obtidas no computador; não demonstram redução de tráfego nem funcionamento no Tab5.

## Validação

- **44/44 testes de host e smoke da interface passaram**, em `build/mobile-browser-host.log`.
- Regressões específicas passaram em `build/mobile-browser-targeted.log`: viewport do Runtime, CSS responsivo, cache desktop antigo ignorado, backend móvel para recursos e isolamento dos caches móvel/genérico.
- As respostas obtidas com o perfil móvel passaram pela integração Runtime/Lua/cache/imagens no host: Google formatado, 3 páginas; G1 em leitura leve, 10 páginas. Registro em `build/mobile-browser-pages.log`.
- Build ESP32-P4 concluído, em `build/mobile-browser-p4.log`.

Não foi executado flash nem aberto monitor/serial. Validação física pendente.

## Teste no Tab5

1. Enviar BIN e pasta SymbianOS de `sist/0.19.0/launcher/` pela WebGUI do launcher e reiniciar.
2. Com Wi-Fi, abrir `https://www.google.com.br/` e `https://g1.globo.com/`. Conferir **Móvel** na apresentação; o G1 pode indicar também **Leitura leve**.
3. Percorrer as páginas e abrir links. Carregar CSS/imagens pelo comando existente, se desejado. O número de páginas pode variar conforme a resposta atual do servidor.
4. Reabrir uma página sem rede para conferir a nova cópia móvel em cache.
5. Se houver reinício, copiar pelo launcher `logs/browser-render.txt` e `logs/startup.txt`, antes de abrir outra página. O perfil móvel não substitui a investigação de um eventual panic físico.
