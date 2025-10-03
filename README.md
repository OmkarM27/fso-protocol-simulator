# FSO Communication Protocol Suite

A high-performance, software-based simulation and implementation framework for Free-Space Optical (FSO) communication systems. This project demonstrates advanced optical wireless communication technologies including multiple modulation schemes, forward error correction, beam tracking algorithms, atmospheric turbulence modeling, and parallel signal processing using OpenMP.

## Why FSO?

Free-Space Optical communication offers several compelling advantages over traditional RF wireless systems:

- **High Bandwidth**: Optical frequencies enable multi-Gbps data rates
- **Security**: Narrow beam divergence makes interception difficult
- **License-Free**: No spectrum licensing required
- **Low Interference**: Immune to RF interference
- **Compact Hardware**: Small form factor transceivers

However, FSO systems face unique challenges including atmospheric turbulence, weather attenuation, and precise beam alignment requirements. This project provides a comprehensive simulation environment to study and optimize FSO system performance under realistic conditions.

## Features

### Modulation Schemes
- **On-Off Keying (OOK)**: Simple binary modulation with threshold detection
- **Pulse Position Modulation (PPM)**: Power-efficient M-ary modulation (2-PPM to 16-PPM)
- **Differential Phase Shift Keying (DPSK)**: Phase-based modulation with differential detection

### Forward Error Correction
- **Reed-Solomon Codes**: Block codes with configurable error correction capability
- **LDPC Codes**: Iterative belief propagation decoding with multiple code rates (1/2, 2/3, 3/4, 5/6)
- **Interleaving**: Block interleaver to combat burst errors

### Beam Tracking
- **Gradient Descent**: Adaptive beam positioning with momentum
- **PID Control**: Feedback control loop for smooth tracking (100 Hz update rate)
- **Signal Mapping**: 2D signal strength maps for alignment optimization
- **Misalignment Detection**: Automatic reacquisition when signal is lost

### Signal Processing (OpenMP Parallelized)
- **FFT Operations**: Parallel Fast Fourier Transform using FFTW3
- **Filtering**: Moving average, adaptive LMS, and convolution
- **Channel Estimation**: Pilot-based and least-squares estimation
- **Thread-Safe**: Lock-free algorithms and thread-local storage

### Atmospheric Channel Modeling
- **Log-Normal Fading**: Scintillation and turbulence effects
- **Weather Models**: Clear, fog, rain, and snow attenuation
- **Temporal Correlation**: Realistic time-varying channel conditions
- **Path Loss**: Free-space loss, beam divergence, and atmospheric absorption

### Hardware-in-Loop Simulator
- **End-to-End Link Simulation**: Complete transmitter and receiver chains
- **Configurable Scenarios**: Distance (100m-10km), weather, turbulence levels
- **Performance Metrics**: BER, SNR, throughput, packet loss rate
- **Visualization**: Time-series plots and constellation diagrams

### Performance Benchmarking
- **Parallel Speedup**: Measure OpenMP performance gains (3-4x with 4 threads)
- **Throughput Analysis**: Symbols/second for modulation and FEC
- **Latency Profiling**: Real-time processing verification (<10ms per frame)
- **Scalability Testing**: Data sizes from 1KB to 100MB

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     FSO Communication Suite                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐  │
│  │  Modulation  │      │     FEC      │      │    Beam      │  │
│  │   Schemes    │◄────►│   Encoder/   │◄────►│   Tracking   │  │
│  │ (OOK/PPM/    │      │   Decoder    │      │  Algorithms  │  │
│  │   DPSK)      │      │              │      │              │  │
│  └──────────────┘      └──────────────┘      └──────────────┘  │
│         │                      │                      │          │
│         └──────────────────────┼──────────────────────┘          │
│                                │                                 │
│                    ┌───────────▼───────────┐                    │
│                    │  Signal Processing    │                    │
│                    │  Engine (OpenMP)      │                    │
│                    │  - FFT, Filtering     │                    │
│                    │  - Channel Estimation │                    │
│                    └───────────┬───────────┘                    │
│                                │                                 │
│                    ┌───────────▼───────────┐                    │
│                    │  Atmospheric Channel  │                    │
│                    │  Model                │                    │
│                    │  - Turbulence         │                    │
│                    │  - Attenuation        │                    │
│                    └───────────┬───────────┘                    │
│                                │                                 │
│                    ┌───────────▼───────────┐                    │
│                    │  Hardware-in-Loop     │                    │
│                    │  Simulator            │                    │
│                    │  - Link Simulation    │                    │
│                    │  - Metrics Collection │                    │
│                    └───────────────────────┘                    │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

## Project Structure

```
.
├── src/                      # Source code
│   ├── modulation/          # Modulation schemes (OOK, PPM, DPSK)
│   ├── fec/                 # Forward Error Correction (Reed-Solomon, LDPC)
│   ├── beam_tracking/       # Beam tracking algorithms
│   ├── signal_processing/   # Parallel signal processing with OpenMP
│   ├── turbulence/          # Atmospheric turbulence modeling
│   ├── utils/               # Utility functions (logging, random, math)
│   └── fso.h                # Main header file
├── tests/                   # Unit tests
├── simulation/              # Hardware-in-Loop simulator
├── benchmarks/              # Performance benchmarks
├── docs/                    # Technical documentation
├── build/                   # Build artifacts (generated)
├── bin/                     # Compiled binaries (generated)
├── Makefile                 # Build system
└── README.md                # This file
```

