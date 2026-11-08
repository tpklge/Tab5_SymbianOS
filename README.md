# Tab5 SymbianOS

O nome **SymbianOS é apenas uma referência e homenagem ao velho, excelente e lendário sistema operacional da Nokia**. Bons tempos — ainda tenho um Nokia N95 funcional em casa! Este é um projeto independente, escrito do zero para o M5Stack Tab5, com interface inspirada no S60 5th Edition.

Plataforma original para M5Stack Tab5. O [planejamento](plano_sistema_s60_tab5.md) é o documento normativo. Drivers, Lua e LVGL compõem o residente; interface, aplicativos e recursos vêm do SD, sob `/SymbianOS` e quota decimal de 4 GB.

## Última versão: 0.8.0 / API 8

[Baixe a release 0.8.0](https://github.com/tpklge/Tab5_SymbianOS/releases/tag/v0.8.0).

| Arquivo | Uso |
|---|---|
| [SymbianOS-runtime.bin](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SymbianOS-runtime.bin) | Aplicação ESP32-P4 para OTA pelo launcher. |
| [SymbianOS-sdcard.zip](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SymbianOS-sdcard.zip) | Conteúdo completo do cartão para instalação inicial. |
| [SymbianOS-update-0.8.0.zip](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SymbianOS-update-0.8.0.zip) | Atualização do SD preservando documentos e configurações. |
| [SHA256SUMS](https://github.com/tpklge/Tab5_SymbianOS/releases/latest/download/SHA256SUMS) | Verificação dos arquivos publicados. |

**Atualize BIN e SD juntos** para 0.8.0. Faça backup da instalação existente e mantenha `/data`, `/home` e `/config`. [Procedimento pelo launcher](docs/ATUALIZACAO_LAUNCHER.md).

Rede agora tem **redes salvas, conexão automática, conectar sem salvar e esquecer rede**. Conecte e salve uma vez para que a rede seja reconhecida nas próximas inicializações. A transferência web usa token temporário de **8 caracteres**. [Uso e armazenamento das credenciais](docs/REDES_SALVAS_0_8.md).

Mantém os aplicativos Lua no SD, organizador, multimídia, câmera com exposição automática e alvo de prévia até 20 fps, vídeo MJPEG até 5 fps sem áudio, bateria/carga QC, volume, busca Wi-Fi, downloads e feeds. Inclui a correção de upload da 0.7.3; o autor confirmou arquivo enviado com sucesso. Também relatou câmera funcionando, reprodução de um set de DJ de 133 MB e carga QC chegando a 12 W nas revisões anteriores.

Oito suites de host, jornada gráfica e ASan/UBSan validam a lógica; build ESP32-P4 verifica compilação/tamanho. Reconexão automática e token curto desta versão ainda precisam de aceitação na unidade física. Relatos do autor não equivalem a medições exaustivas de fps, consumo ou recuperação de falhas.

## Compilar a versão publicada

Use ESP-IDF **5.4.4** para ESP32-P4; as versões dos componentes estão fixadas em `dependencies.lock`. Lua e o código original estão neste repositório; os componentes gerenciados são baixados pelo IDF na configuração. Para reproduzir a configuração da release:

```sh
cp sdkconfig.release sdkconfig
idf.py build
```

Execute isso em um terminal com o ambiente do ESP-IDF 5.4.4 ativado. O build gera `build/tab5symbian_runtime.bin`; não acessa a placa. Artefatos da instalação independente e avisos de licenças também estão na release. Para o OTA de aplicativo do launcher, use `SymbianOS-runtime.bin`.

## Desenvolvimento

```sh
./tools/idf.sh build
./tools/host.sh test
python3 tools/release.py
```

O primeiro comando compila para ESP32-P4. O segundo verifica a instalação, executa testes de núcleo/organizador e uma jornada gráfica headless. O terceiro gera artefatos locais; não faz upload nem acessa a placa. Consulte [validação e tamanhos](docs/BUILD.md).

O host requer CMake, pkg-config, SDL2 e OpenSSL. O helper seleciona o SDK macOS 15.4 com deployment target 26.0 para contornar a incompatibilidade local SDK/linker. Os helpers aceitam o ambiente do ESP-IDF instalado; `TAB5_IDF_PATH` e `TAB5_IDF_PYTHON_ENV` permitem ajustar os caminhos.

Para executar uma cópia de trabalho:

```sh
mkdir -p build/simulator-data
cp -R sdcard/SymbianOS build/simulator-data/
./tools/host.sh run build/simulator-data/SymbianOS
```

Ctrl+S salva; Ctrl+A/C/X/V edita; Shift+setas seleciona; Tab/Shift+Tab move foco; Enter ativa; Esc volta ou cancela uma operação em andamento; Ctrl+Esc abre Menu; Ctrl+Tab abre o alternador. O teclado virtual permite entrada por toque. No menu clássico, primeiro toque seleciona e o seguinte abre, sem prazo de duplo clique.

## Artefatos

- `dist/SymbianOS-runtime.bin`: somente a aplicação, para OTA pelo launcher.
- `dist/SymbianOS-update-0.8.0.zip`: nova release/manifesto/diretórios, sem documentos ou configurações para sobrescrever os existentes.
- `dist/SymbianOS-SDK-0.8.0.zip` e `dist/packages/`: SDK e exemplos de app/tema.
- `dist/launcher/`: os mesmos arquivos organizados para envio manual pela WebUI.
- `dist/SymbianOS-sdcard.zip`: árvore completa para instalação inicial.
- `dist/runtime-installation/`: partes de uma instalação independente; não enviar pelo OTA de aplicativo do launcher.
- `dist/SHA256SUMS` e `dist/release.json`: hashes e identificação dos artefatos.

Scripts/tema podem ser alterados no SD sem recompilar o residente enquanto sua API for compatível. Execute `python3 tools/package_sd.py --zip dist/SymbianOS-sdcard.zip` após editar a release para regenerar hashes. Faça a troca com o sistema parado e preserve os dados.

[API](docs/API.md), [recuperação](docs/RECUPERACAO.md), [bancada física](docs/TESTES_FISICOS.md) e [licenças](docs/THIRD_PARTY.md). Sem SIS/SISX, OTA próprio ou deep sleep com despertar por alarme nesta entrega. Nenhum flash, monitor, erase-flash ou acesso serial foi executado pelo agente.

## Licença

Código original sob [MIT](LICENSE); bibliotecas, fontes e demais dependências mantêm suas próprias licenças, descritas em [THIRD_PARTY](docs/THIRD_PARTY.md). O texto GPL que existia sozinho no commit inicial do repositório foi preservado em [docs/legacy](docs/legacy/GPL-3.0-initial-repository.txt) como registro histórico.
