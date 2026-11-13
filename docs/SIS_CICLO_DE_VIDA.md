# SIS sem assinatura — atualização, dados e recuperação

Esta entrega implementa as fases 5 e 6 do plano, mantendo assinatura e SISX para a última fase. A validação física ainda está pendente. O formato continua sendo um perfil restrito do contêiner SIS v9 para aplicações Lua originais, sem compatibilidade com executáveis de telefones.

## Perfil 2

O perfil 1 continua aceito. O perfil 2 é escolhido pelo gerador quando `sis.json` declara `schema`, `permissions`, `dependencies` ou `migration`. Permanecem os limites de pacote/payload de 4 MiB, até 32 arquivos da aplicação mais o manifesto, arquivos de até 1 MiB e Lua de até 64 KiB. Não há compressão, pacotes embutidos, scripts de instalação ou SISX. O arquivo de migração gerado conta no limite de arquivos.

Novos metadados:

- `schema`: inteiro de 1 a 9999, padrão 1.
- `permissions`: lista sem repetições dentre `documents`, `media`, `camera`. Lista vazia significa somente dados próprios. A revisão mostra permissões adicionais antes de confirmar. Capacidades como instalação, segurança, firmware e acesso irrestrito continuam indisponíveis.
- `dependencies`: até 16 pares UID/faixa inclusiva de versões, com chaves `uid`, `min`, `max`. Usam dependências SIS no controlador e são repetidas no manifesto; divergência é recusada.
- `migration`: objeto com `from`, `rename` e `defaults`, descrito abaixo.

A identidade é o par ID/UID. Outro ID não pode reutilizar um UID instalado, e outro UID não pode assumir o mesmo ID, inclusive quando há dados preservados. O autor declarado deve permanecer igual nas atualizações. Isso detecta trocas acidentais; **não autentica o autor**, pois o pacote permanece sem assinatura. O hash da revisão vincula a confirmação aos bytes escolhidos, sem provar sua origem.

## Dependências e versões

Versões têm exatamente três inteiros canônicos, comparados numericamente. O instalador valida o catálogo candidato inteiro: presença dos requisitos, limites mínimo/máximo, UID duplicado, dependências repetidas, autorreferência e ciclos. Atualizar ou restaurar uma biblioteca não pode quebrar aplicações dependentes. A remoção é recusada enquanto houver dependentes instalados.

As dependências verificam compatibilidade de instalação; não concedem acesso aos dados ou ao código privado de outra aplicação e não implementam carregamento automático de módulos de bibliotecas. UID/versão referem-se ao pacote instalado, não ao firmware.

Um upgrade mantém ID, UID e autor declarado. Downgrade requer uma segunda confirmação na interface e ainda precisa satisfazer dependências e contrato de dados. Reinstalar a versão já ativa é recusado. Uma versão imutável já retida só é reutilizada quando o manifesto e todos os arquivos coincidem; não é sobrescrita com conteúdo diferente.

## Dados e migração

Os dados de cada aplicação ficam em `/SymbianOS/data/<id>/`. Antes de ativar uma atualização, o instalador faz uma cópia desses dados, confere os arquivos e aplica a migração **na cópia**. A árvore aceita até 64 MiB, 256 arquivos, 512 entradas e profundidade de 8 diretórios, respeitando também os limites globais de caminho e quota. Antes da troca, o sistema verifica se os caminhos cabem tanto no staging quanto no backup. O espaço físico e o pico estimado da transação são conferidos; as operações do Storage continuam verificando espaço a cada escrita.

Se o esquema muda, o pacote precisa declarar uma migração cujo `from` coincide com o esquema atual. Não se executa Lua durante a instalação. O gerador produz `migration.tsv` com até 32 operações:

- `rename`: renomeia um arquivo próprio, de nome plano. Origem ausente é ignorada; destino existente causa recusa. Diretórios e caminhos externos não são aceitos.
- `defaults`: cria um arquivo próprio apenas se ele ainda não existe. Cada valor é texto UTF-8 de até 4 KiB, codificado no arquivo de comandos; não substitui dados existentes.

Exemplo de autoria:

```json
"schema": 2,
"migration": {
  "from": 1,
  "rename": [["counter.txt", "counter-v2.txt"]],
  "defaults": {"note.txt": "migração concluída"}
}
```

Falhas na cópia ou migração preservam a versão e os dados ativos. Essa linguagem não oferece conversões arbitrárias de bancos de dados nem migrações destrutivas; aplicações que precisarem delas exigem um novo contrato. Reinstalar após uma remoção com dados preservados consulta o UID/esquema guardado pelo sistema, evitando abrir dados de outro pacote ou ignorar uma mudança de esquema.

