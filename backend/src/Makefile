CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I/usr/local/include
LDFLAGS := -static -static-libgcc -static-libstdc++ -lpthread -lssl -lcrypto -lz -s

SRC_DIR := .
BUILD_DIR := ../build
OBJ_DIR := $(BUILD_DIR)/obj

# --- ESTA ES LA PARTE QUE FALTABA ---
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
# -------------------------------------

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
	@echo "✓ Binary compiled successfully: $(TARGET)"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean