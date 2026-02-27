CXX      := g++
# Añadimos -s para eliminar tablas de símbolos durante la compilación
# Añadimos -ffunction-sections -fdata-sections para optimizar espacio
CXXFLAGS := -std=c++17 -Wall -Wextra -O3 -I/usr/local/include -I/usr/include \
            -DCROW_ENABLE_SSL -ffunction-sections -fdata-sections

# --- LDFLAGS ---
# Añadimos --gc-sections para que el linker elimine código muerto de las librerías estáticas
LDFLAGS  := -static -static-libgcc -static-libstdc++ \
            -Wl,--gc-sections \
            -s \
            -lssl -lcrypto \
            -lsqlite3 \
            -lz \
            -lpthread -ldl

SRC_DIR   := .
BUILD_DIR := ../build
OBJ_DIR   := $(BUILD_DIR)/obj

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TARGET := $(BUILD_DIR)/app

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR): $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(wildcard $(SRC_DIR)/*.hpp) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@strip --strip-unneeded $@
	@echo "-------------------------------------------------------"
	@echo "✓ Binario optimizado creado en: $(TARGET)"
	@echo "✓ Tamaño reducido mediante stripping y gc-sections"
	@echo "✓ Soporte SSL y SQLite: Activado"
	@echo "-------------------------------------------------------"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean