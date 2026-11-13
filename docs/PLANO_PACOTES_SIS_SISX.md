# Plano de pacotes SIS/SISX para Tab5Symbian

Estado atualizado: fases 1–6 implementadas para SIS sem assinatura. Contratos em [PERFIL_SIS_1.md](PERFIL_SIS_1.md), [SIS_CICLO_DE_VIDA.md](SIS_CICLO_DE_VIDA.md) e [SIS_SDK.md](SIS_SDK.md). Validação física permanece pendente e tem roteiro próprio. Assinaturas e SISX continuam exclusivamente na fase 7. Os limites implementados prevalecem sobre os tetos propostos abaixo.

## Objetivo e compatibilidade

Distribuir aplicações originais do Tab5Symbian, inicialmente Lua e recursos, em um subconjunto documentado do contêiner SIS v9. O suporte ao contêiner não torna executáveis de telefones Symbian compatíveis com o ESP32-P4. Não haverá emulação ARM, carregamento de DLLs E32, API Symbian, lojas ou certificados proprietários. Aplicações nativas futuras exigirão outro projeto de ABI e isolamento.

A especificação v9 descreve cabeçalho, campos TLV, alinhamento, metadados, dados comprimidos, hashes e assinaturas. A implementação deverá seguir esses campos, sem inventar identificadores reservados. Arquivos SIS anteriores à v9 ficam fora da primeira fase e serão rejeitados com diagnóstico específico. Referência técnica: [Symbian OS v9.X SIS File Format Specification, v1.1, junho de 2006](https://www.cryer.co.uk/file-types/s/sis/softwareinstallsis.pdf), documento original hospedado em espelho.

O fluxo de referência usa PKG para descrever o pacote, MakeSIS para produzir SIS e SignSIS para assinatura. Referência: [MakeSIS — documentação original do SDK](https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-35228542-8C95-4849-A73F-2B4F082F0C44/sdk/doc_source/ToolsAndUtilities94/Installing-ref/MakesisToolReference.guide.html). O projeto escreverá ferramentas próprias e não distribuirá o SDK legado. Adotaremos `.sis` para saída sem assinatura e `.sisx` para saída assinada; a extensão nunca substituirá a inspeção do conteúdo nem a verificação criptográfica. Renomear ZIP/T5PKG para SIS será inválido.

## Contrato de aplicação a definir antes do código

Cada pacote declarará UID de 32 bits, identificador estável, nome, autor, versão de três componentes, versão mínima/máxima da API Tab5, ponto de entrada Lua, idiomas, dependências, licenças, permissões solicitadas e lista fechada de arquivos. UID é identidade local do ecossistema Tab5: não haverá alegação de reserva ou registro oficial Symbian. Registrar colisões e exigir a mesma identidade de assinatura para substituir uma aplicação.

Um manifesto Tab5 versionado será um arquivo normal do payload, vinculado criptograficamente ao restante do pacote, sem extensões arbitrárias em campos reservados. Metadados duplicados no controlador e nesse manifesto devem coincidir. Inicialmente aceitar apenas instalação de arquivos e um único pacote por arquivo: recusar scripts de instalação, execução de programas, condições não suportadas, dependências embutidas, componentes de sistema e arquivos de firmware. Documentar o subconjunto PKG aceito; comandos desconhecidos falham de forma explícita.

## Geração no computador

1. Validar manifesto, Lua, recursos, licenças e identidade; recusar links simbólicos, caminhos absolutos, duplicados e arquivos fora da raiz do projeto da aplicação.
2. Converter o subconjunto PKG/manifesto para um modelo intermediário independente do formato. Ordenar arquivos e normalizar timestamps para geração reproduzível.
3. Emitir cabeçalho, controlador, descrições, hashes e blocos de dados SIS v9 com tamanhos verificados e compressão suportada. Conferir o resultado com um leitor independente e fixtures legais conhecidas. Não assumir interoperabilidade antes desses testes.
4. No modo de desenvolvimento, produzir `.sis` e um relatório de conteúdo, permissões e SHA-256 externo. No modo de distribuição, assinar o controlador e produzir `.sisx`, relatório de assinatura e cadeia de certificados. A chave privada permanece no computador, fora de repositórios, cartão e artefatos.
5. Verificar novamente o arquivo final com o mesmo perfil que o dispositivo aceitará e publicar somente pacote, relatório e licenças. Comandos de CLI e layout exatos serão definidos na fase de implementação.

A seleção de algoritmos será uma decisão prévia e testada: o formato legado possui restrições próprias. Não substituir hashes ou algoritmos em campos padronizados por valores inventados. O perfil deverá exigir assinatura moderna verificável no P4, ou declarar explicitamente a incompatibilidade com ferramentas antigas. Se não houver interseção segura e interoperável, interromper essa fase para decidir o perfil; não enfraquecer a verificação silenciosamente. SHA-256 externo é integridade de transporte, não prova de autoria.

## Transferência e pré-instalação

Enviar o arquivo pela interface web existente, autenticada pelo token da sessão, para `/SymbianOS/home/Transfer/`. O token autoriza transferência; não concede confiança de instalação nem permissões à aplicação. Abrir o arquivo pelo gerenciador/instalador mostrará nome, autor, versão, compatibilidade, tamanho comprimido/descomprimido, permissões e resultado da assinatura antes da confirmação.

O leitor terá limites iniciais propostos: pacote de até 32 MiB, payload de até 64 MiB, 512 arquivos, metadados de até 256 KiB, profundidade estrutural de 16 campos e nenhuma instalação embutida. São limites do nosso perfil, sujeitos a medições, não limites atribuídos ao formato original. Usar leitura por blocos, aritmética com overflow verificado, teto de descompressão e cancelamento. Rejeitar truncamentos, sobreposições, tamanho divergente, caminhos com `..`, unidades, NUL, separadores ambíguos, colisões de caixa e nomes reservados. Validar UTF-16 do formato e converter nomes de forma determinística.

## Confiança e permissões

Certificados confiáveis e decisões persistentes ficam apenas no SD, em área de configuração administrada pelo sistema. Certificados autoassinados não serão automaticamente confiáveis. Pacotes sem assinatura terão modo de desenvolvimento explícito, confirmação individual e permissões limitadas. Atualizações devem preservar a identidade do assinante; troca de chave exige autorização específica. Sem relógio confiável, não declarar válida uma cadeia cuja validade temporal não pôde ser verificada. Planejar revogação offline e política de expiração antes da distribuição pública.

Capacidades do contêiner não mapeiam automaticamente para permissões Tab5. Negar por padrão e apresentar apenas capacidades reconhecidas. Lua continua isolada pelas APIs e grants existentes; nenhum pacote recebe acesso a `/`, configuração privada de terceiros, PIN, credenciais ou mecanismos de firmware.

## Instalação e recuperação no cartão

1. Resolver dependências instaladas, detectar ciclos e conflitos de UID/arquivos, validar API e conferir espaço livre físico mais quota global de 4.000.000.000 bytes. Reservar espaço para pacote, staging, nova versão, versão anterior e journal; a reserva de segurança do Storage continua obrigatória.
2. Extrair para uma transação privada sob `/SymbianOS/tmp/install/<id>/`. Fazer todas as verificações antes de alterar o catálogo. Nunca executar payload durante instalação.
3. Preparar versão imutável em `/SymbianOS/apps/<app-id>/versions/<versão>/`, com manifesto de hashes, origem e permissões concedidas. Parar/checkpointar a aplicação antes da troca.
4. Persistir journal e trocar o registro ativo por operação recuperável. FAT não garante uma transação de múltiplos arquivos: especificar os estados `prepared`, `committing`, `committed` e a recuperação idempotente. Queda de energia deve deixar uma versão completa selecionável, nunca misturar versões.
5. Confirmar abertura inicial da nova aplicação antes de liberar a versão anterior. Migrações de dados precisam de cópia recuperável, versão de esquema e consentimento para alterações irreversíveis. Falha não pode destruir dados anteriores.
6. No boot, recuperar transações interrompidas antes de expor aplicações ao shell. Se o cartão mudar durante a operação, invalidar a transação pela geração do Storage.

Registro proposto em `/SymbianOS/meta/packages/`: UID, versão, dependências, hash do pacote, identidade de assinatura, concessões, arquivos de propriedade exclusiva e versão ativa/anterior. Desinstalar remove apenas arquivos pertencentes ao pacote; conservar dados por padrão e oferecer exclusão separada. Bloquear remoção enquanto houver dependentes. Downgrade exige escolha explícita e compatibilidade dos dados.

## Fases e critérios de aceite

1. **Contrato e fixtures:** fixar perfil SIS v9, algoritmos, UID, manifesto e ameaças; confirmar licenças dos materiais de referência. Entrega: especificação e corpus de exemplos originais.
2. **Ferramentas de host:** gerador e leitor independentes, determinismo e verificador sem assinatura; testes de truncamento em cada posição, fuzzing, limites e colisões. Nenhuma escrita no Tab5.
3. **Inspeção no dispositivo:** abrir pacote e mostrar revisão, ainda sem instalar. Medir RAM/PSRAM, tempo, responsividade e cancelamento com pacotes no limite.
4. **Instalação transacional:** aplicativos Lua simples, registro e recuperação; injetar falhas em cada escrita, espaço insuficiente, remoção de cartão e corrupção. Testar primeiro em host, depois fisicamente no Tab5.
5. **Atualização e remoção:** dependências, dados persistentes, permissões novas, identidade, downgrade e rollback. Validar interrupção real de energia em diferentes pontos.
6. **Distribuição SIS sem assinatura:** documentação para autores, comando de geração, exemplos MIT e instalação por token. Declarar exatamente o perfil SIS suportado e os resultados físicos disponíveis.
7. **Assinaturas por último:** especificar confiança, algoritmos e cadeia de certificados, geração/validação de SISX e atualização com identidade autenticada.

Implementados: contrato, exemplos MIT, gerador, inspeção nativa/host, instalação, dependências, atualização, migração declarativa, downgrade confirmado, rollback com snapshot, recuperação, remoção e distribuição SDK/kit. Assinaturas, certificados e SISX não foram implementados. Os testes físicos de corte de energia dependem da execução do roteiro pelo usuário.