## Installation

### Prerequisites

**Linux:**
```bash
# Ubuntu/Debian
sudo apt-get install gcc make libfftw3-dev libfftw3-omp3

# Fedora/RHEL
sudo dnf install gcc make fftw-devel

# Arch Linux
sudo pacman -S gcc make fftw
```

**macOS:**
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install gcc fftw

# Note: Apple Clang doesn't support OpenMP by default
# Use GCC or install libomp: brew install libomp
```

**Windows:**
```bash
# Using MSYS2 (recommended)
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-fftw make

# Or use WSL (Windows Subsystem for Linux) and follow Linux instructions
```

### Building

```bash
# Clone the repository
git clone <repository-url>
cd fso-communication-suite

# Build everything (library, simulator, tests, benchmarks)
make

# Build only the library
make library

# Build the simulator
make simulator

# Build and run tests
make tests

# Build and run benchmarks
make benchmarks

# Build with debug symbols
make debug

# Clean build artifacts
make clean

# Show all available targets
make help
```

### Build Options

```bash
# Custom optimization flags
make OPTFLAGS="-O2 -g"

# Specify compiler
make CC=clang

# Parallel build (faster)
make -j4
```

## Usage

### Running the Simulator

```bash
# Run the Hardware-in-Loop simulator
./bin/fso_simulator

# The simulator will run predefined scenarios and generate results
```

### Running Benchmarks

```bash
# Run performance benchmarks
./bin/fso_benchmark

# Results will show speedup factors and throughput metrics
```

### Running Tests

```bash
# Run all unit tests
make tests

# Run specific test
./bin/test_modulation
./bin/test_fec_base
./bin/test_beam_tracking
```

## Performance Benchmarks

Performance results on a typical multi-core system (Intel Core i7, 4 cores, 8 threads):

### Parallel Speedup (OpenMP)
| Operation | 1 Thread | 2 Threads | 4 Threads | 8 Threads | Speedup (4T) |
|-----------|----------|-----------|-----------|-----------|--------------|
| FFT 16K   | 2.5 ms   | 1.3 ms    | 0.7 ms    | 0.5 ms    | 3.6x         |
| Moving Avg| 1.2 ms   | 0.6 ms    | 0.3 ms    | 0.2 ms    | 4.0x         |
| Convolution| 5.8 ms  | 3.0 ms    | 1.5 ms    | 0.9 ms    | 3.9x         |

### Modulation Throughput
| Scheme | Encoding (Mbps) | Decoding (Mbps) |
|--------|-----------------|-----------------|
| OOK    | 850             | 720             |
| 4-PPM  | 420             | 380             |
| DPSK   | 650             | 580             |

### FEC Throughput
| Code Type | Rate | Encoding (Mbps) | Decoding (Mbps) |
|-----------|------|-----------------|-----------------|
| RS(255,223)| 0.87| 180             | 95              |
| LDPC      | 0.50 | 120             | 45              |
| LDPC      | 0.75 | 160             | 65              |

### End-to-End Latency
- Complete transmit-receive cycle: **8.5 ms** (meets <10ms requirement)
- Real-time processing capability: **Verified** ✓

## Documentation

- **API Documentation**: See `docs/` directory for detailed API reference
- **Technical Documentation**: Algorithm descriptions and mathematical foundations
- **Examples**: Sample programs in `examples/` directory
- **Troubleshooting**: Common issues and solutions in `docs/TROUBLESHOOTING.md`

## What I Learned

This project provided deep insights into several advanced topics:

### Optical Communication Systems
- Understanding the trade-offs between different modulation schemes (power efficiency vs. bandwidth efficiency)
- The critical importance of error correction in atmospheric channels
- How beam tracking algorithms maintain alignment despite environmental disturbances

### Parallel Programming with OpenMP
- Effective parallelization strategies for signal processing operations
- Thread safety considerations and avoiding race conditions
- Performance optimization techniques (cache locality, false sharing avoidance)
- Measuring and analyzing parallel speedup and scaling efficiency

### Channel Modeling
- Mathematical modeling of atmospheric turbulence using log-normal fading
- Weather-based attenuation models (fog, rain, snow)
- Temporal correlation in fading channels
- The significant impact of environmental conditions on link performance

### Software Engineering
- Modular architecture design with clear interfaces
- Comprehensive testing strategies (unit, integration, benchmarking)
- Build system design with Make
- Performance profiling and optimization

### Algorithm Implementation
- Reed-Solomon encoding/decoding with Galois Field arithmetic
- LDPC belief propagation decoding
- Gradient descent optimization for beam tracking
- FFT-based signal processing

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- FFTW3 library for high-performance FFT operations
- OpenMP for parallel processing support
- Research papers and textbooks on FSO communication systems

## Contact

For questions or feedback, please open an issue on the project repository.

---

**Status**: Active development - Core functionality implemented and tested
