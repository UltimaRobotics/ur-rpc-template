#!/usr/bin/make -f

# Simple Makefile wrapper for CMake build

BUILD_DIR = build

.PHONY: all clean test basic ssl conditional setup

all: setup
	cd $(BUILD_DIR) && make -j$(nproc)

setup:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..

basic: setup
	cd $(BUILD_DIR) && make basic_relay_client

ssl: setup
	cd $(BUILD_DIR) && make ssl_relay_client

conditional: setup
	cd $(BUILD_DIR) && make conditional_relay_client

clean:
	rm -rf $(BUILD_DIR)

test: all
	@echo "========================================"
	@echo "  Relay Clients Build Test"
	@echo "========================================"
	@echo "✅ Basic Relay Client: $(shell test -f $(BUILD_DIR)/basic_relay_client && echo "Built" || echo "Missing")"
	@echo "✅ SSL Relay Client: $(shell test -f $(BUILD_DIR)/ssl_relay_client && echo "Built" || echo "Missing")"
	@echo "✅ Conditional Relay Client: $(shell test -f $(BUILD_DIR)/conditional_relay_client && echo "Built" || echo "Missing")"
	@echo "✅ Configuration Files:"
	@echo "   - basic_relay_config.json: $(shell test -f $(BUILD_DIR)/basic_relay_config.json && echo "Ready" || echo "Missing")"
	@echo "   - ssl_relay_config.json: $(shell test -f $(BUILD_DIR)/ssl_relay_config.json && echo "Ready" || echo "Missing")"
	@echo "   - conditional_relay_config.json: $(shell test -f $(BUILD_DIR)/conditional_relay_config.json && echo "Ready" || echo "Missing")"

help:
	@echo "Available targets:"
	@echo "  all         - Build all relay clients"
	@echo "  basic       - Build basic relay client only"
	@echo "  ssl         - Build SSL relay client only"
	@echo "  conditional - Build conditional relay client only"
	@echo "  test        - Build and verify all components"
	@echo "  clean       - Remove build directory"
	@echo "  help        - Show this help message"