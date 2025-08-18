# C Logging API

A comprehensive, thread-safe logging library for C applications with multiple log levels, configurable output options, and predefined warning flags.

## Features

- **Multiple Log Levels**: DEBUG, INFO, WARN, ERROR, FATAL
- **Predefined Warning Flags**: Easy configuration with bitwise flags
- **Thread-Safe Operations**: Uses pthread mutexes for thread safety
- **Dual Output**: Console and file logging capabilities
- **Configurable Formatting**: Timestamps, thread IDs, and color output
- **Performance Optimized**: Minimal overhead with compile-time and runtime filtering
- **Cross-Platform**: Standard C with POSIX threading support

## Quick Start

### Building

```bash
# Build everything
make all

# Run the demonstration
make run

# Build just the library
make liblogger.a

# Run with debug information
make debug
