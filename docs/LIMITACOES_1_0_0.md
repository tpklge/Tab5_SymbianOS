# Limitações conhecidas — 1.0.0

**A numeração 1.0.0 identifica a release; não significa compatibilidade completa com a web, com Symbian original ou com todos os recursos do hardware.**

## Plataforma e instalação

- Destino: M5Stack Tab5, ESP32-P4/C6 e teclado oficial I²C. Outros dispositivos não foram validados.
- Requer launcher existente com slot de aplicação de 4 MiB. O BIN é somente a aplicação, não uma imagem completa da flash. Não criar NVS é requisito do projeto; credenciais Wi-Fi e dados da aplicação ficam no SD.
- Shell, recursos, aplicativos e persistência dependem de `/SymbianOS` no cartão. Quota: 4.000.000.000 bytes. Falha, remoção ou corrupção do cartão pode interromper o sistema; recuperação e rollback não substituem backup.
- Atualizações do runtime são feitas pelo launcher. Não há OTA interno do SymbianOS. A interface web com token transfere arquivos.

## Navegador, e-mail e feeds

O motor é litehtml com uma camada original de admissão e leitura alternativa. O navegador oferece perfil móvel por cabeçalhos; isso não implementa Android nem um navegador móvel completo.

- Recebimento de página limitado a 4 MiB; até cerca de 32 KiB de marcação admitida, 256 elementos e 24 níveis. O leitor textual também tem limites; documentos podem mostrar apenas trechos.
- CSS é um subconjunto: cores, ênfase, alinhamento, margens/padding e estilos simples. Imports, overlays, estruturas complexas e muitas regras são descartados. Layout idêntico ao navegador de desktop não é garantido.
- Imagens são carregadas por solicitação, com até 16 recursos por rodada e 256 KiB por recurso, além de orçamento de decodificação. Não há garantia para todos os formatos, imagens grandes ou conteúdo protegido por sessão.
- JavaScript inline compatível com ES5 básico: até 32 KiB de código, heap de 1,5 MiB e prazo de 300 ms para bytecode/RegExp. A ponte oferece document.write/writeln, document.title, getElementById(...).textContent e document.cookie.
- Scripts externos, modules, fetch/XHR, timers, DOM/eventos completos e frameworks modernos não são implementados. Funções C do interpretador não são preemptadas individualmente. Vídeos web e aplicações web complexas não são suportados por essa ponte.
- CAPTCHA, autenticação moderna, redirecionamentos de login e proteção contra automação podem impedir acesso. Google/G1/Globo e outros sites podem mudar sem aviso; seu funcionamento futuro não é garantido.
- Resposta HTTP/TLS incompleta não é tratada como página completa. Uma segunda tentativa GET em TLS 1.2 não garante resolver falha de rede, servidor ou transporte P4/C6. POST não é repetido automaticamente.
- Memória interna e PSRAM têm usos diferentes. Mesmo com PSRAM livre, a leitura formatada pode ser substituída por texto para preservar memória interna. Uma renderização interrompida também pode provocar leitura alternativa na abertura seguinte.
- E-mail e feeds compartilham a admissão HTML/CSS; JavaScript de mensagens/artigos não é executado. Aparência e recursos externos podem ser simplificados ou omitidos.
- IMAP/POP3/SMTP dependem de configuração, certificado e hora corretos e da autenticação aceita pelo servidor. OAuth2 e compatibilidade com todos os provedores não são garantidos; provedores podem exigir senha de aplicativo.

## Formulários e sessões

- Suporte a inputs text/password/email/search/number/hidden/submit e URL encoding GET/POST; até 8 formulários de 12 campos, quatro campos editáveis por tela.
- Sem select, checkbox, textarea, upload, JavaScript de submit ou autenticação web completa.
- POST é restrito à mesma origem; campos password exigem HTTPS. 301/302/303 passa a GET; 307/308 com reenvio POST é recusado.
- Até 64 cookies, restritos ao host exato e caminho, com tratamento básico de Secure, HttpOnly, SameSite e expiração. Domain= é rejeitado: sessões compartilhadas entre subdomínios podem falhar.
- Sessões persistem no SD até logout/limpeza. Credenciais e cookies no cartão não têm promessa de criptografia em repouso. O acesso físico ao SD permite lê-los.
- A transferência web local usa HTTP e token, sem TLS próprio. O token autentica o acesso, mas não cifra o tráfego. Não é um serviço projetado para exposição direta à internet.

## SIS e aplicativos

- SIS é o perfil sem assinatura deste projeto, para aplicativos Lua. Não há compatibilidade com binários, ABI, SDK ou aplicativos proprietários Symbian/Nokia.
- SISX e assinaturas ficam adiados. Pacotes incompatíveis, algoritmos ou recursos fora do perfil são rejeitados; Deflate de SIS não é aceito.
- Aplicam-se limites de tamanho, quantidade de arquivos, API/ABI, dependências e permissões descritos em [Perfil SIS](PERFIL_SIS_1.md) e [Ciclo de vida](SIS_CICLO_DE_VIDA.md).

## Alcance dos testes

O usuário informou funcionamento dos ajustes finais R3 no aparelho em 13/11/2026. Não foi executada pelo agente uma matriz física completa de hardware, autonomia, rádio, codecs, todos os provedores de e-mail ou todos os sites. Testes de host, jornada LVGL e build ESP32-P4 não substituem medições e uso prolongado no dispositivo.

As quedas anteriores foram mitigadas com alocação em PSRAM, pilhas externas, limites e recuperação em texto. Os registros disponíveis não identificam com certeza a instrução original de cada reset. Não se afirma ausência de bugs, falhas de rede ou reinícios em todos os cenários.
