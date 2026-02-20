#include <emscripten.h>
bool estado_toggle = false;

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    const char* mensaje() {
        estado_toggle = !estado_toggle;

        if (estado_toggle) {
            return "Mensaje 1";
        } else {
            return "Mensaje 2";
        }
    }
}
