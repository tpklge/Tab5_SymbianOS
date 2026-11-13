# Lista SIS e envio web por token

O instalador mantém os controles Anterior, Próxima, Atualizar e Tema padrão em uma coluna fixa de 180 px à direita. A lista ocupa a área à esquerda, sem sobreposição. O estilo dos botões das aplicações instaladas foi preservado.

A lista reúne pacotes de `/home/Transfer`, `/update/packages` e aplicações instaladas em uma sequência de páginas de até 40 itens, além dos controles de tentativa pendente. A filtragem de `.sis` e `.t5pkg` ocorre antes da paginação, sem distinguir maiúsculas/minúsculas. Pastas com extensão `.sis` e arquivos de outros tipos não consomem vagas. Os índices da API e do instalador agora coincidem, evitando repetições entre páginas.

A rolagem aceita toque com inércia e barra visível. Setas percorrem as linhas; PgUp/PgDn avançam uma área de leitura; Home/End vão ao início/fim da página. Tab continua permitindo chegar aos controles laterais. Ao voltar da revisão, a página conserva posição e foco. Atualizar relê os arquivos após novos envios. Os pacotes incompatíveis continuam sem botão Instalar, como previsto no contrato.

O antigo Compartilhar agora aparece como **Enviar arquivos**. É a interface web do SymbianOS para transferência por token, não OTA. O servidor usa a porta HTTP **8080**, com porta de controle separada da configuração padrão. A interface exibe o endereço completo `http://<IP>:8080` e o token, mantendo o IP da conexão em um campo independente. Desconexão invalida o endereço/token exibidos; recuperação de rede pode reativar o envio com novo token.

Uma falha ao abrir o servidor não é mais apresentada genericamente como falta de Wi-Fi quando a conexão existe. O backend fornece o erro da inicialização/registro HTTP, incluindo o nome ESP_ERR. Isso permite distinguir falha de rede de falha do serviço. A porta foi separada para evitar o compartilhamento dos padrões HTTP/controle com outros servidores; não houve medição física do problema original nesta tarefa.

Enviar os SIS pela WebGUI do launcher continua válido, desde que os arquivos cheguem às pastas aceitas pelo instalador. Atualizações do BIN e dos recursos do sistema permanecem exclusivamente no launcher.

O botão do PIN exibe **OK**; a indicação de Enter continua no título. Isso evita truncamento do texto no botão. Nenhuma regra de autenticação foi alterada.

## Validação

38/38 testes de host e smoke da interface passaram (`build/installer-polish-final-host.log`). A cobertura nova verifica filtragem e paginação de 65 SIS, extensão em maiúsculas, ausência de duplicatas, diagnóstico de envio com Wi-Fi conectado, endereço de transferência separado do IP, geometria da coluna lateral, Page Down e preservação da rolagem. O smoke também confirmou PIN por matriz física simulada/Enter e por toque. Build ESP32-P4 concluído (`build/installer-polish-final-esp32p4.log`).

Relato físico do usuário anterior a esta alteração: os SIS se comportaram conforme esperado e um pacote incompatível não ofereceu Instalar. Não foram fornecidos resultados individuais para todos os casos nem para cortes de energia. A nova coluna, rolagem e o servidor na porta 8080 aguardam teste no Tab5. Nenhuma serial ou gravação física foi executada nesta tarefa.
