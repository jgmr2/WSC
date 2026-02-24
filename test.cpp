#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <curl/curl.h>
#include <unistd.h>
#include <random>
#include <sstream>

const std::string URL = "http://localhost:80/api/save-localstorage";
const int TOTAL_REQUESTS = 100; // Bajamos a 100 para no saturar el log del navegador
const int CONCURRENT_THREADS = 10;
const int DIMS = 478 * 3; // 1434 dimensiones

// Generador de vectores aleatorios para simular datos reales
std::string generate_vector_json(int size) {
    std::stringstream ss;
    ss << "[";
    static std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-0.5, 0.5);
    for (int i = 0; i < size; ++i) {
        ss << dis(gen) << (i == size - 1 ? "" : ",");
    }
    ss << "]";
    return ss.str();
}

void send_post(int id) {
    CURL* curl = curl_easy_init();
    if(curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Creamos la estructura compleja que espera Svelte 5
        std::string mean_v = generate_vector_json(DIMS);
        std::string inv_v = generate_vector_json(DIMS);
        std::string user_id = "FAKE_ID_" + std::to_string(id);

        // OJO: Escapamos comillas para que el campo "value" sea un string JSON válido dentro de otro JSON
        std::string inner_json = "{"
            "\\\"id\\\":\\\"" + user_id + "\\\","
            "\\\"thumb\\\":\\\"data:image/webp;base64,UklGRstresstest\\\","
            "\\\"model\\\":{"
                "\\\"mean\\\":" + mean_v + ","
                "\\\"invCovariance\\\":" + inv_v + ""
            "}"
        "}";

        std::string payload = "{\"key\":\"BIO_" + user_id + "\", \"value\":\"" + inner_json + "\"}";

        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) std::cerr << "!" << std::flush;
        else std::cout << "." << std::flush;

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

int main() {
    curl_global_init(CURL_GLOBAL_ALL);
    std::vector<std::thread> workers;
    std::cout << "🚀 Inyectando perfiles biométricos realistas..." << std::endl;

    for (int i = 0; i < TOTAL_REQUESTS; ++i) {
        workers.emplace_back(send_post, i);
        if (workers.size() >= CONCURRENT_THREADS) {
            for (auto& t : workers) if(t.joinable()) t.join();
            workers.clear();
        }
    }
    for (auto& t : workers) if(t.joinable()) t.join();
    
    std::cout << "\n✅ Test completado con éxito." << std::endl;
    curl_global_cleanup();
    return 0;
}