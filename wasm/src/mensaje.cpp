#include <emscripten.h>

// Variable de estado que persiste en la memoria del WASM
bool estado_toggle = false;

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    const char* mensaje() {
        // Cambiamos el estado cada vez que se invoca la función
        estado_toggle = !estado_toggle;

        if (estado_toggle) {
            return "Estado A: El servidor C++ dice hola.";
        } else {
            return "Estado B: El toggle ha cambiado en WASM.";
        }
    }
}
