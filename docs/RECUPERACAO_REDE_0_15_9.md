# DNS e recuperação de tráfego — 0.15.9 / API 14

## Diagnóstico atualizado

A foto da 0.15.8 registra EAI_FAIL (202 no lwIP instalado), antes de TCP/TLS. O usuário confirmou que, nesse estado, a página OTA também deixa de abrir. Isso aponta para uma falha mais ampla no caminho de rede, e não permite continuar atribuindo tudo ao servidor IMAP. Não identifica definitivamente a camada responsável: C6, transporte esp-hosted, associação, DHCP, DNS ou serviço HTTP.

Não foram trocadas senhas, configurados DNS públicos nem atualizado firmware C6. O diagnóstico passa a registrar IP, gateway e DNS0/DNS1/DNS2, junto ao estado/protocolo do C6. O arquivo wifi-transport.txt diferencia fase anterior à conexão de conexão concluída, evitando interpretar o IP/DNS ainda não obtido como resultado final.

## Recuperação

O serviço de rede agora pode recuperar a última conexão, usando credenciais da sessão em RAM, inclusive para redes não salvas. O trabalho acontece no executor de rede: desconecta/para a estação Wi-Fi, encerra o servidor OTA se estava ativo, reconecta e reinicia o serviço OTA. O IP pode mudar e o token OTA pode ser renovado. Isso reinicia conexões de rede; o usuário pode precisar atualizar a página e informar o token atual.

Depois de duas falhas de conexão/DNS anteriores à autenticação durante recebimento, o e-mail pode agendar essa recuperação. É uma heurística de falha de transporte, não prova automática de que toda a rede parou. Não repete LOGIN recusado, falha de certificado nem envio SMTP incerto. Não reinicia e-mail automaticamente depois da recuperação: aguarde o estado conectado e toque em Receber.

A recuperação automática é limitada a uma vez por minuto e não começa durante operação de rede ou transferência de arquivos ativa. Uma reserva atômica impede que novas transferências iniciem durante a recuperação, fechando a corrida entre checar transferência e reiniciar a rede. Desconexão manual e perfil offline são respeitados.

A recuperação manual está disponível em **E-mail → Ajustes → Diagnóstico → Reconectar Wi-Fi**, e também na tela principal de Wi-Fi. Pode interromper conexões em andamento. Se uma operação do serviço de rede estiver ocupada, cancele-a e aguarde antes de pedir a reconexão. Não reinicia o Tab5 nem regrava o C6.

## Outras correções

- Registro e tratamento de IP_EVENT_STA_LOST_IP; estado conectado também exige interface ativa e IPv4 válido.
- DNS EAI_FAIL/EAI_AGAIN recebe uma repetição pré-autenticação, dentro do limite existente, antes de considerar recuperação.
- Diagnóstico do e-mail exibe o estado atual do serviço de rede, independentemente do erro histórico.
- A rede só deixa de indicar busy depois de marcar o trabalho concluído, evitando tentativa de reutilizar executor ainda ocupado logo após o resultado.
- Reinício do serviço HTTP remove sockets antigos ligados à conexão anterior; token e estado OTA exibidos são atualizados.

## Registros

- `/SymbianOS/logs/mail.txt`: falha e eventual recuperação agendada.
- `/SymbianOS/logs/wifi-transport.txt`: protocolo, firmware, IP, gateway e servidores DNS.
- `/SymbianOS/logs/network-recovery.txt`: resultado da recuperação e restauração da OTA.

Nenhum registro inclui senha da rede ou da conta.

## Validação e limites

18 suítes host e jornada LVGL. Testes de rede simulam conexão/queda: recuperação assíncrona, credenciais de sessão não salva, parada/reabertura de OTA, bloqueio durante transferência, bloqueio de novas transferências durante recuperação, cooldown, recuperação manual e respeito à desconexão explícita. O e-mail agenda recuperação somente após falhas pré-autenticação persistentes; testes garantem que sucesso na repetição e LOGIN recusado não a disparam. As ações Lua de Wi-Fi e E-mail também são exercitadas.

Testes network_power, known_networks, mail e mail_transport passam com ASan/UBSan, separados do build ESP32-P4. Os backends Wi-Fi são simulados: **a recuperação física no P4/C6 ainda precisa ser testada**. Se o transporte esp-hosted/driver não responder a stop/start, essa recuperação pode falhar; o resultado ficará registrado. Não é alegada solução definitiva do travamento do caminho de dados. Nenhum flash, monitor, serial ou publicação executado.

Atualize BIN e SD juntos. Após o próximo erro, aguarde a tentativa automática ou use Reconectar Wi-Fi; confira se a página OTA volta a abrir antes de repetir Receber.