## Primeira abertura e rollback

Após instalar uma versão do perfil 2 ou atualizar uma aplicação SIS, a interface mostra **Abrir / validar** e **Cancelar tentativa pendente**. Apenas uma tentativa pode ficar pendente de cada vez. Outras alterações de pacotes aguardam validação ou cancelamento. A versão anterior e sua árvore de dados permanecem disponíveis.

Antes de executar a nova versão, o runtime persiste o início da tentativa. A confirmação ocorre depois que o código principal e o primeiro evento de foco retornam uma view válida dentro dos limites da sandbox. Erro de Lua, falha de carregamento ou interrupção com reinício durante essa primeira abertura restaura a versão e os dados anteriores. Uma instalação nova sem versão anterior é desativada, preservando os dados que existiam antes da tentativa. Isso não prova estabilidade prolongada: um erro posterior à primeira abertura não provoca rollback automático.

Após a confirmação, o sistema conserva a versão imediatamente anterior e o snapshot dos dados de antes da atualização, com inventário SHA-256. **Restaurar** verifica esse inventário, valida novamente as dependências e restaura ambos. Alterações posteriores ao snapshot não entram nos dados restaurados; essa consequência é mostrada na confirmação. A versão que saiu passa a ser a anterior, permitindo reverter a escolha. Só duas versões imutáveis e um snapshot anterior são retidos após a finalização.

A cópia cobre somente os dados privados da aplicação. Arquivos exportados para Documents, fotos ou outros recursos compartilhados não são desfeitos por rollback. As permissões e ações da aplicação continuam sob a sandbox e a confirmação do usuário.

## Desinstalação

A opção padrão remove os arquivos da aplicação e seus backups de versão, preservando os dados privados e um marcador de UID/esquema. **Apagar app e dados** remove também esses dados e o marcador, após confirmação separada. Documentos exportados em `/home/Documents/` são preservados nas duas opções. Dependentes precisam ser removidos primeiro.

A desativação no catálogo é o ponto de commit da remoção. Se a limpeza for interrompida depois disso, o aplicativo já não aparece como instalado e a recuperação continua a limpeza no próximo acesso/boot. Antes do commit, a aplicação permanece ativa. Uma tentativa pendente precisa ser validada ou cancelada antes da remoção.

## Persistência e recuperação

Além do journal anterior, preservado para compatibilidade, o ciclo de vida usa `/meta/sis-operation.txt`, staging em `/tmp/install/sis-lifecycle/`, snapshots/inventários em `/meta/sis/<id>/` e marcadores em `/meta/sis-data/`. Tudo fica exclusivamente no cartão, dentro da quota global de 4.000.000.000 bytes. Nenhum fluxo usa NVS ou instala firmware.

O journal tem checksum e vincula as versões, manifests e catálogos antigo/novo. A recuperação distingue preparação, tentativa, execução iniciada, restauração, finalização e limpeza. Trocas de dados precedem a ativação atômica do catálogo; uma interrupção anterior restaura a árvore antiga. A finalização é retomável inclusive se o snapshot já mudou de pasta ou seu ponteiro já foi persistido. Cada mutação é vinculada à geração do cartão sob o mutex do Storage.

A interface web por token só transfere arquivos; instalação, downgrade, rollback e remoção exigem ações no instalador. Atualizações do runtime continuam exclusivamente pelo launcher.

Os testes de host injetam falhas entre operações persistidas. Isso não simula perfeitamente FAT, cache de cartão e corte real de energia. Os testes físicos estão em [ROTEIRO_FISICO_SIS.md](ROTEIRO_FISICO_SIS.md).

## Validação desta entrega

- Host: 37/37 testes e smoke da interface passaram (`build/sis-release-validation.log`). Incluem dependências e ciclos, identidade e permissões, migração e conflitos, primeira abertura com falha, recuperação após reinício, downgrade, rollback, dados retidos, remoção e interrupções em atualização/finalização, além de SDK determinístico e exclusão de arquivos privados alheios ao manifesto do kit.
- ESP32-P4: build concluído (`build/sis-final-esp32p4.log`).
- BIN: 3343728 bytes; SHA-256 `479c5bba984ff5d6312538e420605e5ec9bfe0b9c6cd270b479ef08f505788fa`.
- Entrega: `sist/0.19.0/launcher/`, `sist/0.19.0/Tab5-SIS-SDK.zip` e `sist/0.19.0/Tab5-SIS-test-kit.zip` com 20 cenários.
- Não foram executados flash, serial ou testes físicos. O roteiro deve ser preenchido com resultados reais no Tab5.
