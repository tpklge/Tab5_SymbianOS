Duktape 2.7.0, MIT: https://duktape.org/duktape-2.7.0.tar.xz

Fontes upstream em vendor/, licença preservada. Integração original em
components/platform/browser_script.cpp. Configuração local no fim de duk_config.h:
interruptor de execução, limites de recursão/compilação e pilha de chamadas.
Alteração em duktape.c: o executor RegExp verifica o mesmo prazo a cada 1024
passos, além do limite upstream. Nenhuma API nativa de arquivos/rede é exposta.
Alocador limitado a 1,5 MiB; memória em PSRAM no P4; prazo de 300 ms. O prazo
interrompe bytecode e RegExp, não preempta cada função C do interpretador.
