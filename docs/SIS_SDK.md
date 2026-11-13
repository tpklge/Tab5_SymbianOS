# SDK SIS sem assinatura

O SDK distribui ferramentas e exemplos originais MIT para gerar pacotes de aplicações Lua do Tab5Symbian. Não contém ferramentas, executáveis ou recursos proprietários do Symbian/Nokia. Não assina pacotes e não produz SISX.

## Gerar uma aplicação

Requer Python 3 no computador. As ferramentas de geração usam apenas a biblioteca padrão. O manifesto `sis.json`, `main.lua` e `LICENSE.txt` ficam na pasta da aplicação. O exemplo básico está em `sdk/sis-hello/`; o kit contém fontes de bibliotecas, contador, atualização, permissões e migração.

```sh
python3 -B tools/make_sis.py sdk/sis-hello sist/0.19.0/packages/hello.sis
```

A saída é um SIS binário e um relatório JSON com UID, versão, arquivos, permissões e SHA-256. A geração é determinística para a mesma entrada. Não renomeie um ZIP para `.sis`.

Dentro de `Tab5-SIS-SDK.zip`, os exemplos ficam em `examples/`. Nesse caso use:

```sh
python3 -B tools/make_sis.py examples/02-counter-1.0.0 counter.sis
```

O gerador não interpreta arquivos PKG legados. Aceita os campos documentados no [perfil inicial](PERFIL_SIS_1.md) e no [ciclo de vida](SIS_CICLO_DE_VIDA.md); campos desconhecidos são recusados. Dependências devem ser distribuídas como pacotes separados e instaladas em ordem. Não há download automático, execução de scripts de instalação ou loja remota.

## Inspeção independente

No checkout completo do projeto, o inspetor C++ usa o parser nativo, independentemente do gerador Python:

```sh
./tools/host.sh build
build/host/sis_inspect counter.sis
```

A inspeção mostra metadados e hashes sem executar a aplicação nem alterar o pacote original. O SDK inclui o código do inspetor para referência, mas sua compilação depende do checkout e das dependências de host do projeto. O Tab5 também faz a inspeção completa antes de confirmar uma instalação.

## Gerar a distribuição de desenvolvimento e teste

```sh
python3 -B tools/release_sis.py
```

Saídas:

- `sist/0.19.0/Tab5-SIS-SDK.zip`: gerador, fontes dos exemplos, contrato, documentação e roteiro físico.
- `sist/0.19.0/Tab5-SIS-test-kit.zip` e `sist/0.19.0/sis-test-kit/`: fontes e 20 cenários SIS, incluindo falhas intencionais.
- `sist/0.19.0/SIS-SHA256SUMS`: hashes dos dois ZIPs; o kit possui também hashes individuais dos SIS.

Os ZIPs organizam a distribuição no computador. Os arquivos enviados para o instalador são os `.sis` dentro de `packages/`, e não esses ZIPs.

## Enviar e instalar

1. No SymbianOS, abrir Rede e ativar a interface de transferência.
2. Usar o endereço e token exibidos para enviar o SIS para `home/Transfer`.
3. No Tab5, abrir Instalador, revisar origem não autenticada, dependências, versão, dados e permissões; confirmar por toque ou teclado.
4. Abrir a aplicação para validar a primeira execução. Para atualizações, conferir os dados antes de prosseguir com outros pacotes.

A interface de transferência não instala automaticamente, não autentica o autor do pacote e não grava firmware. Instale o BIN e os recursos do próprio sistema somente pela WebGUI do launcher.

## Contrato para autores

Mantenha ID, UID e autor declarado entre versões; incremente a versão numérica. Declare todas as dependências e evite ciclos. Não aumente o esquema de dados sem migração compatível. Use somente permissões necessárias. Não coloque segredos, credenciais, arquivos pessoais ou bytecode na distribuição. Preserve as licenças de todos os componentes; o perfil atual dos exemplos é MIT.

Teste com o roteiro e registre hardware, cartão, versão, hashes e resultados. Só declarar compatibilidade física após executar os testes no Tab5. Assinaturas, certificados e SISX permanecem para a última fase.
