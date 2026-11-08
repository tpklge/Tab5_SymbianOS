# Residente 0.7.2 — exposição automática e prévia da câmera

Compatível com o mesmo SD 0.7.1/API 7. Se ele já está instalado, atualize somente o BIN pelo launcher.

O usuário confirmou imagem da câmera na 0.7.1, mas relatou escuridão e baixa frequência de atualização. Foram corrigidas duas condições concretas do software:

- `ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER` agora está habilitado no sdkconfig e nos defaults. O ESP Video passa a iniciar seu controlador com a configuração IPA padrão do SC202CS, ajustando exposição/ganho, balanço de branco e processamento de imagem. Os dados de calibração são parte do driver do sensor, não recursos da interface. A configuração gerada contém os parâmetros SC202CS de AWB e AGC. Não aplica um simples clareamento artificial sobre pixels escuros.
- A captura/prévia deixa de esperar 200 ms a cada quadro: o alvo passa a um quadro a cada 50 ms, até 20 fps. O renderer consulta a imagem nesse mesmo intervalo. Quando o backend já fornece 640×360, transfere o buffer em vez de redimensionar/copiar novamente todos os pixels.
- A cadência de gravação MJPEG fica separada: continua em até 5 fps, sem áudio. Só comprime/grava quando vence o prazo de 200 ms, sem compensar atrasos com rajadas de frames. Assim, a prévia não precisa esperar artificialmente o intervalo da gravação; compressão, SD e processamento ainda consomem tempo no worker e podem reduzir o resultado real.

O alvo de 20 fps não é uma medição na placa. Exposição longa com pouca luz, processamento/rotação no LVGL, SD e concorrência podem reduzir a frequência efetiva. O controlador pode precisar de alguns quadros para convergir ao abrir ou ao mudar a iluminação. Luminosidade, cor e fluidez precisam de confirmação física no Tab5.

Testes de host verificam que a prévia ultrapassa o antigo limite de 5 fps com backend simulado, que avança mais rápido que a gravação durante filmagem e que o AVI continua com cadência limitada. Os testes existentes de câmera, áudio, navegação, rede e armazenamento permanecem. ASan/UBSan cobre as suites sem SDL; o ISP real é compilado para ESP32-P4, não exercitado no host.

Nenhum flash, monitor ou acesso serial foi executado. O runtime mantém os recursos do SD 0.7.1, documentos e configurações. Veja [validação](BUILD.md) e [atualização pelo launcher](ATUALIZACAO_LAUNCHER.md).

## Confirmação do autor

Em 8 de novembro de 2026, após instalar o residente 0.7.2, o autor confirmou: “perfeito, tudo funcionando”. A confirmação é um relato de uso da unidade; fps, convergência e consumo não foram medidos de forma independente pelo agente.
