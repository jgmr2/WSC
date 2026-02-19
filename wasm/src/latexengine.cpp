#include <emscripten.h>
#include <emscripten/bind.h>
#include <vector>
#include <string>
#include <iostream>
#include <regex>
#include <hpdf.h>
#include <sstream>  // <--- ESTA ES LA QUE FALTA


void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    throw std::runtime_error("libHaru error");
}

namespace LatexEngine {

    // Función auxiliar para extraer contenido entre llaves: \comando{contenido}
    std::string extract_content(const std::string& code, size_t pos) {
        size_t start = code.find("{", pos);
        size_t end = code.find("}", start);
        if (start != std::string::npos && end != std::string::npos) {
            return code.substr(start + 1, end - start - 1);
        }
        return "";
    }

    std::vector<unsigned char> compileLatex(std::string latex_code) {
        HPDF_Doc pdf = HPDF_New(error_handler, NULL);
        HPDF_Page page = HPDF_AddPage(pdf);
        HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_LETTER, HPDF_PAGE_PORTRAIT);

        HPDF_Font font_bold = HPDF_GetFont(pdf, "Helvetica-Bold", NULL);
        HPDF_Font font_reg = HPDF_GetFont(pdf, "Helvetica", NULL);

        float current_y = 750; // Cursor vertical
        const float margin_x = 50;

        // --- EL PARSER ---
        // Dividimos el código por líneas para procesarlas una a una
        std::stringstream ss(latex_code);
        std::string line;

        while (std::getline(ss, line)) {
            if (line.empty() || line.find("\\documentclass") != std::string::npos || 
                line.find("\\begin{document}") != std::string::npos || 
                line.find("\\end{document}") != std::string::npos) {
                continue;
            }

            // Detectar \section{...}
            if (line.find("\\section{") != std::string::npos) {
                std::string content = extract_content(line, line.find("\\section"));
                
                HPDF_Page_BeginText(page);
                HPDF_Page_SetFontAndSize(page, font_bold, 18);
                HPDF_Page_MoveTextPos(page, margin_x, current_y);
                HPDF_Page_ShowText(page, content.c_str());
                HPDF_Page_EndText(page);
                
                current_y -= 30; // Salto de línea grande para sección
            }
            // Detectar \subsection{...}
            else if (line.find("\\subsection{") != std::string::npos) {
                std::string content = extract_content(line, line.find("\\subsection"));
                
                HPDF_Page_BeginText(page);
                HPDF_Page_SetFontAndSize(page, font_bold, 14);
                HPDF_Page_MoveTextPos(page, margin_x, current_y);
                HPDF_Page_ShowText(page, content.c_str());
                HPDF_Page_EndText(page);
                
                current_y -= 25;
            }
            // Texto normal
            else {
                HPDF_Page_BeginText(page);
                HPDF_Page_SetFontAndSize(page, font_reg, 11);
                HPDF_Page_MoveTextPos(page, margin_x, current_y);
                HPDF_Page_ShowText(page, line.c_str());
                HPDF_Page_EndText(page);
                
                current_y -= 15; // Salto de línea normal
            }

            // Control de fin de página simple
            if (current_y < 50) break; 
        }

        // Finalización del stream
        HPDF_SaveToStream(pdf);
        HPDF_UINT32 stream_size = HPDF_GetStreamSize(pdf);
        std::vector<unsigned char> output(stream_size);
        HPDF_ReadFromStream(pdf, output.data(), &stream_size);
        HPDF_Free(pdf);

        return output;
    }
}

EMSCRIPTEN_BINDINGS(latex_engine_module) {
    emscripten::register_vector<unsigned char>("VectorUChar");
    emscripten::function("compileLatex", &LatexEngine::compileLatex);
}