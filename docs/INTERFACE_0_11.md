# Interface e rede — 0.11.0 / API 11

Ajustes substitui Energia e volume. O acesso ao PIN fica em **Ajustes → Bloqueio por PIN**: quatro dígitos, confirmação e persistência local. Tela e Som usam barras deslizantes; OK salva, Cancelar/Voltar descarta a prévia. Música e Gravador também têm volume por barra, com aplicação imediata.

Home, Menu e Abertos usam ícones. Confirmação, cancelamento e retorno ficam no rodapé. A engrenagem aparece em aplicativos com configuração implementada, sem o antigo botão global Opções. Carga e Perfis usam listas não editáveis; o perfil aplicado aparece na barra superior. Suspensão apresenta ícone e duração.

Wi-Fi, busca de redes e transferência atualizam o estado automaticamente, em campos separados e não editáveis. Formulários de credenciais não são atualizados enquanto o usuário escreve. Galeria usa lupas +/−. Arquivos, contatos, agenda, alarmes, backup e instalador recebem ações com ícones; alarmes incluem relógio digital lateral.

Memória mostra heap interno e PSRAM reais quando disponíveis, incluindo livre, maior bloco e mínimo livre interno, além da memória Lua. No simulador os dados físicos indisponíveis são identificados. Abertos usa linhas compactas, ícone, memória Lua da VM e X para fechar: essa memória não representa todo o custo nativo compartilhado de um aplicativo.

## RSS e HTTP/TLS

A leitura distingue espera temporária de conexão encerrada. Há até 12 segundos sem progresso para retomada, com cancelamento, sem repetir bytes já entregues. Content-Length deve ser integralmente recebido; chunked exige o terminador. Resposta sem tamanho aceita fechamento limpo. Feeds precisam ter fechamento RSS/Atom antes de serem aceitos. Diagnósticos registram bytes, tamanho esperado, chunked, retorno e errno/TLS em logs/network.txt. Não se aceita um feed truncado para ocultar erro de rede.

## Validação

14 suites CTest passaram em host e sob ASan/UBSan; os testes JavaScript executam fora da instrumentação do motor. Jornada gráfica testa sliders, descarte da prévia, atualização automática do Wi-Fi, fechamento por X, PIN e navegação. Build ESP32-P4 concluído; nenhuma medição física foi realizada nesta entrega. RSS com os servidores afetados, toque, áudio e energia precisam de aceitação no Tab5. OTA/atualização própria continuam aguardando validação física. BIN e SD API 11 devem ser atualizados juntos.
