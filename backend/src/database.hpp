#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include "crow.h"

struct Database {
    sqlite3* db;

    /**
     * Inicializa la base de datos.
     * @param db_path_input Ruta opcional. Si está vacía, busca en DB_PATH o usa defecto.
     */
    void init(std::string db_path_input = "") {
        const char* env_path = std::getenv("DB_PATH");
        
        // Prioridad: 1. Parámetro de función | 2. Variable Entorno | 3. Hardcoded
        std::string db_path = !db_path_input.empty() ? db_path_input : 
                             (env_path != nullptr ? env_path : "app.db");

        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            CROW_LOG_CRITICAL << "CRITICAL: No se pudo abrir SQLite: " << sqlite3_errmsg(db);
            exit(1);
        }

        // Optimización WAL (Write-Ahead Logging) para alto rendimiento
        sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
        sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", 0, 0, 0);
        sqlite3_exec(db, "PRAGMA cache_size=-64000;", 0, 0, 0); // 64MB cache

        const char* sql = "CREATE TABLE IF NOT EXISTS user_data ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "key_name TEXT UNIQUE,"
                          "value_data TEXT,"
                          "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP);"
                          "CREATE INDEX IF NOT EXISTS idx_key_name ON user_data(key_name);";
        
        char* err_msg = nullptr;
        if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
            CROW_LOG_ERROR << "SQL Error en inicialización: " << err_msg;
            sqlite3_free(err_msg);
        } else {
            std::cout << "✓ ALPHA DB READY: " << db_path << " [WAL Mode]" << std::endl;
        }
    }

    bool user_exists(const std::string& key) {
        const char* sql = "SELECT count(*) FROM user_data WHERE key_name = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        
        int exists = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return exists > 0;
    }

    bool save_user(const std::string& key, const std::string& value) {
        // Usamos INSERT OR REPLACE si quieres actualizar, o solo INSERT para prevenir duplicados
        const char* sql = "INSERT OR IGNORE INTO user_data (key_name, value_data) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

        int res = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        // Verifica que se haya insertado correctamente
        return (res == SQLITE_DONE);
    }

    std::string get_all_biometrics() {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT value_data FROM user_data LIMIT 5000;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return "[]";

        std::string res = "[";
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) res += ",";
            const char* val = (const char*)sqlite3_column_text(stmt, 0);
            if (val) {
                res += val;
                first = false;
            }
        }
        res += "]";
        sqlite3_finalize(stmt);
        return res;
    }

    void close() {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
    }
};