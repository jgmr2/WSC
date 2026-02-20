#include <emscripten.h>
#include <stdlib.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    double test_montecarlo(int iteraciones) {
        int dentro = 0;
        for (int i = 0; i < iteraciones; i++) {
            double x = (double)rand() / RAND_MAX;
            double y = (double)rand() / RAND_MAX;
            if (x*x + y*y <= 1.0) dentro++;
        }
        return 4.0 * dentro / iteraciones;
    }
}