# Troubleshooting Guide

## Table of Contents

1. [Build Issues](#build-issues)
2. [OpenMP Problems](#openmp-problems)
3. [Runtime Errors](#runtime-errors)
4. [Performance Issues](#performance-issues)
5. [Platform-Specific Notes](#platform-specific-notes)
6. [FAQ](#faq)

## Build Issues

### Problem: "fftw3.h: No such file or directory"

**Cause**: FFTW3 library not installed or not in include path.

**Solution**:

**Linux (Ubuntu/Debian)**:
```bash
sudo apt-get install libfftw3-dev libfftw3-omp3
```

**Linux (Fedora/RHEL)**:
```bash
sudo dnf install fftw-devel
```

**macOS**:
```bash
brew install fftw
```

**Windows (MSYS2)**:
```bash
pacman -S mingw-w64-x86_64-fftw
```

### Problem: "undefined reference to `omp_get_thread_num`"

**Cause**: OpenMP not enabled or linker flag missing.

**Solution**:

1. Ensure `-fopenmp` flag is in both CFLAGS and LDFLAGS
2. Check Makefile has:
   ```makefile
   CFLAGS += -fopenmp
   LDFLAGS += -fopenmp
   ```

3. For Clang on macOS:
   ```bash
   brew install libomp
   export LDFLAGS="-L/usr/local/opt/libomp/lib"
   export CPPFLAGS="-I/usr/local/opt/libomp/include"
   ```

### Problem: "make: gcc: Command not found"

**Cause**: GCC not installed or not in PATH.

**Solution**:

**Linux**:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# Fedora/RHEL
sudo dnf groupinstall "Development Tools"
```

**macOS**:
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Or install GCC via Homebrew
brew install gcc
```

**Windows**:
- Install MSYS2 from https://www.msys2.org/
- Or use WSL (Windows Subsystem for Linux)

### Problem: Compilation warnings about unused variables

**Cause**: Debug code or incomplete implementation.

**Solution**:

1. These are usually harmless warnings
2. To suppress: add `-Wno-unused-variable` to CFLAGS
3. Or fix by removing unused variables

### Problem: "ld: library not found for -lfftw3"

**Cause**: FFTW3 library installed but linker can't find it.

**Solution**:

1. Find library location:
   ```bash
   # Linux
   ldconfig -p | grep fftw
   
   # macOS
   brew --prefix fftw
   ```

2. Add to linker path:
   ```bash
   export LIBRARY_PATH=/usr/local/lib:$LIBRARY_PATH
   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ```

3. Or specify in Makefile:
   ```makefile
   LDFLAGS += -L/usr/local/lib
   ```

## OpenMP Problems

### Problem: Program runs but doesn't use multiple threads

**Symptoms**:
- `sp_get_num_threads()` returns 1
- No speedup with parallel operations
- `top` shows only one core at 100%

**Diagnosis**:
```c
#ifdef _OPENMP
    printf("OpenMP is enabled\n");
    printf("Max threads: %d\n", omp_get_max_threads());
#else
    printf("OpenMP is NOT enabled\n");
#endif
```

**Solutions**:

1. **Check compilation**:
   ```bash
   gcc -fopenmp -dM -E - < /dev/null | grep -i openmp
   # Should output: #define _OPENMP 201511
   ```

2. **Set thread count**:
   ```bash
   export OMP_NUM_THREADS=4
   ./bin/fso_simulator
   ```

3. **Verify in code**:
   ```c
   omp_set_num_threads(4);
   #pragma omp parallel
   {
       printf("Thread %d of %d\n", 
              omp_get_thread_num(), 
              omp_get_num_threads());
   }
   ```

### Problem: "libgomp.so.1: cannot open shared object file"

**Cause**: OpenMP runtime library not found.

**Solution**:

**Linux**:
```bash
# Ubuntu/Debian
sudo apt-get install libgomp1

# Fedora/RHEL
sudo dnf install libgomp
```

**macOS**:
```bash
brew install libomp
export DYLD_LIBRARY_PATH=/usr/local/opt/libomp/lib:$DYLD_LIBRARY_PATH
```

### Problem: Poor parallel speedup (< 2x with 4 threads)

**Possible Causes**:

1. **Problem size too small**:
   - OpenMP overhead dominates
   - Solution: Increase data size or reduce thread count

2. **Memory bandwidth bottleneck**:
   - All threads competing for memory
   - Solution: Optimize cache usage, reduce memory access

3. **False sharing**:
   - Threads writing to adjacent memory locations
   - Solution: Pad data structures, use thread-local storage

4. **Insufficient parallelism**:
   - Not enough independent work
   - Solution: Increase batch size, use coarser granularity

**Diagnosis**:
```bash
# Profile with perf
perf stat -e cache-misses,cache-references ./bin/fso_benchmark

# Check thread scaling
for threads in 1 2 4 8; do
    export OMP_NUM_THREADS=$threads
    echo "Threads: $threads"
    time ./bin/fso_benchmark
done
```

### Problem: Segmentation fault in parallel region

**Cause**: Race condition or stack overflow.

**Solutions**:

1. **Check for race conditions**:
   ```c
   // Bad: multiple threads writing to same variable
   #pragma omp parallel for
   for (int i = 0; i < n; i++) {
       sum += array[i];  // RACE CONDITION!
   }
   
   // Good: use reduction
   #pragma omp parallel for reduction(+:sum)
   for (int i = 0; i < n; i++) {
       sum += array[i];
   }
   ```

2. **Increase stack size**:
   ```bash
   # Linux
   ulimit -s unlimited
   
   # Or set in code
   export OMP_STACKSIZE=16M
   ```

3. **Use thread sanitizer**:
   ```bash
   gcc -fopenmp -fsanitize=thread -g program.c
   ./a.out
   ```

## Runtime Errors

### Problem: "FSO_ERROR_INVALID_PARAM" when initializing modulator

**Cause**: Invalid parameter values.

**Check**:
- Symbol rate > 0
- PPM order is 2, 4, 8, or 16
- Modulation type is valid enum value

**Example**:
```c
// Bad
modulator_init_ppm(&mod, 1e6, 3);  // 3 is not valid PPM order

// Good
modulator_init_ppm(&mod, 1e6, 4);  // 4-PPM is valid
```

### Problem: "FSO_ERROR_MEMORY" during FEC initialization

**Cause**: Insufficient memory or memory allocation failure.

**Solutions**:

1. **Check available memory**:
   ```bash
   free -h  # Linux
   vm_stat  # macOS
   ```

2. **Reduce code length**:
   ```c
   // Instead of RS(255, 223)
   fec_init(&codec, FEC_REED_SOLOMON, 127, 155, &config);
   ```

3. **Check for memory leaks**:
   ```bash
   valgrind --leak-check=full ./bin/fso_simulator
   ```

### Problem: LDPC decoder not converging

**Symptoms**:
- High BER even at high SNR
- Decoder always hits max iterations
- `stats.iterations == max_iterations`

**Solutions**:

1. **Increase max iterations**:
   ```c
   ldpc_config.max_iterations = 100;  // Default is 50
   ```

2. **Check SNR**:
   - LDPC requires minimum SNR to work
   - Try higher SNR first to verify decoder works

3. **Verify parity check matrix**:
   - Ensure H is valid LDPC matrix
   - Check for proper structure

4. **Adjust convergence threshold**:
   ```c
   ldpc_config.convergence_threshold = 1e-5;  // Less strict
   ```

### Problem: Beam tracker not converging

**Symptoms**:
- `beam_track_is_converged()` always returns 0
- Beam position oscillates
- Signal strength not improving

**Solutions**:

1. **Reduce step size**:
   ```c
   tracker->step_size = 0.0001;  // Smaller steps
   ```

2. **Increase momentum**:
   ```c
   tracker->momentum = 0.95;  // More smoothing
   ```

3. **Check signal map**:
   - Ensure signal measurements are valid
   - Verify map is being updated

4. **Adjust convergence criteria**:
   ```c
   tracker->convergence_threshold = 20;  // More iterations
   tracker->convergence_epsilon = 1e-5;  // Less strict
   ```

### Problem: Channel model produces NaN values

**Cause**: Invalid parameters or numerical overflow.

**Check**:
- Distance > 0 and < 100 km
- Wavelength in valid range (e.g., 1550 nm = 1.55e-6 m)
- C_n² in valid range (1e-17 to 1e-12)

**Debug**:
```c
printf("Rytov variance: %e\n", channel->rytov_variance);
printf("Scintillation index: %e\n", channel->scintillation_index);

if (isnan(channel->rytov_variance)) {
    printf("ERROR: Invalid Rytov variance\n");
    printf("  C_n²: %e\n", channel->cn2);
    printf("  Distance: %f\n", channel->link_distance);
    printf("  Wavelength: %e\n", channel->wavelength);
}
```

## Performance Issues

### Problem: Simulator runs very slowly

**Diagnosis**:

1. **Profile the code**:
   ```bash
   gcc -pg -O2 ...
   ./bin/fso_simulator
   gprof bin/fso_simulator gmon.out > profile.txt
   ```

2. **Check optimization level**:
   ```bash
   # Ensure using -O3
   make clean
   make OPTFLAGS="-O3 -march=native"
   ```

3. **Verify OpenMP is working**:
   ```bash
   export OMP_NUM_THREADS=4
   ./bin/fso_simulator
   # Check CPU usage with top or htop
   ```

**Common Bottlenecks**:

1. **FEC decoding**: Most expensive operation
   - Solution: Use lower code rate or smaller blocks

2. **FFT operations**: Can be slow for large sizes
   - Solution: Use power-of-2 sizes, enable OpenMP

3. **Channel model**: Fading generation
   - Solution: Pre-generate fading samples

### Problem: High memory usage

**Diagnosis**:
```bash
# Monitor memory
top -p $(pgrep fso_simulator)

# Or use valgrind massif
valgrind --tool=massif ./bin/fso_simulator
ms_print massif.out.*
```

**Solutions**:

1. **Reduce buffer sizes**:
   ```c
   sp_init(&sp, 4, 4096);  // Smaller buffer
   ```

2. **Free resources promptly**:
   ```c
   fec_free(&codec);
   modulator_free(&mod);
   channel_free(&channel);
   ```

3. **Use streaming instead of batch processing**

### Problem: Tests fail intermittently

**Cause**: Race conditions or timing-dependent behavior.

**Solutions**:

1. **Run with single thread**:
   ```bash
   export OMP_NUM_THREADS=1
   make tests
   ```

2. **Increase tolerance in tests**:
   ```c
   // Instead of exact equality
   assert(fabs(result - expected) < 1e-6);
   ```

3. **Use thread sanitizer**:
   ```bash
   make clean
   make CC=gcc CFLAGS="-fsanitize=thread -g"
   make tests
   ```

## Platform-Specific Notes

### Linux

**Recommended Setup**:
```bash
# Ubuntu 20.04 or later
sudo apt-get update
sudo apt-get install build-essential libfftw3-dev libfftw3-omp3

# Clone and build
git clone <repo>
cd fso-communication-suite
make -j4
```

**Known Issues**:
- None currently

### macOS

**Recommended Setup**:
```bash
# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install gcc fftw

# Build with GCC (not Apple Clang)
make CC=gcc-13  # Or whatever version brew installed
```

**Known Issues**:

1. **Apple Clang doesn't support OpenMP**:
   - Solution: Use GCC from Homebrew
   - Or install libomp: `brew install libomp`

2. **Library path issues**:
   ```bash
   export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
   ```

3. **M1/M2 Macs (ARM)**:
   - FFTW may need to be built from source for optimal performance
   - Or use Rosetta 2 for x86_64 binaries

### Windows

**Recommended Setup** (MSYS2):
```bash
# Install MSYS2 from https://www.msys2.org/

# In MSYS2 terminal
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-fftw make

# Build
make
```

**Alternative** (WSL):
```bash
# Install WSL2 with Ubuntu
wsl --install

# Then follow Linux instructions
```

**Known Issues**:

1. **Path separators**:
   - Use forward slashes in Makefile
   - Or use WSL for better compatibility

2. **Performance**:
   - Native Windows build may be slower than WSL
   - Consider using WSL2 for better performance

## FAQ

### Q: How do I know if OpenMP is working?

**A**: Run this test:
```c
#include <stdio.h>
#include <omp.h>

int main() {
    #pragma omp parallel
    {
        #pragma omp critical
        printf("Hello from thread %d of %d\n", 
               omp_get_thread_num(), 
               omp_get_num_threads());
    }
    return 0;
}
```

Compile and run:
```bash
gcc -fopenmp test.c -o test
./test
```

You should see multiple threads printing.

### Q: What's the optimal number of threads?

**A**: Generally:
- Use number of physical cores (not hyperthreads)
- Leave 1-2 cores for OS
- Test with different values

Example for 8-core CPU:
```bash
for t in 1 2 4 6 8; do
    export OMP_NUM_THREADS=$t
    echo "Threads: $t"
    time ./bin/fso_benchmark
done
```

### Q: Why is my BER higher than expected?

**A**: Check:
1. SNR calculation is correct
2. FEC is enabled and working
3. Modulation/demodulation is correct
4. Channel model parameters are reasonable
5. No bugs in bit comparison

Debug:
```c
printf("Transmitted bits: ");
for (int i = 0; i < 10; i++) printf("%d", tx_bits[i]);
printf("\n");

printf("Received bits: ");
for (int i = 0; i < 10; i++) printf("%d", rx_bits[i]);
printf("\n");
```

### Q: How do I reduce latency?

**A**: 
1. Use smaller FEC block sizes
2. Reduce interleaver depth
3. Use simpler modulation (OOK)
4. Increase beam tracking update rate
5. Optimize code with -O3

### Q: Can I use this with real hardware?

**A**: The code is designed for simulation, but could be adapted:
1. Replace channel model with actual hardware interface
2. Implement DAC/ADC interfaces
3. Add GPIO control for beam steering
4. Modify timing for real-time constraints

### Q: How accurate is the channel model?

**A**: The models are based on published research:
- Log-normal fading: Widely accepted for weak-to-moderate turbulence
- Weather attenuation: Empirical models (Kim, Carbonneau)
- Limitations: Doesn't model all effects (e.g., beam wander, aperture averaging)

### Q: Why does the simulator use so much CPU?

**A**: FSO simulation is computationally intensive:
- FEC encoding/decoding
- FFT operations
- Channel model calculations
- Beam tracking updates

This is normal. Use OpenMP to parallelize and reduce wall-clock time.

### Q: How do I contribute?

**A**: 
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

See CONTRIBUTING.md for details.

### Q: Where can I learn more about FSO?

**A**: Recommended resources:
- "Free-Space Laser Communications" by Hamid Hemmati
- "Optical Wireless Communications" by Z. Ghassemlooy et al.
- IEEE/OSA journals on optical communications
- ITU-T recommendations for FSO systems

---

If your issue isn't covered here, please open an issue on the project repository with:
- Detailed description of the problem
- Steps to reproduce
- System information (OS, compiler version, etc.)
- Error messages or logs
