#include <emscripten/emscripten.h>
#include <vector>

// Estructura para representar el color en formato RGB
struct Color {
    int r, g, b;
};

// Estado global del círculo
Color currentColor = {255, 0, 0}; // Empezamos en rojo

extern "C" {

    // Función que será llamada desde JS al hacer click
    EMSCRIPTEN_KEEPALIVE
    void handle_click() {
        // Lógica simple: rotar colores básicos
        if (currentColor.r == 255 && currentColor.g == 0) {
            currentColor = {0, 255, 0}; // Verde
        } else if (currentColor.g == 255) {
            currentColor = {0, 0, 255}; // Azul
        } else {
            currentColor = {255, 0, 0}; // Rojo
        }
    }

    // Funciones para obtener los componentes del color
    EMSCRIPTEN_KEEPALIVE
    int get_r() { return currentColor.r; }
    
    EMSCRIPTEN_KEEPALIVE
    int get_g() { return currentColor.g; }
    
    EMSCRIPTEN_KEEPALIVE
    int get_b() { return currentColor.b; }
}