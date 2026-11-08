# 0.8.0 / API 8 — redes conhecidas e token curto

## Usar

Rede → Buscar redes Wi-Fi → escolha o SSID → informe a senha → **Conectar e salvar**. Após a autenticação bem-sucedida, a rede aparece em **Redes salvas** e fica habilitada para conexão automática. **Conectar sem salvar** mantém apenas a conexão da sessão.

Na lista de redes salvas, escolha uma rede para **Conectar**, **Ativar/Desativar conexão automática** ou **Esquecer rede**. A interface não mostra a senha armazenada. Esquecer a rede atualmente conectada também desconecta; esquecer uma outra rede preserva a conexão atual.

O serviço residente do SD verifica conexão automática aproximadamente a cada segundo, com intervalo mínimo de 30 segundos entre ciclos de tentativa. Ao iniciar ou perder o link, busca redes e tenta as conhecidas habilitadas; um SSID oculto pode ser tentado pela última preferência salva quando nenhum conhecido aparece. As tentativas executam no worker de rede, sem chamadas de rádio no renderer. Conexão manual pode cancelar uma tentativa automática em andamento.

**Offline** impede conexão automática. **Desconectar Wi-Fi** suspende reconexões automáticas nesta sessão; conectar manualmente, reaplicar um perfil com rádio ativo ou reiniciar permite retomá-las. Desativar a conexão automática de uma rede persiste essa escolha. Redes usadas em versões anteriores precisam ser conectadas e salvas uma vez, pois antes a senha não era persistida.

## Dados e recuperação

Até 16 redes são guardadas em `/SymbianOS/config/network/known-wifi.tsv`, usando escrita transacional e a quota comum do Storage. O formato `T5WIFI1` codifica SSID/senha em hexadecimal e guarda a preferência automática; hexadecimal é codificação, **não criptografia**. Quem tiver acesso ao cartão pode recuperar as credenciais. A opção de conectar sem salvar está disponível para redes temporárias.

Só salva após conexão bem-sucedida. Cadastro malformado é preservado, sem sobrescrita silenciosa; o usuário continua conectado se houve sucesso no rádio mas falha ao salvar. A API de listagem retorna apenas SSID e preferência automática. Não grava senha ou SSID nos logs de diagnóstico. `.gitignore` protege a pasta de credenciais, e a ferramenta de release recusa um SD de distribuição contendo arquivos pessoais nessa pasta. Os ZIPs publicados usam a árvore modelo, sem credenciais.

## Transferência

O token temporário tem **8 caracteres**, escolhidos de um alfabeto de 32 letras maiúsculas/números sem I, O, 0 e 1. Os oito bytes de origem vêm do gerador aleatório do ESP. O token é renovado a cada abertura do serviço e invalidado ao encerrá-lo. A página converte a digitação para maiúsculas. Permanece transferência HTTP local por token, confinada a arquivos diretos de `home/Transfer`, até 64 MiB por arquivo; não é OTA de firmware.

Esta versão inclui a correção 0.7.3: buffer de upload no heap, pilha HTTP de 32 KiB, tratamento de timeout e logs por etapa. O usuário confirmou envio de arquivo bem-sucedido após essa correção. Isso é evidência de uso físico, não ensaio exaustivo de limites/queda de energia.

## Atualizar e validar

Atualize **BIN e SD juntos** para 0.8.0/API 8, preservando documentos e configurações. [Procedimento](ATUALIZACAO_LAUNCHER.md). O residente mantém compatibilidade com releases de API 1 a 7; o novo SD exige API 8 para a API de redes conhecidas.

Testes de host verificam persistência entre instâncias, reutilização da credencial, conexão sem salvar, senha errada sem cadastro, reconexão automática/backoff, perda de link, desconexão manual, preferência automática, esquecer, arquivo inválido preservado, tela sem senha e token de oito caracteres. As suites anteriores e sanitizadores continuam. Reconexão e token no Tab5 precisam de aceitação física, além do build ESP32-P4. Nenhum flash, monitor ou acesso serial foi executado pelo agente.

[Prévia de redes salvas no simulador](preview-saved-networks-0.8.png), com nomes e credenciais sintéticos em uma cópia temporária do SD.
