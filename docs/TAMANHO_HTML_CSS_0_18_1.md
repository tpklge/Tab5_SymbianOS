# Tamanho do motor HTML/CSS e slots de firmware

O limite de 2 MiB vem de partitions.csv: ota_0 e ota_1 têm tamanho 0x200000 cada. O build e o empacotador recusam imagens maiores para preservar a compatibilidade com esse layout. O projeto está configurado para flash de 16 MiB. Não é limite do Lua, do microSD nem da capacidade total da flash. A tabela instalada pelo launcher pode ser diferente da tabela standalone do projeto; a revisão OTA informa a capacidade efetiva do slot escolhido no dispositivo.

É possível projetar um layout com slots maiores (por exemplo, dois de 4 MiB) dentro de 16 MiB, mas isso exige uma migração planejada da tabela de partições e compatibilidade com o launcher. Uma atualização comum de aplicativo não aumenta o slot. Nenhuma partição foi alterada nem foi executado flash.

## Medição de compilação RISC-V, não de execução física

Foi compilado litehtml v0.10, commit 9bc84b8b8d15a4e50f18b327aa30955048b441c2, com o toolchain riscv32-esp-elf do projeto, arquitetura RV32IMAFC/P4, ABI ilp32f, otimização -Os e seções por função/dado. RTTI e exceções foram necessários ao código original. Foram somadas as seções dos objetos dos arquivos estáticos com riscv32-esp-elf-size:

| Biblioteca | text | data | bss | text + data |
| --- | ---: | ---: | ---: | ---: |
| Litehtml | 530817 | 370276 | 1708 | 901093 |
| Gumbo | 282474 | 620 | 0 | 283094 |
| Soma | 813291 | 370896 | 1708 | 1184187 |

São aproximadamente 1,13 MiB de código/dados dos núcleos, antes de linkar a aplicação de renderização. Não é uma medição do aumento final do BIN: remoção de funções não usadas e LTO podem reduzir o valor, enquanto rasterização, recursos, fontes, RTTI/exceções e integração podem acrescentar código. A soma aritmética ao firmware atual fica na ordem de 3,12 MiB, mas esse não é um binário integrado medido. Não há garantia de que um slot específico comportaria o resultado sem realizar esse link.

Se o motor for compilado como módulo ELF no microSD, os aproximadamente 1,13 MiB não precisam estar dentro do BIN residente, mas o firmware precisa de carregador/ABI, e o módulo precisa de RAM executável e integração gráfica. O crescimento desse carregador ainda não foi medido. Fontes, imagens e recursos continuam no SD. Motor HTML/CSS completo não foi implementado nesta release; Litehtml é um motor HTML/CSS com limites próprios, não uma promessa de todos os padrões de um navegador moderno.

Artefatos de estudo em build/html-engine-probe, fora da distribuição. Fontes: https://github.com/litehtml/litehtml/tree/v0.10 ; https://components.espressif.com/components/espressif/elf_loader/versions/2.1.0/readme .
