# HTTP, feeds e recuperação — 0.19.0

Entrega revisada em 12/11/2026. O usuário relatou resposta HTTP interrompida no navegador e feed parado em zero bytes, seguido de “services:10”, tela de recuperação e reinício. Não houve reprodução física pelo agente nem captura serial nesta tarefa. As correções abaixo tratam defeitos confirmados por inspeção e testes; o motivo exato do reset observado ainda depende de teste no Tab5/log do reset.

## Correções principais

1. **Corpo HTTP encerrado por fechamento da conexão.** Na função `esp_http_client_read` do SDK instalado, o laço comparava bytes recebidos com Content-Length mesmo quando este era desconhecido/negativo. O teste compila a função original e reproduz o descarte do corpo; com o ajuste, lê todos os bytes. Um overlay de build, com licença upstream preservada, corrige apenas esse caminho. EOF é aceito após FIN limpo e conclusão do parser; timeout/reset, tamanho declarado truncado e chunks incompletos continuam sendo erros. O SDK instalado não é editado. Alterações inesperadas no fonte do SDK fazem o patch falhar durante configuração.
2. **Esperas/repetição HTTP.** A espera genérica usa 30 segundos reais sem progresso, substituindo 1200 tentativas de 10 ms (aproximadamente 12 segundos quando a leitura retornava imediatamente). Há também prazo de 90 segundos por tentativa, mesmo se o servidor continuar enviando lentamente. O navegador repete uma resposta interrompida uma vez, descartando integralmente o prefixo anterior. O feed conserva sua repetição já existente. Resposta incompleta nunca é publicada sobre o cache válido.
3. **Arquivos de serviços e `services:10`.** A linha relatada era compatível com o `error` disparado ao ler os registros do serviço. Agora há uma leitura prévia dos arquivos necessários e validação dos campos usados, antes de alterar notificações/agendamentos. Arquivos ilegíveis, de formato errado, registros incompletos e timestamps não finitos geram um aviso e são preservados. Não viram silenciosamente uma lista vazia. A aplicação do perfil Offline, o serviço de rede e o processamento dos outros módulos continuam independentes desse aviso; a atualização do feed não depende de um tick de notificações bem-sucedido.
4. **Recuperação sem bloquear LVGL.** A tela de recuperação não executa o encerramento síncrono da rede na tarefa gráfica. Ela solicita cancelamento e usa o executor existente para encerrar serviços; a tarefa gráfica continua atendendo LVGL enquanto aguarda HTTP/DNS. Durante essa limpeza, a tentativa de reinício da aplicação aguarda. O teste smoke simula HTTP que leva mais 200 ms para terminar após cancelado e verifica que a entrada na recuperação não espera esse término na tarefa gráfica.
5. **Diagnóstico de leitura.** `sys.read` recebe o erro da própria leitura sob o mesmo mutex, em vez de consultar um erro global que outra tarefa pode substituir. Falta transitória de descritores/memória ao abrir arquivo não é tratada como evidência de cartão removido. Erros reais de dispositivo/I/O continuam invalidando o armazenamento.

## Ajustes de menor grau confirmados nesta revisão

A auditoria original recuperada do chat tinha oito itens altos e seis médios, sem uma lista separada de itens baixos. Nesta entrega foram corrigidos os seguintes problemas menores encontrados no fluxo relatado:

- Navegador atualiza bytes recebidos e fase da operação, em vez de manter somente “Carregando”.
- Feed distingue espera por conexão/cabeçalhos do recebimento do corpo. Zero bytes durante a abertura passa a ter contexto; não significa que conteúdo foi confirmado no SD.
- Uma repetição do navegador reinicializa progresso, URL final e buffer; não soma bytes de duas respostas.
- Content-Length inválido tem diagnóstico próprio, em vez da mensagem incorreta sobre Location. Resposta comprimida, apesar de `Accept-Encoding: identity`, é recusada explicitamente, sem alimentar HTML/XML com bytes comprimidos.
- Avisos de arquivo de serviço são deduplicados e desaparecem após uma leitura válida, sem apagar o arquivo problemático. Notificações carregadas têm campos/tamanhos/timestamps validados antes de usar as APIs nativas.
- A tela de recuperação informa API 14, em vez do texto antigo API 11.
- Download de feed verifica a geração do cartão durante a recepção e antes da publicação, preservando o cartão substituído.

## Validação

Resultado: **42/42 testes de host e smoke da interface passaram; build ESP32-P4 concluído.**

Logs: `build/http-services-host.log` e `build/http-services-p4.log`.

- `http_client_stream`: compila o laço real de leitura do SDK original e ajustado, com transporte/parser simulados; reproduz o defeito e verifica fechamento limpo, tamanho conhecido, timeout transitório, truncamento, chunk final ausente e reset. Não é uma conexão TLS física com o C6.
- `http_services`: arquivos danificados preservados, registro inválido/timestamp infinito, perfil Offline aplicado apesar de ledger danificado, erro de leitura isolado de outra tarefa, esgotamento temporário de descritores, repetição integral do navegador e prazos HTTP com relógio simulado.
- Smoke da interface: recuperação enquanto HTTP ainda encerra, reinserção/reabertura, teclado e PIN, além das verificações anteriores.
- Build ESP32-P4 usa o overlay real e mantém os limites de tamanho do binário. Não executa flash.

## Teste físico prioritário

1. Enviar **BIN e pasta SymbianOS juntos** pela WebGUI do launcher, em `sist/0.19.0/launcher/`; reiniciar e conectar ao Wi-Fi.
2. Abrir novamente o mesmo endereço que falhou no navegador. Conferir as fases de conexão/recepção e os bytes. Em falha transitória pode ocorrer uma repetição; depois deve abrir a página ou mostrar o motivo, sem publicar HTML parcial. Continua valendo o limite de 1 MiB e os limites de complexidade do renderizador.
3. Abrir Feeds e baixar a assinatura que falhou. Antes do primeiro byte deve mostrar espera por conexão/cabeçalhos; depois, os bytes recebidos. Cancelar uma tentativa e navegar entre os apps. Repetir o download e conferir que o cache anterior continua utilizável em caso de erro.
4. Se aparecer aviso de serviços, conferir qual arquivo foi indicado. A aplicação deve continuar aberta e o arquivo permanecer intacto. Não apagar registros para esconder o aviso; copiar os dados/logs antes de reparar um arquivo. O teste automatizado de corrupção usa uma cópia temporária do SD modelo.
5. Se houver novo reinício, registrar a mensagem completa/URL e copiar `/SymbianOS/logs/network.txt`, `/SymbianOS/logs/lua-fault.txt` e `/SymbianOS/logs/startup.txt`, quando presentes. O log de startup inclui o motivo do reset. Não é possível confirmar watchdog, panic ou falha física somente pelo relato de tela azul.

A transferência Web por token, o limite de upload de 64 MiB, SIS sem assinatura e a instalação pelo launcher permanecem. Não foi acrescentado OTA interno, NVS ou SISX. A distribuição mantém apenas 0.19.0 e 0.18.1 em `sist`.
