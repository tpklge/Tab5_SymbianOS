# Versão 0.19.0 — leitor HTML/CSS e slots de 4 MiB

O leitor de e-mail e RSS usa litehtml v0.10 (commit 9bc84b8b8d15a4e50f18b327aa30955048b441c2), Gumbo e rasterização RGB565. O núcleo de análise/layout fica no firmware; navegação, paginação, CSS padrão, fonte e recursos ficam no microSD. Recursos externos exigem ação explícita. Não executa JavaScript, formulários ou aplicações web; a compatibilidade CSS é a do litehtml, não a de um navegador moderno completo.

Limites: HTML do navegador até 1 MiB; campos HTML enviados pelo Lua até 96 KiB, 8192 elementos de abertura (scripts e comentários excluídos), profundidade máxima de 128 e até 32768 nós, até 256 páginas, cache limitado e renderização cancelável. A fonte do SD substitui famílias não disponíveis. Recursos remotos permanecem em cache e podem ser lidos offline.

Pastas do e-mail passam a uma coluna de ícones. Enviar fila permite uma nova tentativa manual de mensagens rejeitadas após corrigir as credenciais; envios com resultado incerto continuam excluídos da repetição automática. O diagnóstico serial registra códigos, sem senhas nem corpo das mensagens.

Testes e medições agora acrescenta uma linha por amostra, evitando reescrever todo o histórico e esgotar o orçamento Lua. Regressão de host cobre 400 amostras e rotação dos arquivos.

## Partições

Mapa compatível com o Launcher do Tab5: NVS 0x9000/0x5000; otadata 0xe000/0x2000; factory 0x10000/0x180000; coredump 0x190000/0x10000; ota_0 0x1a0000/0x400000; ota_1 0x5a0000/0x400000. O usuário confirmou a montagem e conferência pelo PMAN. Cada slot tem 4194304 bytes. O projeto não grava essa tabela automaticamente.

Pela WebGUI do launcher, envie somente SymbianOS-runtime.bin como aplicação, escolhendo um slot OTA do SymbianOS. Atualize também o SD pelo pacote incremental 0.19.0. Não envie bootloader, tabela de partições ou imagem combinada como runtime. A instalação standalone não preserva automaticamente um launcher existente.

As mudanças foram verificadas em host e compilação ESP32-P4. SMTP real, renderização no painel e alternância OTA precisam de validação física; não houve flash nesta preparação.

## Licenças e alterações locais

litehtml MIT; Gumbo Apache-2.0, com avisos preservados na distribuição. Gumbo usa alocadores direcionados à PSRAM e conversão para unsigned char em isalnum; a construção do documento usa guarda RAII para liberar a árvore Gumbo em exceções. Rasterização usa stb_truetype da dependência LVGL existente.
