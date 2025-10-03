# ============================================================================
# FSO Communication Protocol Suite - Makefile
# ============================================================================

# Compiler and tools
CC = gcc
AR = ar
RANLIB = ranlib

# Directories
SRC_DIR = src
TEST_DIR = tests
SIM_DIR = simulation
BENCH_DIR = benchmarks
BUILD_DIR = build
BIN_DIR = bin
DOC_DIR = docs

# Source subdirectories
MOD_DIR = $(SRC_DIR)/modulation
FEC_DIR = $(SRC_DIR)/fec
BEAM_DIR = $(SRC_DIR)/beam_tracking
SP_DIR = $(SRC_DIR)/signal_processing
TURB_DIR = $(SRC_DIR)/turbulence
UTIL_DIR = $(SRC_DIR)/utils

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -I$(SRC_DIR)
CFLAGS_DEBUG = $(CFLAGS) -g -O0 -DDEBUG
CFLAGS_RELEASE = $(CFLAGS) -O3 -march=native -DNDEBUG

# OpenMP flags
OPENMP_FLAGS = -fopenmp

# Linker flags
LDFLAGS = -lm
LDFLAGS_FFTW = -lfftw3 -lfftw3_omp

# Default to release build
OPTFLAGS ?= $(CFLAGS_RELEASE)

# Combined flags
ALL_CFLAGS = $(OPTFLAGS) $(OPENMP_FLAGS)
ALL_LDFLAGS = $(LDFLAGS) $(LDFLAGS_FFTW) $(OPENMP_FLAGS)

# Library name
LIB_NAME = libfso.a
LIB_PATH = $(BUILD_DIR)/$(LIB_NAME)

# Source files (to be populated as modules are implemented)
MOD_SOURCES = $(wildcard $(MOD_DIR)/*.c)
FEC_SOURCES = $(wildcard $(FEC_DIR)/*.c)
BEAM_SOURCES = $(wildcard $(BEAM_DIR)/*.c)
SP_SOURCES = $(wildcard $(SP_DIR)/*.c)
TURB_SOURCES = $(wildcard $(TURB_DIR)/*.c)
UTIL_SOURCES = $(wildcard $(UTIL_DIR)/*.c)

ALL_SOURCES = $(MOD_SOURCES) $(FEC_SOURCES) $(BEAM_SOURCES) \
              $(SP_SOURCES) $(TURB_SOURCES) $(UTIL_SOURCES)

# Object files
ALL_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ALL_SOURCES))

# Test sources and binaries
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_BINARIES = $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_SOURCES))

# Simulator sources and binary
SIM_SOURCES = $(wildcard $(SIM_DIR)/*.c)
SIM_BINARY = $(BIN_DIR)/fso_simulator

# Benchmark sources and binary
BENCH_SOURCES = $(wildcard $(BENCH_DIR)/*.c)
BENCH_BINARY = $(BIN_DIR)/fso_benchmark

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean library simulator tests benchmarks docs help debug release

# Default target
all: library simulator tests benchmarks

# Help target
help:
	@echo "FSO Communication Protocol Suite - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all         - Build library, simulator, tests, and benchmarks (default)"
	@echo "  library     - Build the FSO library (libfso.a)"
	@echo "  simulator   - Build the Hardware-in-Loop simulator"
	@echo "  tests       - Build and run unit tests"
	@echo "  benchmarks  - Build and run performance benchmarks"
	@echo "  docs        - Generate documentation with Doxygen"
	@echo "  clean       - Remove all build artifacts"
	@echo "  debug       - Build with debug symbols (make debug OPTFLAGS='...')"
	@echo "  release     - Build with optimizations (default)"
	@echo ""
	@echo "Build options:"
	@echo "  OPTFLAGS    - Override optimization flags"
	@echo "              Example: make OPTFLAGS='-O2 -g'"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build everything (release mode)"
	@echo "  make library            # Build only the library"
	@echo "  make debug              # Build with debug symbols"
	@echo "  make clean all          # Clean rebuild"

# Debug build
debug:
	$(MAKE) all OPTFLAGS="$(CFLAGS_DEBUG)"

# Release build (explicit)
release:
	$(MAKE) all OPTFLAGS="$(CFLAGS_RELEASE)"

# ============================================================================
# Library Target
# ============================================================================

library: $(LIB_PATH)

$(LIB_PATH): $(ALL_OBJECTS) | $(BUILD_DIR)
	@echo "Creating static library: $@"
	$(AR) rcs $@ $^
	$(RANLIB) $@
	@echo "Library created successfully"

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	$(CC) $(ALL_CFLAGS) -c $< -o $@

# ============================================================================
# Simulator Target
# ============================================================================

simulator: $(SIM_BINARY)

$(SIM_BINARY): $(SIM_SOURCES) $(LIB_PATH) | $(BIN_DIR)
	@echo "Building simulator: $@"
	$(CC) $(ALL_CFLAGS) $(SIM_SOURCES) -L$(BUILD_DIR) -lfso $(ALL_LDFLAGS) -o $@
	@echo "Simulator built successfully"

# ============================================================================
# Tests Target
# ============================================================================

tests: $(TEST_BINARIES)
	@echo "Running tests..."
	@for test in $(TEST_BINARIES); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(LIB_PATH) | $(BIN_DIR)
	@echo "Building test: $@"
	$(CC) $(ALL_CFLAGS) $< -L$(BUILD_DIR) -lfso $(ALL_LDFLAGS) -o $@

# ============================================================================
# Benchmarks Target
# ============================================================================

benchmarks: $(BENCH_BINARY)

$(BENCH_BINARY): $(BENCH_SOURCES) $(LIB_PATH) | $(BIN_DIR)
	@echo "Building benchmarks: $@"
	$(CC) $(ALL_CFLAGS) $(BENCH_SOURCES) -L$(BUILD_DIR) -lfso $(ALL_LDFLAGS) -o $@
	@echo "Benchmarks built successfully"

# ============================================================================
# Documentation Target
# ============================================================================

docs:
	@echo "Generating documentation..."
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen Doxyfile; \
		echo "Documentation generated in $(DOC_DIR)/html/"; \
	else \
		echo "Error: Doxygen not found. Please install Doxygen to generate documentation."; \
		exit 1; \
	fi

# ============================================================================
# Directory Creation
# ============================================================================

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/modulation
	@mkdir -p $(BUILD_DIR)/fec
	@mkdir -p $(BUILD_DIR)/beam_tracking
	@mkdir -p $(BUILD_DIR)/signal_processing
	@mkdir -p $(BUILD_DIR)/turbulence
	@mkdir -p $(BUILD_DIR)/utils

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# ============================================================================
# Clean Target
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -rf $(DOC_DIR)/html $(DOC_DIR)/latex
	@echo "Clean complete"

# ============================================================================
# Install Target (optional)
# ============================================================================

install: library
	@echo "Installing FSO library..."
	@mkdir -p /usr/local/lib
	@mkdir -p /usr/local/include/fso
	@cp $(LIB_PATH) /usr/local/lib/
	@cp $(SRC_DIR)/fso.h /usr/local/include/fso/
	@echo "Installation complete"

uninstall:
	@echo "Uninstalling FSO library..."
	@rm -f /usr/local/lib/$(LIB_NAME)
	@rm -rf /usr/local/include/fso
	@echo "Uninstall complete"

# ============================================================================
# Dependency tracking
# ============================================================================

-include $(ALL_OBJECTS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(ALL_CFLAGS) -MM -MT $(patsubst %.d,%.o,$@) $< > $@
