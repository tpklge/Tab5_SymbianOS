> Registro histórico da release 0.1.0. Para a versão atual, consulte [Fases 2/3](FASES_2_3.md) e [Build](BUILD.md).

# Fases 0 e 1 — implementação e aceitação

## Entregas em código

| Área | Implementação | Evidência disponível |
|---|---|---|
| Runtime | ESP-IDF, Lua 5.4.9 e LVGL 9.5.0; firmware sem shell/apps/assets do ambiente | Build ESP32-P4 e inspeção dos artefatos |
| Boot | Manifesto formato 1, API 1, esquema 1, arquivos essenciais e SHA-256; texto Lua somente | Host: ausência, corrupção, API incompatível, bytecode recusado |
| Armazenamento | Raiz física fixa `/sdcard/SymbianOS`; caminhos lógicos; recusa de traversal, aliases, links no host e nomes FAT inválidos | Host: arquivos externos preservados, symlinks e aliases recusados |
| Quota | Contadores de 64 bits, teto decimal 4 GB, margem de 64 MiB, auditoria no boot e por transação, serialização de escritores e pico de temporário | Host: concorrência, substituição, manutenção e arquivo esparso de 4.000.000.001 bytes |
| Gravação | Temporário sincronizado, cópia anterior e recuperação no boot; falhas não geram fallback de persistência | Host: reinício e transação interrompida |
| Scripts | Estado por app, alocador de 1 MiB, hook de 100.000 instruções, módulos verificados, API por diretório do app | Host: memória, loop infinito, permissões e retorno ao shell |
| UI | Renderer genérico residente; telas/conteúdo/tema/fonte no SD; operações Lua/arquivos em worker, LVGL em seu proprietário | Simulador headless, screenshot e edição por entrada real do LVGL |
| Teclado oficial | I²C Normal na Ext.Port1: bus 1, SDA 0, SCL 1, endereço 0x6D; press/release, reconexão, mapa externo, repetição e modificadores | Matriz e modificadores testados em host; driver compilado, sem evidência física |
| Recuperação | Diagnóstico mínimo sem SD/sistema válido, repetição de boot; remoção invalida o backend; reinserção exige nova validação | Ausência e boot em host; remoção/reentrada simuladas na UI |
| Ambiente | Home, grade de menu clássico, navegação comum, alternador com estados realmente abertos, Notas e Calculadora externos | Jornada de host preserva estado e documento; mudança de tema sem rebuild |
| Entrada | Touch, QWERTY virtual com Shift/acentos/números/símbolos, foco e seleção UTF-8, atalhos e clipboard interno | Smoke gráfico por toque e teclado; teste físico pendente |
| Distribuição | ZIP `SymbianOS`, verificador, binário separado, hashes, contratos e guia de recuperação | Verificação local dos artefatos |

## Limites deste marco

- Retrato 720×1280. Paisagem adaptativa e IMU não são anunciados como prontos.
- Notas edita um documento, com até 16.000 caracteres no campo e 64 KiB na API. Documento ilegível/maior que o limite não é sobrescrito silenciosamente.
- Calculadora interpreta números, sinais, parênteses e +/−/*/÷ com precedência. Entradas inválidas e divisão por zero mostram erro recuperável, sem executar `load`/eval.
- Alternador preserva estados em RAM; fechar/descarregar apps, serviços de fundo e restauração de todas as vistas após reboot são da fase 2.
- USB HID não foi integrado; o driver obrigatório é o I²C. O teclado desktop é entrada do simulador, não comprovação do acessório real.
- Rede/C6 não está integrada. Áudio e RTC têm probes I²C de diagnóstico; não foram reproduzidos/gravados sons nem ajustado o RTC. Não inferir compatibilidade funcional por ACK.
- A tela inicial informa quando o relógio do firmware não foi ajustado. Não mostra bateria, sinal de rede ou porcentagens fictícias.
- Há uma release; rollback do SD, confirmação de saúde, OTA e assinaturas são posteriores. Hash não é autenticação.
- Manutenção acima da quota orienta intervenção no computador; o backend suporta exclusão, mas ainda não há gerenciador residente completo de arquivos.
- Auditoria limitada a 20.000 entradas, 10.000 arquivos e 16 níveis; arquivos de código até 64 KiB, manifesto até 32 KiB e assets individuais até 1 MiB. Instalações fora desses limites são recusadas.

## Critérios físicos ainda abertos

Identificar revisão do SoC e painel/touch; testar quatro cantos e orientação; demonstrar texto, acentos, modificadores, seleção e reconexão no teclado oficial; reiniciar e recuperar documento no SD; testar cartão ausente, removido, cheio e somente leitura; ensaiar cortes de energia com cópias de teste; medir memória, maior bloco livre, latência e tamanho por capacidade residente; executar ensaios funcionais de áudio, RTC e transporte C6 antes de prometer esses serviços.

O software permite iniciar essa bancada, mas as fases 0/1 não estão aprovadas fisicamente. Não houve autorização nem execução de flash, monitor, erase-flash ou abertura de serial.
