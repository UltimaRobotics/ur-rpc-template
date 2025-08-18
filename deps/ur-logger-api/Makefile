# Default build directory
BUILD_DIR = build

CC ?= gcc
CXX ?= g++

# Default target
all: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) .. && make

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install target
install: all
	cd $(BUILD_DIR) && make install

# Phony targets
.PHONY: all clean install 
