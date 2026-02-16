# Nombre del archivo de salida
TARGET = logic.js

# Compilador de Emscripten
CC = emcc

# Fuentes
SOURCES = main.cpp

# Flags de optimización y configuración de WASM
# -O3: Optimización máxima de tamaño/rendimiento
# -s WASM=1: Indica que queremos generar un binario .wasm
# -s EXPORTED_RUNTIME_METHODS: Permite llamar funciones desde JS
# -s MODULARIZE=1: Envuelve el código en una función para cargarlo fácilmente
# -s EXPORT_ES6=1: Genera un módulo compatible con Svelte/Vite
LDFLAGS = -O3 \
          -s WASM=1 \
          -s MODULARIZE=1 \
          -s EXPORT_ES6=1 \
          -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap']" \
          -s ALLOW_MEMORY_GROWTH=1

# Regla principal
all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p ../dist
	$(CC) $(SOURCES) -o ../dist/$(TARGET) $(LDFLAGS)

clean:
	rm -rf ../dist