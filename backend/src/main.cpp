#include "crow.h"
#include "database.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

// 1. DEFINIR MIME_TYPE PRIMERO para que todos lo vean
std::string get_mime_type(const std::string& path) {
    static const std::map<std::string, std::string> mime_types = {
        {".html", "text/html"}, {".js", "application/javascript"},
        {".mjs", "application/javascript"}, {".css", "text/css"},
        {".json", "application/json"}, {".wasm", "application/wasm"},
        {".svg", "image/svg+xml"}, {".png", "image/png"},
        {".jpg", "image/jpeg"}, {".ico", "image/x-icon"}
    };
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = path.substr(dot_pos);
        if (mime_types.count(ext)) return mime_types.at(ext);
    }
    return "application/octet-stream";
}

// 2. SERVIDO DE ARCHIVOS
void serve_file(crow::response& res, const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    if (is) {
        std::string contents((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        res.set_header("Content-Type", get_mime_type(path));
        res.write(contents);
    } else {
        res.code = 404;
        res.write("Not Found");
    }
}

int main() {
    crow::logger::setLogLevel(crow::LogLevel::Info);
    
    Database storage;
    const char* env_path = std::getenv("DB_PATH");
    storage.init(env_path ? env_path : "/database/app.db");

    crow::SimpleApp app;

    // 3. MANEJADOR ESTÁTICOS (Corregido)
    auto static_handler = [](crow::response& res, std::string path) {
        std::string full_path = "/static_assets/" + path;
        
        if (fs::exists(full_path) && !fs::is_directory(full_path)) {
            serve_file(res, full_path);
        } else {
            serve_file(res, "/static_assets/index.html");
        }
        
        res.add_header("Cross-Origin-Opener-Policy", "same-origin");
        res.add_header("Cross-Origin-Embedder-Policy", "require-corp");
        res.end();
    };

    // --- API: GUARDAR ---
    CROW_ROUTE(app, "/api/save-localstorage").methods(crow::HTTPMethod::POST)
    ([&storage](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "JSON Invalido");

        // Solución definitiva al error de tipos r_string vs const char*
        std::string key = "";
        if (x.has("key")) key = std::string(x["key"].s());
        else if (x.has("key_name")) key = std::string(x["key_name"].s());

        if (key.empty()) return crow::response(400, "Falta clave");

        std::string val = "";
        if (x.has("value")) {
            if (x["value"].t() == crow::json::type::String) {
                val = std::string(x["value"].s());
            } else {
                std::ostringstream os;
                os << x["value"]; 
                val = os.str();
            }
        }

        if (storage.save_user(key, val)) return crow::response(201);
        return crow::response(500, "Error DB");
    });

    // --- API: CONSULTAR ---
    CROW_ROUTE(app, "/api/get-biometrics")
    ([&storage]() {
        crow::response out(200, storage.get_all_biometrics());
        out.set_header("Content-Type", "application/json");
        out.set_header("Access-Control-Allow-Origin", "*");
        return out;
    });

    // --- RUTAS ---
    CROW_ROUTE(app, "/<path>")([&](crow::response& res, std::string path) { static_handler(res, path); });
    CROW_ROUTE(app, "/")([&](crow::response& res) { static_handler(res, ""); });

    // --- SSL ---
    const std::string cert = "/bin/server.crt", key_file = "/bin/server.key";
    if (fs::exists(cert) && fs::exists(key_file)) {
        app.ssl_file(cert, key_file);
    }

    app.port(443).multithreaded().run();
    storage.close();
    return 0;
}