#include "crow.h"

int main()
{

    crow::SimpleApp app;

    // En Crow, para servir archivos estáticos se usa esto:
    // Crow buscará automáticamente en la carpeta "static" relativa al binario.
    
    CROW_ROUTE(app, "/")
    ([](crow::response& res) {
        // Buscamos el index.html dentro de nuestra carpeta de assets
        res.set_static_file_info("static_assets/index.html");
        res.end();
    });
    
    // Esta ruta comodín permite que Svelte cargue sus JS, CSS y el WASM
    // Crow servirá cualquier archivo que esté dentro de la carpeta "static"
    CROW_ROUTE(app, "/<path>")
    ([](crow::response& res, std::string path){
        res.set_static_file_info("static_assets/" + path);
        res.end();
    });

    app.port(8080).multithreaded().run();
}