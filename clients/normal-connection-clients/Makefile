# Makefile for UR-RPC Normal Connection Clients

# Build directory
BUILD_DIR = build

# Default target
all: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make

# Create build directory and configure
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install targets
install: all
	cd $(BUILD_DIR) && make install

# Run Client A (requires MQTT broker on localhost:1883)
run-client-a: all
	cd $(BUILD_DIR) && ./client_a client_a_config.json

# Run Client B (requires MQTT broker on localhost:1883)  
run-client-b: all
	cd $(BUILD_DIR) && ./client_b client_b_config.json

# Test build without running
test-build: all
	@echo "Build successful. Executables created:"
	@ls -la $(BUILD_DIR)/client_a $(BUILD_DIR)/client_b
	@echo "Configuration files:"
	@ls -la $(BUILD_DIR)/*.json

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both clients"
	@echo "  clean        - Remove build directory"
	@echo "  install      - Install clients and configs"
	@echo "  run-client-a - Build and run Client A"
	@echo "  run-client-b - Build and run Client B"
	@echo "  test-build   - Build and verify executables"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - MQTT broker running on localhost:1883"
	@echo "  - CMake 3.10+ and build tools installed"

# Phony targets
.PHONY: all clean install run-client-a run-client-b test-build help