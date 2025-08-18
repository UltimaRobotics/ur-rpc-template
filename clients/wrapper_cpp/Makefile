# Simple Makefile for C++ wrapper clients
# Uses CMake for actual building

BUILD_DIR = build
CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Release

.PHONY: all clean configure build test install

all: configure build

configure:
	@echo "Configuring C++ wrapper clients..."
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. $(CMAKE_FLAGS)

build: configure
	@echo "Building C++ wrapper clients..."
	cd $(BUILD_DIR) && make -j4

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

test: build
	@echo "Running C++ wrapper client tests..."
	cd $(BUILD_DIR) && ./cpp_basic_client cpp_basic_config.json &
	sleep 2
	cd $(BUILD_DIR) && ./cpp_bidirectional_client cpp_bidirectional_config.json &
	sleep 2
	@echo "Test clients started - check logs for output"

install: build
	@echo "Installing C++ wrapper clients..."
	cd $(BUILD_DIR) && make install

# Individual client targets
cpp_basic_client: configure
	cd $(BUILD_DIR) && make cpp_basic_client

cpp_bidirectional_client: configure
	cd $(BUILD_DIR) && make cpp_bidirectional_client

cpp_ssl_client: configure
	cd $(BUILD_DIR) && make cpp_ssl_client

cpp_relay_client: configure
	cd $(BUILD_DIR) && make cpp_relay_client

cpp_advanced_features: configure
	cd $(BUILD_DIR) && make cpp_advanced_features

help:
	@echo "Available targets:"
	@echo "  all                    - Configure and build all clients"
	@echo "  configure              - Configure build with CMake"
	@echo "  build                  - Build all clients"
	@echo "  clean                  - Remove build directory"
	@echo "  test                   - Build and run test clients"
	@echo "  install                - Install built clients"
	@echo "  cpp_basic_client       - Build basic client only"
	@echo "  cpp_bidirectional_client - Build bidirectional client only"
	@echo "  cpp_ssl_client         - Build SSL client only"
	@echo "  cpp_relay_client       - Build relay client only"
	@echo "  cpp_advanced_features  - Build advanced features client only"
	@echo "  help                   - Show this help message"