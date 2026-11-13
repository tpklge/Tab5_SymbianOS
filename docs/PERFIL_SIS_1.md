# Perfil SIS 1 — aplicações Lua sem assinatura

Estado: fases 1–6 implementadas para SIS sem assinatura; este documento descreve o perfil binário inicial. O [ciclo de vida e perfil 2](SIS_CICLO_DE_VIDA.md) acrescenta atualização, dependências, migração, rollback e remoção. Testes de host e build são separados de validação física. SISX e assinaturas ficam para a última fase. Este perfil não executa aplicações de telefones Symbian.

## Contrato binário e de aplicação

Usamos o contêiner SIS v9 documentado, com campos e identificadores padronizados, controlador e dados separados, arrays e padding. Cabeçalho: UID1 `0x10201a7a`, UID2 zero, UID3 local entre `0xe0000000` e `0xefffffff`, checksum CRC16 dos bytes pares/ímpares. Todos os inteiros são little endian. Strings são UCS-2 sem controles ou substitutos. Fonte: [especificação original SIS v9, v1.1](https://www.cryer.co.uk/file-types/s/sis/softwareinstallsis.pdf), hospedada em espelho. A consulta serve para interpretar o formato; não copiamos código do SDK nem recursos proprietários.

O controlador contém Info, opções vazias, um idioma estrutural (English/1), pré-requisitos vazios, propriedades vazias, bloco de instalação de arquivos e índice zero. Texto exibido no aplicativo pode estar em português. Timestamp fixo 2026-01-01 UTC para determinismo. Sem logos, condições, pacotes embutidos, scripts ou execução durante instalação. Operação permitida: instalar arquivo, opções zero. Algoritmo de compressão: nenhum; Deflate é recusado explicitamente nesta versão.

O hash SHA-1 do formato legado é verificado como checksum de conteúdo, sem qualquer alegação de autenticação. Além dele, o manifesto Tab5 exige SHA-256 dos arquivos; o SHA-256 do pacote vincula revisão e confirmação. Como o pacote inteiro é sem assinatura, esses hashes não provam autoria.

Cada descrição aponta exclusivamente para `!:\SymbianOS\apps\<id>\<versão>\<nome>`. O instalador interpreta esse destino de forma confinada, sem confiar em caminhos fornecidos pelo pacote. O diretório real é `/SymbianOS/apps/<id>/<versão>/`, compatível com o runtime atual; não introduzimos uma segunda árvore `versions`.

O payload inclui `manifest.txt`, `main.lua` e `LICENSE.txt`. Manifesto interno: contrato existente de aplicação Lua (`format=1`, ABI de aplicações 4), acrescido de `sis_profile=1`, `uid` decimal, `license=MIT` e `runtime_api=14`. Versão tem três componentes numéricos. UID, autor, título, versão, arquivos e hashes precisam coincidir com o controlador. O UID identifica pacotes locais de desenvolvimento, sem alegação de registro oficial. IDs de aplicativos de sistema são reservados.

Permissões nesta fase: **somente dados próprios**, sem câmera, rede, arquivos compartilhados ou concessões adicionais. A API Lua isolada, teto de memória e orçamento de instruções existentes continuam obrigatórios. Não são aceitos bytecode Lua ou executáveis `.exe`, `.dll`, `.bin`. A instalação não roda o aplicativo; a abertura acontece pelo menu após confirmação.

## Limites da primeira implementação

- Pacote e soma dos arquivos: até 4 MiB.
- Até 32 arquivos da aplicação mais o manifesto gerado; nomes planos ASCII, únicos também sem distinguir maiúsculas/minúsculas.
- Arquivo: até 1 MiB; Lua em fonte: até 64 KiB; controlador: até 64 KiB; manifesto: até 16 KiB.
- Até 32 aplicações instaladas. O perfil 1 não declara dependências; atualização, retenção e remoção agora seguem SIS_CICLO_DE_VIDA.md.
- Perfil deliberadamente menor que os tetos propostos no plano: aumentar limites e adicionar compressão depende de medições físicas. Não há promessa de ler qualquer SIS v9 existente.

A inspeção lê o pacote por intervalos, verifica cada arquivo e o descarta antes do seguinte. Usa até um arquivo de 1 MiB mais estruturas limitadas, sem carregar o pacote todo. A instalação mantém um mapa de payload limitado a 4 MiB para reutilizar verificações e staging. Esses são limites lógicos, não medições do pico real de heap/PSRAM. A tela informa o tempo efetivamente gasto na inspeção; no simulador esse tempo é de host.

## Geração e inspeção no computador

O arquivo de autoria é `sis.json`, com somente `id`, `uid`, `version`, `title`, `author`, `license` e `files`. Exemplo MIT original: `sdk/sis-hello/`. O gerador desta fase recebe esse manifesto; não interpreta arquivos PKG do SDK legado.

```sh
python3 -B tools/make_sis.py sdk/sis-hello dist/packages/sis-hello-1.0.0.sis
./tools/host.sh build
build/host/sis_inspect dist/packages/sis-hello-1.0.0.sis
```

O gerador Python produz `.sis` e relatório `.sis.json` com tamanho e SHA-256. O inspetor C++ usa o mesmo parser nativo do dispositivo, independentemente da implementação do gerador; a entrada original é somente lida, e uma cópia temporária isolada é usada para a inspeção. A extensão não substitui a validação estrutural. Uma renomeação de ZIP/T5PKG não produz SIS.

O teste de determinismo fixa o SHA-256 do exemplo original. Não declaramos interoperabilidade com instaladores de telefones ou ferramentas legadas sem validação externa adicional. O gerador não incorpora o SDK original, e o código novo é MIT. Licenças de arquivos de terceiros precisam ser preservadas em futuros perfis; este exemplo e o perfil inicial são MIT.

## Fluxo no Tab5

1. Enviar `.sis` pela interface web com token para `/SymbianOS/home/Transfer/`; `/update/packages/` também é aceito.
2. Abrir Instalador e selecionar o pacote. A inspeção mostra título, autor declarado, UID, versão, tamanho, permissões, tempo e **sem assinatura / autoria não verificada**. Ainda não instala nem executa nada.
3. Confirmar Instalar por toque ou selecionar o botão com o teclado e usar Enter. O hash revisto precisa ser o mesmo no início da instalação; alterações exigem nova revisão.
4. Após sucesso, abrir a aplicação pelo Menu. Nenhuma atualização de firmware ou uso de NVS participa desse fluxo.

## Transação e recuperação

A ativação é uma troca atômica do catálogo `/apps/registry.tsv`; o payload fica invisível até esse ponto. Journal restrito em `/meta/sis-install.txt`, staging em `/tmp/install/sis/`. O journal é persistido antes do payload. Estados: `prepared` antes da extração, `committing` antes de publicar a versão, `committed` após ativar o catálogo. Cada arquivo passa por leitura de conferência antes da publicação. UID e ID devem estar livres, diretório de destino deve estar ausente, e há conferência de quota global, pico estimado e espaço físico antes das escritas.

No boot e antes de ler o catálogo, a recuperação consulta o journal. Se a linha de ativação não está no catálogo, remove apenas a versão incompleta e o staging da transação. Se a linha está ativa, confere manifesto e cada hash antes de limpar o journal. Arquivos ativos danificados ou journal inválido causam falha explícita com preservação do diagnóstico; não são apagados silenciosamente. Recuperação repetida é idempotente. A geração do Storage vincula a operação ao cartão montado; cada mutação confere essa geração sob o mesmo mutex das operações de montagem, e o cancelamento é verificado durante leitura, staging e antes da ativação. Depois do commit do catálogo a instalação já é efetiva.

O diretório de dados existente não é apagado. Atualizações, migração, rollback e remoção usam o journal adicional documentado em SIS_CICLO_DE_VIDA.md. O fluxo original de instalação do perfil 1 é preservado por compatibilidade.

FAT, cache do controlador do cartão e quedas reais de energia exigem testes no hardware. Os testes de host injetam interrupções entre escritas persistidas e na ativação; não equivalem a cortar energia no Tab5. A reserva global de 4.000.000.000 bytes e a margem operacional do Storage continuam em vigor.

## Validação física pendente

Inspecionar e instalar exemplo no Tab5 usando o teclado oficial, medir RAM/PSRAM e duração com arquivos perto dos limites, testar cancelamento pela UI, falta de espaço, remoção e reinserção do cartão, desligamento em diferentes pontos e boot de recuperação. Não houve autorização para flash/serial nesta tarefa, portanto essas medições não foram executadas. A entrega inclui BIN e pasta SymbianOS para o launcher.

## Resultados da entrega

- Host: 34/34 testes e smoke da interface passaram (`build/sis-validation.log`). O grupo do gerador inclui 10 casos de contrato; o parser foi exercitado em todos os cortes do exemplo, 300 mutações e pontos de interrupção de instalação, além de cancelamento, quota, remoção de cartão e corrupção.
- ESP32-P4: build concluído (`build/sis-device-build.log`).
- Binário: 3293456 bytes; SHA-256 `34aba3011619d944528d0dbadf732ae57aab607a712fcc4c4aff709b56d7e210`.
- Exemplo `dist/packages/sis-hello-1.0.0.sis`: 2.816 bytes, sem assinatura. A pasta e o binário foram gerados em `dist/launcher/`.
- Sem flash, serial ou medições físicas nesta tarefa.
