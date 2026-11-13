# Tab5 SymbianOS 1.0.0

Plataforma independente para **M5Stack Tab5 (ESP32-P4 + ESP32-C6)**, com interface inspirada em telefones clássicos. Implementação original em C++/Lua, com recursos e dados exclusivamente no microSD em `/SymbianOS`. Teclado oficial I²C obrigatório.

**A versão 1.0.0 tem compatibilidade limitada com a web moderna e com formatos de aplicações. Leia [Limitações conhecidas](docs/LIMITACOES_1_0_0.md) antes de instalar.** O navegador não equivale a Chrome/Firefox, e os SIS aceitos instalam aplicações Lua do perfil deste projeto; não executam aplicativos Symbian originais.

## Instalação pelo launcher

Baixe os arquivos da [release v1.0.0](https://github.com/tpklge/Tab5_SymbianOS/releases/tag/v1.0.0):

1. Confira os hashes em `SHA256SUMS`.
2. Extraia `SymbianOS-SD-1.0.0.zip` e envie a pasta `SymbianOS` pela WebGUI do launcher. Preserve seus dados pessoais existentes; o pacote contém recursos e diretórios vazios.
3. Envie `SymbianOS-runtime-1.0.0.bin` pelo launcher e reinicie. Atualize **BIN e pasta do cartão juntos**.

Requer launcher já instalado e slot de aplicação de **4 MiB**. O pacote não instala nem altera bootloader, tabela de partições ou firmware do C6. A instalação inicial do launcher não é fornecida por esta release. Não há atualizador OTA interno; a WebGUI do SymbianOS serve para transferência de arquivos por token.

## Recursos

- Shell com aplicativos, alternância de tarefas, temas e idiomas português/inglês/espanhol.
- Arquivos, notas, contatos, agenda, alarmes, calculadora e conversor.
- Wi-Fi, e-mail IMAP/POP3/SMTP, feeds e leitura offline.
- Navegador com leitura nativa, HTML/CSS limitado, links por toque, histórico, favoritos, imagens opcionais e JavaScript inline básico.
- Formulários GET/POST e cookies/sessões no SD.
- Transferência web por token, progresso de uploads, download e exclusão de arquivos.
- Aplicações Lua distribuídas em SIS sem assinatura do perfil próprio do projeto; instalação, atualização, migração, rollback e remoção.

## Limites essenciais

- HTML/CSS passa por admissão limitada; páginas podem aparecer simplificadas ou incompletas. Sites com JavaScript moderno, CAPTCHA, OAuth ou proteção automatizada podem não funcionar.
- JavaScript ES5 básico: sem scripts externos, modules, fetch/XHR, timers, DOM completo ou suporte garantido a frameworks.
- Formulários não oferecem select, checkbox, textarea ou upload. Cookies `Domain=` são rejeitados; sessões entre subdomínios não são suportadas.
- SISX, assinaturas, aplicações nativas Symbian e pacotes fora do perfil aceito não são suportados.
- O cartão pode conter credenciais e cookies em texto legível. A transferência web é HTTP local autenticado por token, sem TLS próprio.
- Quota total do sistema no SD: **4.000.000.000 bytes**. Hardware, codecs e recursos disponíveis têm limites próprios.

Detalhes: [Limitações 1.0.0](docs/LIMITACOES_1_0_0.md), [API](docs/API.md), [SIS SDK](docs/SIS_SDK.md) e [roteiro físico](docs/NAVEGADOR_EMAIL_FEEDS_20261113_R3.md).

## Validação

O usuário confirmou que os ajustes finais da revisão R3 funcionaram no Tab5 em 13/11/2026. Esse relato não representa certificação exaustiva de todos os aplicativos, servidores ou cenários. Testes de host/LVGL e compilação ESP32-P4 são registrados separadamente nas [notas da 1.0.0](docs/RELEASE_1_0_0.md). O agente não executou flash nem validação física nesta publicação.

## Compilação e empacotamento

ESP-IDF e dependências são identificados por `dependencies.lock`, configuração e scripts do projeto.

```sh
./tools/host.sh test
IDF_COMPONENT_CHECK_NEW_VERSION=0 ./tools/idf.sh build
python3 tools/package_sd.py --verify
python3 tools/release.py --launcher-only
python3 -B tools/release_sis.py
```

Distribuições locais ficam em `sist/`, ignorado pelo Git, com retenção das duas últimas versões: 1.0.0 e 0.19.0. Binários e ZIPs são publicados como assets da release GitHub. O documento normativo é [plano_sistema_s60_tab5.md](plano_sistema_s60_tab5.md).

## Licenças

Código original MIT; dependências preservam suas licenças próprias. Consulte [LICENSE](LICENSE), [dependências](docs/THIRD_PARTY.md) e os arquivos de licença dos respectivos componentes. Assets fornecidos pelo usuário preservam seus direitos e não são automaticamente relicenciados como MIT. Não são usados código, ícones ou sons proprietários do Symbian/Nokia.
