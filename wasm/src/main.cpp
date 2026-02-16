#include <emscripten/emscripten.h>

extern "C" {

// Esta función será llamable desde Svelte
EMSCRIPTEN_KEEPALIVE
int sumar(int a, int b) {
    return a + b;
}

}