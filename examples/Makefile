# Makefile for FSO Communication Suite Examples

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I../src
LDFLAGS = -L../build -lfso -lm -lfftw3 -lfftw3_omp -fopenmp
OPTFLAGS = -O2

# Combined flags
ALL_CFLAGS = $(CFLAGS) $(OPTFLAGS)
ALL_LDFLAGS = $(LDFLAGS)

# Example programs
EXAMPLES = simple_ook_link ppm_comparison beam_tracking_demo parallel_benchmark custom_channel

# Default target
all: $(EXAMPLES)

# Individual targets
simple_ook_link: simple_ook_link.c
	@echo "Building simple_ook_link..."
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

ppm_comparison: ppm_comparison.c
	@echo "Building ppm_comparison..."
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

beam_tracking_demo: beam_tracking_demo.c
	@echo "Building beam_tracking_demo..."
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

parallel_benchmark: parallel_benchmark.c
	@echo "Building parallel_benchmark..."
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

custom_channel: custom_channel.c
	@echo "Building custom_channel..."
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

# Clean target
clean:
	@echo "Cleaning examples..."
	@rm -f $(EXAMPLES)
	@echo "Clean complete"

# Help target
help:
	@echo "FSO Communication Suite - Examples Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all                - Build all examples (default)"
	@echo "  simple_ook_link    - Build simple OOK link example"
	@echo "  ppm_comparison     - Build PPM comparison example"
	@echo "  beam_tracking_demo - Build beam tracking demo"
	@echo "  parallel_benchmark - Build parallel benchmark"
	@echo "  custom_channel     - Build custom channel example"
	@echo "  clean              - Remove all compiled examples"
	@echo "  help               - Show this help message"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - FSO library must be built first: cd .. && make library"
	@echo "  - FFTW3 library with OpenMP support"
	@echo "  - GCC with OpenMP support"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build all examples"
	@echo "  make clean        # Clean up"
	@echo "  ./simple_ook_link # Run an example"

.PHONY: all clean help
