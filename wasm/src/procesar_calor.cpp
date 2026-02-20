#include <emscripten.h>
#include <vector>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void procesar_calor(float* actual, float* siguiente, int width, int height) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                int idx = y * width + x;
                
                // Promedio simple (Laplaciano) para simular difusión
                float suma = actual[idx - width - 1] + actual[idx - width] + actual[idx - width + 1] +
                             actual[idx - 1]          + actual[idx]         + actual[idx + 1]         +
                             actual[idx + width - 1] + actual[idx + width] + actual[idx + width + 1];
                
                siguiente[idx] = suma / 9.0f;
            }
        }
    }
}