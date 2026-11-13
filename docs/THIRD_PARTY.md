# Dependências e licenças

Código original do projeto sob MIT (`LICENSE`). Dependências mantêm suas licenças próprias.

- Lua 5.4.9: MIT, texto em `components/lua/lua-5.4.9/doc/readme.html` e cabeçalhos upstream.
- ESP-IDF: licença da distribuição local, identificada por commit em `dependencies.json`; seus subcomponentes podem ter licenças próprias.
- Componentes gerenciados: versões/hashes em `dependencies.lock`; caminhos dos arquivos de licença baixados inventariados em `licenses.json`.
- Demos M5Stack: referências de consulta com `LICENSE` preservado, commits registrados; não incorporados ao firmware.

Este inventário é de preparação, não uma auditoria final de distribuição. Na entrega, incluir os avisos de todos os componentes efetivamente usados e verificar fontes/assets externos.

A interface externa inclui Noto Sans Regular, sob SIL Open Font License 1.1. Arquivo e licença ficam exclusivamente na release do SD (`theme.ttf` e `FONT-LICENSE.txt`). SHA-256 e URL de origem registrados em `dependencies.json`. O renderer residente usa tiny_ttf do LVGL; preservar os avisos da biblioteca e de seu backend stb. A fonte mínima Montserrat 14 do LVGL é a exceção de diagnóstico residente prevista no plano.

O simulador usa SDL2 e OpenSSL instalados no host. Eles não são linked ao firmware ESP32-P4. O formatador clang-format é apenas ferramenta de desenvolvimento.

StyLua 2.6.0 é ferramenta opcional de desenvolvimento, sob MPL 2.0. O binário e a licença original ficam em `.tools/`, fora dos artefatos do firmware/SD. O SHA-256 do ZIP oficial foi conferido e registrado em `dependencies.json`.

Os ícones SVG em `artwork/` são originais, sob MIT, rasterizados para `icons.bin` exclusivamente no SD. A ferramenta opcional `tools/make_icons.py` usa CairoSVG (LGPLv3), Pillow (MIT-CMU) e Cairo (LGPL/MPL), instalados no host; essas bibliotecas não são distribuídas no firmware nem no SD. Versões Python fixadas em `requirements-artwork.txt`. Nenhum asset Nokia/Symbian foi incorporado.

minimp3 (CC0) e stb_image/stb_image_write (MIT ou domínio público, licença dupla upstream preservada) fornecem os codecs nativos necessários ao streaming/decodificação/JPEG. Fontes, commits e SHA-256 estão em `components/platform/vendor/sources.json`; licenças originais estão no mesmo diretório, na release do SD e em `dist/licenses`. Origens primárias: https://github.com/lieff/minimp3 e https://github.com/nothings/stb. Os arquivos de teste de áudio/imagem são sintéticos originais e não entram no firmware/SD. A galeria não ativa ThorVG nem incorpora demos.

G.711 μ-law e o contêiner AVI são implementações originais; MJPEG reutiliza o codec JPEG já listado. Não foi acrescentada biblioteca de áudio/vídeo nesta revisão.

Na 0.7.0: ESP-Hosted 1.4.0 e Wi-Fi Remote 0.8.5, Apache-2.0, e dependências resolvidas em dependencies.lock. Licenças em docs/licenses.json e dist/licenses. O build compila um overlay dos dois arquivos de integração do ESP-Hosted para remover abort em falta do transporte, limitar espera e cancelar; o construtor automático é excluído. Fontes e licenças upstream são preservadas em managed_components; alterações explícitas em CMakeLists.txt e docs/FASE_6_0_7.md. A página web, cliente HTTP, controle de energia, G.711/AVI e ícones novos são originais MIT. Nenhum firmware do C6 é embutido ou alterado.

0.12.0: NimBLE do ESP-IDF (Apache/Mynewt e licenças upstream preservadas) habilitado como central sobre ESP-Hosted. Tab5-Meshtastic, GPL-3.0, foi consultado para documentação de interoperabilidade; nenhum arquivo/código desse projeto é incorporado. Codec protobuf parcial, framing, transportes, telas e ícones desta integração são implementação original MIT. Referências e limites em MESHTASTIC_0_12.md.

0.13.0: ícones dos aplicativos fornecidos pelo usuário em icons/, usados a seu pedido; arquivos e hashes no icon-sources.json da release. USER-ICONS.txt não atribui a eles a licença MIT do código. Os controles vetoriais remanescentes são originais; não foram copiados bitmaps/logotipos das fotografias de referência. Noto Sans e demais licenças permanecem preservadas.

A integração HTTPD compila um overlay gerado a partir do ESP-IDF local para corrigir a liberação de sockets quando a tarefa HTTP não pode ser criada. O SDK original não é alterado; o copyright/licença upstream permanecem no arquivo de build. O patch original do projeto está em tools/patch_httpd.py e sua aplicação explícita em CMakeLists.txt. Detalhes e validação: HTTPD_TASK_ENVIO_WEB.md.

Na revisão 20261113-R2: Duktape 2.7.0 (MIT), obtido de
https://duktape.org/duktape-2.7.0.tar.xz. Fontes/licença em
`components/javascript/vendor/`, configuração e alterações descritas em
`components/javascript/README.md`. A licença também acompanha o SD em
`system/releases/0.19.0/duktape-LICENSE.txt`.
