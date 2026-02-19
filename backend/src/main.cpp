#include "crow.h"
#include <string>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Función auxiliar compatible con C++17 para verificar extensiones
bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main()
{
    crow::SimpleApp app;
    
    // Carpeta donde Svelte deja los archivos (generada por Bun/Vite)
    const std::string static_dir = "static_assets/";

    // 1. RUTA PARA ARCHIVOS ESTÁTICOS Y ASSETS
    CROW_ROUTE(app, "/<path>")
    ([&static_dir](crow::response& res, std::string path) {
        std::string full_path = static_dir + path;

        // Si el archivo existe físicamente, lo servimos
        if (fs::exists(full_path) && !fs::is_directory(full_path)) {
            res.set_static_file_info(full_path);
            
            // Configuración manual de MIME Types para evitar bloqueos del navegador
            if (ends_with(path, ".wasm")) {
                res.add_header("Content-Type", "application/wasm");
            } 
            else if (ends_with(path, ".js")) {
                res.add_header("Content-Type", "application/javascript");
            }
            else if (ends_with(path, ".css")) {
                res.add_header("Content-Type", "text/css");
            }
        } 
        else {
            // FALLBACK: Si el archivo no existe, es una ruta de Svelte (ej: /editor)
            // Entregamos el index.html y dejamos que Svelte maneje el routing.
            res.set_static_file_info(static_dir + "index.html");
            res.code = 200;
        }

        // Cabeceras de seguridad para habilitar SharedArrayBuffer y optimizar WASM
        res.add_header("Cross-Origin-Opener-Policy", "same-origin");
        res.add_header("Cross-Origin-Embedder-Policy", "require-corp");
        
        res.end();
    });

    // 2. RUTA RAÍZ
    CROW_ROUTE(app, "/")
    ([&static_dir](crow::response& res) {
        res.set_static_file_info(static_dir + "index.html");
        res.end();
    });

    // 3. CATCH-ALL (Red de seguridad final)
    CROW_CATCHALL_ROUTE(app)
    ([&static_dir](crow::response& res) {
        // Cualquier error 404 lo redirigimos al index.html para el SPA
        res.set_static_file_info(static_dir + "index.html");
        res.code = 200;
        res.end();
    });

    std::cout << "Servidor Crow corriendo en http://0.0.0.0:8080" << std::endl;
    
    app.port(8080)
       .multithreaded()
       .run();
}