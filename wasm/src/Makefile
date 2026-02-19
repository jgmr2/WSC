# Nombre del compilador
CC = emcc

# Directorios
SRC_DIR = .
OUT_DIR = ../dist

# Apuntamos a la carpeta donde se compiló libharu en la Etapa 1
# Basado en tu Dockerfile: /app/libharu/build/src/libhpdf.a
LIBHARU_PATH = /app/libharu
INCLUDES = -I$(LIBHARU_PATH)/include -I$(LIBHARU_PATH)/build/include
LIBHPDF_A = $(LIBHARU_PATH)/build/src/libhpdf.a

EM_FLAGS = -O3 \
           --bind \
           $(INCLUDES) \
           -s WASM=1 \
           -s MODULARIZE=1 \
           -s EXPORT_ES6=1 \
           -s ALLOW_MEMORY_GROWTH=1 \
           -s FORCE_FILESYSTEM=1 \
           -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap','FS']" \
           -s NO_EXIT_RUNTIME=1 \
           -s ENVIRONMENT='web' \
           -s USE_ZLIB=1 \
           -s USE_LIBPNG=1

# Buscar todos los .cpp
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
TARGETS = $(patsubst $(SRC_DIR)/%.cpp, $(OUT_DIR)/%.js, $(SOURCES))

.PHONY: all clean

all: $(TARGETS)

# Regla de compilación
$(OUT_DIR)/%.js: $(SRC_DIR)/%.cpp
	@mkdir -p $(OUT_DIR)
	$(CC) $< $(LIBHPDF_A) $(EM_FLAGS) -o $@

clean:
	rm -rf $(OUT_DIR)