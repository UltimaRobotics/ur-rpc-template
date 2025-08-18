# Direct Messaging Clients Makefile
# Uses CMake for building the ur-rpc-template-based direct messaging applications

.PHONY: all clean setup test-standard test-queued help

BUILD_DIR = build
CMAKE = cmake
MAKE = make

# Default target
all: setup build

# Setup build environment
setup:
	@echo "Setting up build environment for direct messaging clients..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) ..

# Build all clients
build:
	@echo "Building direct messaging clients..."
	@cd $(BUILD_DIR) && $(MAKE)
	@echo "Direct messaging clients built successfully"
	@echo "Standard Request-Response clients: client_requester, client_responder"
	@echo "Queued messaging clients: queued_client_1, queued_client_2"

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

# Test standard request-response messaging
test-standard:
	@echo "=== Testing Standard Request-Response Messaging ==="
	@echo "Starting responder client (will run in background)..."
	@cd $(BUILD_DIR) && timeout 30s ./client_responder responder_config.json &
	@sleep 2
	@echo "Starting requester client..."
	@cd $(BUILD_DIR) && timeout 25s ./client_requester requester_config.json
	@echo "Standard messaging test completed"

# Test queued messaging
test-queued:
	@echo "=== Testing Queued Direct Messaging ==="
	@echo "Starting queued client 2 (processor - will run in background)..."
	@cd $(BUILD_DIR) && timeout 120s ./queued_client_2 queued_client_2_config.json &
	@sleep 2
	@echo "Starting queued client 1 (sequential requester)..."
	@cd $(BUILD_DIR) && timeout 100s ./queued_client_1 queued_client_1_config.json
	@echo "Queued messaging test completed"

# Show help
help:
	@echo "Direct Messaging Clients Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all           - Setup and build all clients (default)"
	@echo "  setup         - Setup CMake build environment"
	@echo "  build         - Build all direct messaging clients"
	@echo "  clean         - Clean build directory"
	@echo "  test-standard - Test standard request-response messaging"
	@echo "  test-queued   - Test queued direct messaging"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Manual usage examples:"
	@echo "  Standard messaging:"
	@echo "    ./build/client_responder responder_config.json &"
	@echo "    ./build/client_requester requester_config.json"
	@echo ""
	@echo "  Queued messaging:"
	@echo "    ./build/queued_client_2 queued_client_2_config.json &"
	@echo "    ./build/queued_client_1 queued_client_1_config.json"