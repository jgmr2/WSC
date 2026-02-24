#include "crow.h"
#include <sqlite3.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;
sqlite3* db;

/**
 * Inicializa la base de datos con optimizaciones de alto rendimiento.
 * WAL Mode permite que las lecturas no bloqueen las escrituras.
 */
void init_db() {
    const char* env_path = std::getenv("DB_PATH");
    std::string db_path = (env_path != nullptr) ? env_path : "app.db";

    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        CROW_LOG_CRITICAL << "CRITICAL: No se pudo abrir SQLite: " << sqlite3_errmsg(db);
        exit(1);
    }

    // --- CONFIGURACIÓN DE RENDIMIENTO ---
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA cache_size=-64000;", 0, 0, 0); // 64MB de cache

    const char* sql = "CREATE TABLE IF NOT EXISTS user_data ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "key_name TEXT UNIQUE,"
                      "value_data TEXT,"
                      "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP);"
                      "CREATE INDEX IF NOT EXISTS idx_key_name ON user_data(key_name);";
    
    char* err_msg = nullptr;
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        CROW_LOG_ERROR << "SQL Error: " << err_msg;
        sqlite3_free(err_msg);
    } else {
        std::cout << "✓ ALPHA DB READY: " << db_path << " [WAL Mode]" << std::endl;
    }
}

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main() {
    crow::logger::setLogLevel(crow::LogLevel::Info);
    init_db();
    crow::SimpleApp app;
    const std::string static_dir = "static_assets/";

    // --- API: GUARDAR Y VALIDAR (POST) ---
    CROW_ROUTE(app, "/api/save-localstorage").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x || !x.has("key") || !x.has("value")) {
            return crow::response(400, "Invalid JSON structure");
        }

        std::string key_str = (std::string)x["key"].s();
        std::string value_str = (std::string)x["value"].s();

        // 1. VALIDACIÓN: Comprobar existencia previa
        const char* check_sql = "SELECT count(*) FROM user_data WHERE key_name = ?;";
        sqlite3_stmt* check_stmt;
        sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr);
        sqlite3_bind_text(check_stmt, 1, key_str.c_str(), -1, SQLITE_TRANSIENT);
        
        int exists = 0;
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);

        if (exists > 0) {
            CROW_LOG_WARNING << "Conflicto: El usuario ya existe -> " << key_str;
            return crow::response(409, "User already exists");
        }

        // 2. INSERCIÓN: Nuevo registro
        const char* sql = "INSERT INTO user_data (key_name, value_data) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB Error");
        }

        sqlite3_bind_text(stmt, 1, key_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, value_str.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return crow::response(500, "Write Error");
        }

        sqlite3_finalize(stmt);
        CROW_LOG_INFO << "Perfil creado exitosamente: " << key_str;
        return crow::response(201, "Created");
    });

    // --- API: OBTENER BIOMETRÍAS (GET) ---
    CROW_ROUTE(app, "/api/get-biometrics")
    ([]() {
        sqlite3_stmt* stmt;
        // Limitamos para evitar saturación de RAM
        const char* sql = "SELECT value_data FROM user_data LIMIT 5000;";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "Read Error");
        }

        std::string res_body = "[";
        bool first = true;
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) res_body += ",";
            const char* val = (const char*)sqlite3_column_text(stmt, 0);
            if (val) res_body += val;
            first = false;
            count++;
        }
        res_body += "]";
        sqlite3_finalize(stmt);

        CROW_LOG_INFO << "Consulta biometrias: " << count << " registros enviados.";
        return crow::response(200, "application/json", res_body);
    });

    // --- SERVIDOR DE ARCHIVOS ESTÁTICOS ---
    CROW_ROUTE(app, "/<path>")
    ([&static_dir](crow::response& res, std::string path) {
        std::string full_path = static_dir + path;
        if (fs::exists(full_path) && !fs::is_directory(full_path)) {
            res.set_static_file_info(full_path);
            if (ends_with(path, ".wasm")) res.add_header("Content-Type", "application/wasm");
            else if (ends_with(path, ".js")) res.add_header("Content-Type", "application/javascript");
        } else {
            res.set_static_file_info(static_dir + "index.html");
        }
        res.add_header("Cross-Origin-Opener-Policy", "same-origin");
        res.add_header("Cross-Origin-Embedder-Policy", "require-corp");
        res.end();
    });

    CROW_ROUTE(app, "/")
    ([&static_dir](crow::response& res) {
        res.set_static_file_info(static_dir + "index.html");
        res.end();
    });

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "✓ PROTOCOLO ALPHA: BACKEND OPERATIVO" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    app.port(8080).multithreaded().run();
    sqlite3_close(db);
}