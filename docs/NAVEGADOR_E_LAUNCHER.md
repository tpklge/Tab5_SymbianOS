# Navegador, PIN e distribuição pelo launcher

**Revisão atual 13/11 R1:** o navegador usa extração em fluxo e leitor nativo, com CSS/layout remoto suspensos. As descrições anteriores abaixo são históricas. Veja [NAVEGADOR_20261113_R1.md](NAVEGADOR_20261113_R1.md).

O menu inclui Navegador. Digite uma URL HTTP(S) e use Abrir ou Enter no teclado físico. A página é renderizada pelo litehtml com o CSS suportado pelo motor existente. Não há JavaScript, DOM de navegador moderno, submissão de formulários, cookies ou sessões autenticadas. Portanto não se trata de suporte completo à web moderna.

Use Anterior/Próxima para percorrer a página renderizada e Links para escolher um destino por toque ou teclado. Voltar página/Avançar percorrem o histórico; Recarregar solicita uma nova cópia. Favoritos e histórico ficam no SD, limitados a 32 entradas cada. O cache conserva até 12 páginas, de até 1 MiB de HTML por página, e permite reabrir uma página sem rede. O endereço final após redirecionamentos é usado como base dos links e recursos relativos. Uma falha de transferência, cancelamento ou resposta grande demais preserva a cópia anterior.

CSS e imagens externos são solicitados pelo botão Carregar CSS/imagens. Os limites existentes do leitor continuam valendo: recursos em cache limitado, até quatro rodadas para dependências, imagens codificadas de até 256 KiB e CSS de até 64 KiB. Não há carregamento ilimitado em segundo plano. Links HTTP(S) são extraídos do documento pelo motor; protocolos como javascript/file não são executados. Âncoras internas não têm rolagem específica nesta versão.

A tela de PIN mostra OK e indica o uso de Enter no título. Toque em OK ou pressione Enter para confirmar os quatro dígitos; Backspace apaga. Repetição automática de teclas é ignorada no bloqueio. PIN errado mantém o bloqueio e tentativas repetidas ativam a espera existente. O teste físico pendente é digitar rapidamente no teclado oficial, alternar com toque, bloquear novamente e reiniciar.

O aplicativo Atualizações e as operações Lua de OTA foram retirados do catálogo/API. O runtime não contém operações de iniciar/gravar/selecionar uma imagem de atualização. A confirmação de saúde da imagem iniciada pelo launcher permanece para a integração de boot. Reiniciar fica em Ajustes, com confirmação e checkpoint de aplicativos.

A interface web de Wi-Fi e transferências permanece com token de sessão e envio de arquivos para home/Transfer. Ela não instala firmware. Use a WebGUI do launcher para enviar o binário de aplicação e a pasta SymbianOS. tools/release.py gera apenas essa entrega, inclusive sem a opção --launcher-only; não prepara kits OTA.

Persistência nova: `/SymbianOS/data/browser/`, além do cache de recursos HTML já existente em `/SymbianOS/cache/web-images/`. Nenhum uso de NVS foi adicionado.

Validação física informada pelo usuário: inicialização e Wi-Fi funcionando. E-mail será testado na próxima entrega. Navegador, confirmação por Enter e reinício desta entrega ainda exigem teste no Tab5; resultados de host/build não substituem essa validação.

O plano futuro de pacotes está em [PLANO_PACOTES_SIS_SISX.md](PLANO_PACOTES_SIS_SISX.md). Não há implementação SIS/SISX nesta entrega.

## Validação desta entrega

- Host: 31/31 testes passaram (`build/browser-host-final.log`).
- Interface LVGL no simulador: smoke passou após atualização dos testes que ainda procuravam OTA; inclui PIN digitado rapidamente pela matriz física simulada, confirmação por Enter, desbloqueio por toque e Enter no campo de endereço do navegador.
- ESP32-P4: build concluído (`build/browser-esp32p4.log`). BIN: 3.268.816 bytes; SHA-256 `5e1798120fc0158c4fad45146a87ad8932f125cb4f5211b42b6345b29e46979b`.
- Entrega gerada e manifestos verificados em `dist/launcher/`. Nenhum flash ou teste físico foi executado nesta implementação.

Páginas com mais de 512 elementos preparados usam leitura leve automática (texto e links, priorizando o conteúdo principal), identificada na tela. Isso limita o DOM para a memória do Tab5. Páginas pequenas mantêm HTML/CSS formatado. Diagnóstico e limites em [NAVEGADOR_MEMORIA_NATIVA_E_LEITURA_LEVE.md](NAVEGADOR_MEMORIA_NATIVA_E_LEITURA_LEVE.md).

O navegador agora solicita páginas e recursos com perfil móvel e viewport responsivo de 592 × 176, ampliado na tela. O cache móvel é independente do cache anterior. Detalhes e teste físico em [NAVEGADOR_MOBILE.md](NAVEGADOR_MOBILE.md).
