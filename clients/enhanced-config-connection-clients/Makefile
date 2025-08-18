# Enhanced SSL/TLS Clients Makefile

.PHONY: all clean build enhanced_client_a enhanced_client_b

# Default target
all: build enhanced_client_a enhanced_client_b

# Create build directory and run cmake
build:
	@echo "Setting up build environment for enhanced SSL clients..."
	@rm -rf build
	@mkdir -p build
	@cd build && cmake .. && make

# Individual client targets  
enhanced_client_a: build
	@echo "Enhanced Client A built successfully"

enhanced_client_b: build
	@echo "Enhanced Client B built successfully"

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf build

# Install SSL certificates to build directory
install-certs:
	@echo "Installing SSL certificates..."
	@mkdir -p build/ssl
	@cp /tmp/ca.crt build/ssl/
	@cp /tmp/server.crt build/ssl/
	@echo "SSL certificates installed to build/ssl/"

# Test targets
test-a:
	@cd build && timeout 10s ./enhanced_client_a enhanced_client_a_config.json

test-b:
	@cd build && timeout 10s ./enhanced_client_b enhanced_client_b_config.json

test-bidirectional:
	@echo "Starting enhanced SSL bidirectional test..."
	@cd build && timeout 15s ./enhanced_client_a enhanced_client_a_config.json &
	@sleep 2
	@cd build && timeout 13s ./enhanced_client_b enhanced_client_b_config.json