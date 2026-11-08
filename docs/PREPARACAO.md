# Ambiente de desenvolvimento

O scaffold inicial foi substituído pela implementação descrita em `FASES_0_1.md`.

- `main/`: entrada ESP32-P4, adaptação do teclado I²C e probes nativos.
- `components/platform/`: armazenamento, quota, manifesto, Lua, modelo de entrada e renderer genérico compartilhados.
- `components/lua/`: fonte oficial Lua 5.4.9 e licença preservada.
- `sdcard/SymbianOS/`: instalação externa real, scripts e fonte OFL; nenhum desses arquivos é embutido no firmware.
- `simulator/`: adaptação SDL2, CMake e configuração desktop do LVGL.
- `tests/`: casos de armazenamento, sandbox, integridade e entrada em host.
- `tools/`: build sem acesso à placa, teste de host e empacotamento/verificação do SD.
- `references/`: demos oficiais para consulta, não componentes desta aplicação.

ESP-IDF 5.4.4 e Python 3.9 existentes são reutilizados por `tools/idf.sh`. SDL2/OpenSSL/CMake/pkg-config já estão instalados no host. O SDK macOS 27 local falha com o linker disponível; o helper desktop seleciona SDK 15.4 e deployment target 26.0, compatível com as bibliotecas Homebrew presentes nesta máquina. Essa configuração foi testada localmente; não representa uma matriz de suporte de todos os macOS.

O formatador opcional `clang-format` 20.1.8 foi instalado somente em `.venv-dev/`. Sua versão está em `tools/requirements-dev.txt`. Não é necessário ao firmware.

Versões/commits de referências e hash da fonte estão em `dependencies.json`. Dependências de componentes ficam fixadas em `../dependencies.lock`. Os custos do residente e as limitações de medição estão em `BUILD.md`.

## Atualização 0.3.0

A implementação atual está em `FASES_2_3.md`; o renderer usa paisagem e tema escuro. `api.cpp` expõe operações mediadas, `files.cpp` implementa streaming/backup/journal e `timers.cpp` fornece o agendador nativo independente do lock de arquivos. `main/services.cpp` adapta sinalização e RTC; o transporte de rede segue pendente.

O formatador Lua opcional StyLua 2.6.0 foi baixado para `.tools/bin/`, com SHA-256 do asset oficial conferido. Não é embarcado. `tools/release.py` prepara BIN/ZIPs/árvore de launcher sem acessar dispositivos e recusa fontes nativas mais novas que o BIN. Os novos diretórios de dados são preparados pelo runtime somente após validar a instalação existente.
