CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I/usr/local/include
# Añadimos -lsqlite3 y -ldl (necesario para SQLite en sistemas POSIX)
# El orden de las librerías importa: las dependencias van después de los objetos
LDFLAGS := -static -static-libgcc -static-libstdc++ \
           -lsqlite3 -ldl -lpthread -lssl -lcrypto -lz -s

SRC_DIR := .
BUILD_DIR := ../build
OBJ_DIR := $(BUILD_DIR)/obj

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TARGET := $(BUILD_DIR)/app

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR): $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✓ Binary compiled successfully (SQLite Static): $(TARGET)"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean