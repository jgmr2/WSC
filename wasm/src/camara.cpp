#include <emscripten/bind.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace emscripten;

/**
 * Genera el "Ash" (Hash Biométrico) basado en los 68 landmarks.
 * @param buffer: Puntero a la memoria compartida (Float32Array [x0,y0,x1,y1...])
 */
std::string generate_geometric_hash(uintptr_t buffer) {
    if (buffer == 0) return "VOID";
    float* landmarks = reinterpret_cast<float*>(buffer);

    // 1. Usamos la punta de la nariz (Landmark 30) como origen (0,0)
    float refX = landmarks[30 * 2];
    float refY = landmarks[30 * 2 + 1];

    // 2. Usamos la distancia entre ojos (36 y 45) como factor de escala
    float dx = landmarks[45 * 2] - landmarks[36 * 2];
    float dy = landmarks[45 * 2 + 1] - landmarks[36 * 2 + 1];
    float eyeDist = std::sqrt(dx*dx + dy*dy);
    if (eyeDist == 0) return "INVALID_SCALE";

    std::stringstream ss;
    ss << "V6-";
    ss << std::fixed << std::setprecision(2);

    // 3. Serializamos los 68 puntos normalizados
    for (int i = 0; i < 68; i++) {
        float normX = (landmarks[i * 2] - refX) / eyeDist;
        float normY = (landmarks[i * 2 + 1] - refY) / eyeDist;
        ss << normX << "x" << normY << ";";
    }

    return ss.str();
}

/**
 * Compara dos hashes geométricos calculando la distancia euclidiana media
 */
float get_similarity(std::string hash_live, std::string hash_db) {
    if (hash_live.substr(0, 3) != "V6-" || hash_db.substr(0, 3) != "V6-") return 0.0f;

    auto parse = [](std::string h) {
        std::vector<float> coords;
        std::stringstream ss(h.substr(3));
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            size_t xPos = segment.find('x');
            if (xPos != std::string::npos) {
                coords.push_back(std::stof(segment.substr(0, xPos)));
                coords.push_back(std::stof(segment.substr(xPos + 1)));
            }
        }
        return coords;
    };

    auto c1 = parse(hash_live);
    auto c2 = parse(hash_db);
    if (c1.size() != c2.size() || c1.empty()) return 0.0f;

    float total_dist = 0;
    for (size_t i = 0; i < c1.size(); i += 2) {
        total_dist += std::sqrt(std::pow(c1[i] - c2[i], 2) + std::pow(c1[i+1] - c2[i+1], 2));
    }

    float avg_error = total_dist / (c1.size() / 2);
    // Un error menor a 0.15 suele ser la misma persona
    float score = 1.0f - std::min(1.0f, avg_error / 0.5f);
    return (score > 0.85f) ? score : score * 0.4f; 
}

EMSCRIPTEN_BINDINGS(biometric_module) {
    function("generate_geometric_hash", &generate_geometric_hash);
    function("get_similarity", &get_similarity);
}