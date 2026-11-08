# Residente 0.6.1 — inicialização

O relato de tela branca, breve tela azul e reinício não foi reproduzido em hardware. O registro recebido (`runtime=0.5.0`, `action=back`, `stage=lua-complete`) é anterior à falha e não permite determinar sua causa.

A revisão encontrou e corrigiu caminhos concretos de risco na inicialização:

- O áudio de prioridade 5 aguardava 5 ms com `std::this_thread::sleep_for`. Na biblioteca ligada ao ESP, isso chama `usleep`; no ESP-IDF usado, esperas inferiores ao tick de 10 ms são espera ocupada. Agora as esperas dos workers usam `vTaskDelay` com arredondamento para cima e mínimo de um tick. Os antigos `yield` durante áudio também passam a bloquear por um tick, permitindo execução das tarefas de prioridade inferior.
- O worker da câmera chamava `closeCamera` a cada ciclo ocioso, enquanto o programa principal ainda podia configurar os callbacks. Agora só fecha após ter iniciado uma operação de captura, eliminando o acesso ocioso durante a configuração.
- Os objetos persistentes Storage/Runtime/UI passaram da pilha principal para o heap. A capacidade configurada da pilha é mantida; a mudança reduz sua ocupação permanente durante boot, validação de arquivos e criação da UI.
- Novos registros `/SymbianOS/logs/startup.txt` e `startup-stage.txt` indicam etapa, versão e, no primeiro, motivo do reset, memória interna e folga da pilha principal. São pistas, não substituem backtrace.

O residente **0.6.1 usa a mesma release SD 0.6.0/API 5**. Se o SD 0.6.0 já está completo, atualize somente `SymbianOS-runtime.bin` pelo OTA do launcher. O ZIP de atualização 0.6.0 continua correto se o SD ainda não foi atualizado. Preserve documentos e preferências.

Testes de host e build ESP32-P4 são registrados separadamente. A correção do reinício físico só pode ser confirmada no Tab5; não foi executado flash, monitor ou acesso serial.

Validação concluída: cinco suites CTest, jornada gráfica e ASan/UBSan sem SDL passaram; build ESP32-P4 concluído. A desmontagem do binário mostra que o frame direto de `app_main` caiu de 7808 para 320 bytes (isso não mede o pico das chamadas). BIN: 1439024 bytes. Hashes e identidade com o binário compilado foram conferidos. [Host](host-validation-0.6.1.log), [sanitizadores](sanitizer-0.6.1.log), [build nativo](esp32p4-build-0.6.1.log).
