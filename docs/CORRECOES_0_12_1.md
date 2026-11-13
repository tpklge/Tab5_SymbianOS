# Correções — 0.12.1 / API 12

## RAK / Grove

O botão Conectar inicia a conexão UART diretamente, sem uma segunda confirmação. Ligação e contadores estão em Diagnóstico. A UART aberta não equivale a rádio identificado: o app só fica pronto após a resposta Meshtastic de sincronização.

O código anterior drenava 512 bytes por chamada, pelo tick do serviço, e possuía RX de 2 KiB. Um banco de nós pode chegar em rajada antes desse tick. A recepção agora tem tarefa própria a cada 10 ms, RX de 4 KiB, parser incremental com ressincronização e até 256 quadros em PSRAM. O serviço Lua continua consumindo/protocolando esses quadros, mantendo a interface e a persistência no cartão. A fila é limitada e contabiliza perda; não sobrescreve quadros silenciosamente. O estado Diagnóstico mostra TX/RX, quadros válidos/inválidos, ocupação e perdas, sem registrar conteúdo de mensagens/chaves. O usuário confirmou que rádio e cabo funcionam no Tab5-Meshtastic; nenhuma alimentação/pinagem foi alterada.

## Memória e RSS

As fotos do usuário mostram 121,6 KiB livres internamente, mas maior bloco de 31,5 KiB, com mais de 26 MiB livres em PSRAM. Total livre não garante uma alocação contígua. O limite de malloc que prefere RAM interna passou de 16 KiB para 1 KiB; buffers de Wi-Fi/lwIP priorizam PSRAM. TLS já utilizava alocação externa e continua com validação de certificados e CAs completas. RX HTTP passou a 4 KiB e TX a 1 KiB. Pilhas do executor Lua/UI e rede passaram respectivamente de 64/48 KiB para 48/32 KiB; áudio/câmera e pilhas internas dos drivers foram preservados. A recepção UART tem pilha interna de 4 KiB. [Política de alocação documentada pela Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32p4/api-guides/external-ram.html).

Isso reduz a pressão sobre RAM interna; o aumento real de heap/maior bloco e a conexão TLS precisam ser medidos no Tab5. A espera de corpo HTTP de 30 segundos, validação de framing e rejeição de feed incompleto permanecem. O erro de conexão registra errno, heap livre/maior bloco, PSRAM e mínimo livre da pilha de rede. Feeds oferece Detalhes da falha em leitor rolável e não conserva Baixando após a falha. Nenhuma verificação TLS foi desabilitada.

## Interface e teclado

Ícone Wi-Fi vetorial antes do relógio: níveis por RSSI, sem conexão marcado por X, atualizado automaticamente. A consulta de RSSI ocorre no executor de serviços a cada cinco segundos quando a rede está livre, nunca na thread LVGL; a tela de PIN adia essa consulta. Renderização lê somente o snapshot.

Menu: grade de quatro colunas/12 apps por página; lista de duas colunas/oito apps por página. Home e Abertos preservam seus próprios layouts. O campo opcional columns (1–4) é uma propriedade genérica da view; a release continua API 12.

PIN: teclas físicas e toques durante uma ação são enfileirados com dono da tela. A entrada de dígitos não grava breadcrumbs nem roda todos os serviços a cada tecla; o teclado touch e o tema são mantidos e só máscara/título mudam. Nenhum PIN é escrito em logs. Backspace/Delete e Enter funcionam; repetição de tecla não preenche o PIN involuntariamente. Limite de tentativas, verificador salgado, confirmação e preservação de rascunhos foram mantidos.

## Evidência e aceitação

16 suites passaram em host e ASan/UBSan. A nova suite injeta 200 quadros em rajada antes de qualquer consumo pela UI, além de fragmentação byte a byte, ruído, comprimento inválido e fila cheia; também cobre os limiares Wi-Fi. Testes de integração verificam RSSI em cache, adiamento no PIN, mensagem completa de falha e menu paginado. A jornada gráfica usa a matriz real de eventos do teclado para 1234/Enter sem esperar entre teclas, depois testa toques rápidos e identidade dos widgets do teclado. Testes de host não abrem UART, BLE nem rede física. As prévias de sinal/bateria usam dados sintéticos identificados no simulador.

Build ESP32-P4 separado dos testes. Ainda validar RAK/cabo, rajada real, perda/reconexão, sinal Wi-Fi, PIN físico/toque, heap/stacks e RSS do Gizmodo na placa. Atualizar BIN e SD juntos. OTA própria continua aguardando aceitação física; nenhuma gravação, serial, monitor ou publicação foi executada pelo agente.
