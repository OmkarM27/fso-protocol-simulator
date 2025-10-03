# FSO Communication Protocol Suite - Technical Overview

## Table of Contents

1. [Introduction](#introduction)
2. [Modulation Schemes](#modulation-schemes)
3. [Forward Error Correction](#forward-error-correction)
4. [Beam Tracking Algorithms](#beam-tracking-algorithms)
5. [Atmospheric Channel Models](#atmospheric-channel-models)
6. [Signal Processing](#signal-processing)
7. [Configuration Parameters](#configuration-parameters)
8. [Performance Tuning](#performance-tuning)

## Introduction

This document provides detailed technical information about the algorithms, mathematical foundations, and implementation details of the FSO Communication Protocol Suite.

## Modulation Schemes

### On-Off Keying (OOK)

**Description**: Binary modulation where '1' is represented by light on and '0' by light off.

**Mathematical Model**:
```
s(t) = {
    A,  for bit = 1
    0,  for bit = 0
}
```

**Modulation**:
- Each bit is mapped directly to a symbol
- Symbol rate = bit rate
- Spectral efficiency: 1 bit/symbol

**Demodulation**:
- Threshold detection: bit = (received > threshold) ? 1 : 0
- Optimal threshold (AWGN channel): `threshold = A/2`
- Adaptive threshold based on SNR:
  ```
  threshold = A * (1 / (1 + 10^(-SNR_dB/10)))
  ```

**Trade-offs**:
- **Advantages**: Simple implementation, low complexity
- **Disadvantages**: Not power-efficient, susceptible to intensity noise
- **Best for**: High SNR scenarios, simple systems

**Bit Error Rate (BER)**:
```
BER = 0.5 * erfc(sqrt(SNR/2))
```
where SNR is the signal-to-noise ratio (linear scale).

### Pulse Position Modulation (PPM)

**Description**: M-ary modulation where the position of a pulse within M time slots encodes log₂(M) bits.

**Mathematical Model**:
```
For M-PPM, symbol duration T_s is divided into M slots
Symbol k (0 ≤ k < M) has pulse in slot k:
s_k(t) = A * rect((t - k*T_slot)/T_slot)
```

**Modulation**:
- Group log₂(M) bits to form symbol value k
- Place pulse in slot k of M slots
- Symbol rate = bit rate / log₂(M)
- Spectral efficiency: log₂(M) bits/symbol

**Demodulation**:
- Maximum likelihood detection: find slot with maximum energy
- Decision: k = argmax_i(∫ r(t) * s_i(t) dt)

**PPM Orders**:
- 2-PPM: 1 bit/symbol, 2 slots
- 4-PPM: 2 bits/symbol, 4 slots
- 8-PPM: 3 bits/symbol, 8 slots
- 16-PPM: 4 bits/symbol, 16 slots

**Trade-offs**:
- **Advantages**: Power-efficient, good for photon-starved channels
- **Disadvantages**: Bandwidth-inefficient, requires slot synchronization
- **Best for**: Deep space communications, low-power applications

**Symbol Error Rate (SER)**:
```
SER ≈ (M-1)/2 * erfc(sqrt(log₂(M) * SNR/2))
```

**Bandwidth Expansion**:
- PPM requires M times more bandwidth than OOK for same data rate
- Trade bandwidth for power efficiency

### Differential Phase Shift Keying (DPSK)

**Description**: Phase-based modulation where information is encoded in the phase difference between consecutive symbols.

**Mathematical Model**:
```
s(t) = A * exp(j * φ(t))
φ(t) = φ(t-T) + Δφ(bit)

For binary DPSK:
Δφ = {
    0,    for bit = 0
    π,    for bit = 1
}
```

**Modulation**:
- Maintain phase state φ_prev
- For each bit: φ_new = φ_prev + (bit ? π : 0)
- Transmit: s(t) = A * exp(j * φ_new)
- Update: φ_prev = φ_new

**Demodulation**:
- Differential detection: multiply received symbol by conjugate of previous symbol
- Decision based on real part: bit = (Re(r(t) * r*(t-T)) < 0) ? 1 : 0
- No need for absolute phase reference (advantage over coherent PSK)

**Trade-offs**:
- **Advantages**: No carrier phase recovery needed, robust to phase noise
- **Disadvantages**: 3 dB penalty compared to coherent PSK
- **Best for**: Channels with phase instability, simpler receivers

**Bit Error Rate (BER)**:
```
BER = 0.5 * exp(-SNR)
```

**Phase Tracking**:
- Maintain phase continuity across symbols
- Handle phase wrapping (±π boundaries)
- Initial phase can be arbitrary

## Forward Error Correction

### Reed-Solomon Codes

**Description**: Non-binary cyclic error-correcting codes that operate on symbols (typically 8-bit bytes).

**Mathematical Foundation**:

Reed-Solomon codes are defined over Galois Field GF(2^m), typically GF(256) for m=8.

**Code Parameters**:
- n: Code length (total symbols)
- k: Information symbols
- t: Error correction capability
- Relationship: n - k = 2t (number of parity symbols)

**Standard RS(255, 223)**:
- 255 total symbols
- 223 information symbols
- 32 parity symbols
- Can correct up to 16 symbol errors
- Code rate: 223/255 ≈ 0.875

**Encoding Algorithm**:

1. **Generator Polynomial**:
   ```
   g(x) = (x - α^0)(x - α^1)...(x - α^(2t-1))
   ```
   where α is a primitive element of GF(256)

2. **Systematic Encoding**:
   ```
   c(x) = m(x) * x^(2t) + r(x)
   ```
   where r(x) = m(x) * x^(2t) mod g(x)

3. **Implementation**:
   - Multiply message polynomial by x^(2t) (shift left)
   - Divide by generator polynomial
   - Remainder is parity symbols
   - Append parity to message

**Decoding Algorithm** (Berlekamp-Massey):

1. **Syndrome Calculation**:
   ```
   S_i = r(α^i) for i = 0, 1, ..., 2t-1
   ```
   If all syndromes are zero, no errors detected

2. **Error Locator Polynomial** (Berlekamp-Massey):
   - Iteratively compute Λ(x) from syndromes
   - Degree of Λ(x) = number of errors

3. **Error Location** (Chien Search):
   - Evaluate Λ(α^(-i)) for all i
   - Roots indicate error positions

4. **Error Value** (Forney Algorithm):
   ```
   e_i = -S(α^i) / Λ'(α^i)
   ```

5. **Correction**:
   - Add error values to received symbols at error locations

**Performance**:
- Can correct up to t symbol errors
- Can detect up to 2t symbol erasures
- Effective against burst errors when combined with interleaving

**Complexity**:
- Encoding: O(n * t)
- Decoding: O(n * t²) for Berlekamp-Massey

### LDPC Codes

**Description**: Sparse parity-check codes decoded using iterative belief propagation.

**Mathematical Foundation**:

LDPC codes are defined by a sparse parity-check matrix H where:
```
H * c^T = 0 (mod 2)
```
for all valid codewords c.

**Code Structure**:
- Represented as bipartite graph (Tanner graph)
- Variable nodes (code bits)
- Check nodes (parity checks)
- Edges connect variables to checks

**Code Rates**:
- Rate 1/2: 50% redundancy
- Rate 2/3: 33% redundancy
- Rate 3/4: 25% redundancy
- Rate 5/6: 17% redundancy

**Encoding**:
- Systematic encoding using generator matrix G
- c = m * G where m is message
- For structured LDPC, can use efficient encoding

**Decoding Algorithm** (Sum-Product / Belief Propagation):

1. **Initialization**:
   ```
   L_n = log(P(bit=0) / P(bit=1))  // Log-likelihood ratio
   ```

2. **Variable Node Update**:
   ```
   L_n→m = L_n + Σ(L_m'→n) for all m' ≠ m
   ```

3. **Check Node Update**:
   ```
   L_m→n = 2 * atanh(∏ tanh(L_n'→m / 2)) for all n' ≠ n
   ```

4. **Decision**:
   ```
   L_n_total = L_n + Σ(L_m→n)
   bit_n = (L_n_total < 0) ? 1 : 0
   ```

5. **Convergence Check**:
   - Check if H * c^T = 0
   - Or maximum iterations reached

**Parameters**:
- Max iterations: typically 50-100
- Convergence threshold: 1e-6
- Early stopping when parity checks satisfied

**Performance**:
- Near Shannon limit performance
- Excellent for AWGN channels
- Iterative decoding allows soft decisions

**Complexity**:
- Decoding: O(iterations * edges) where edges << n²
- Much lower than ML decoding

### Interleaving

**Purpose**: Distribute burst errors across multiple codewords to improve correction capability.

**Block Interleaver**:

```
Input:  [b0 b1 b2 b3 b4 b5 b6 b7 b8 ...]
        
Write row-wise into matrix:
[b0 b1 b2]
[b3 b4 b5]
[b6 b7 b8]

Read column-wise:
Output: [b0 b3 b6 b1 b4 b7 b2 b5 b8 ...]
```

**Parameters**:
- Block size: Number of columns
- Depth: Number of rows
- Total delay: block_size * depth

**Effect on Burst Errors**:
- Burst of length B becomes B errors separated by depth
- If depth ≥ B, burst is completely distributed
- FEC can correct distributed errors more effectively

**Trade-offs**:
- Increases latency by (block_size * depth)
- Requires buffering at transmitter and receiver
- Essential for channels with burst errors (fading, blockage)

## Beam Tracking Algorithms

### Gradient Descent Optimization

**Objective**: Maximize received signal strength S(θ) where θ = [azimuth, elevation].

**Algorithm**:

1. **Gradient Estimation** (Finite Differences):
   ```
   ∂S/∂az ≈ (S(az + δ, el) - S(az - δ, el)) / (2δ)
   ∂S/∂el ≈ (S(az, el + δ) - S(az, el - δ)) / (2δ)
   ```

2. **Update with Momentum**:
   ```
   v_az = β * v_az_prev + α * ∂S/∂az
   v_el = β * v_el_prev + α * ∂S/∂el
   
   az_new = az_old + v_az
   el_new = el_old + v_el
   ```
   where:
   - α: learning rate (step size)
   - β: momentum coefficient (0.9 typical)
   - v: velocity terms

3. **Adaptive Step Size**:
   ```
   if (S_new > S_old):
       α = min(α * 1.1, α_max)  // Increase step size
   else:
       α = max(α * 0.5, α_min)  // Decrease step size
   ```

**Parameters**:
- Initial step size: 0.001 radians (typical)
- Momentum: 0.9
- Convergence threshold: 1e-6
- Max iterations: 1000

**Convergence**:
- Declare converged when |S_new - S_old| < ε for N consecutive iterations
- Typical: ε = 1e-6, N = 10

**Advantages**:
- Simple to implement
- Works well for smooth signal landscapes
- Momentum helps escape local minima

**Limitations**:
- Can get stuck in local maxima
- Requires signal strength measurements at multiple points
- Sensitive to noise in gradient estimates

### PID Feedback Control

**Objective**: Maintain beam alignment using proportional-integral-derivative control.

**Control Law**:
```
u(t) = K_p * e(t) + K_i * ∫e(τ)dτ + K_d * de(t)/dt
```

where:
- e(t): error signal (target - current position)
- u(t): control output (position adjustment)
- K_p, K_i, K_d: tuning gains

**Discrete Implementation**:
```
error_az = target_az - current_az
error_el = target_el - current_el

integral_az += error_az * dt
integral_el += error_el * dt

derivative_az = (error_az - prev_error_az) / dt
derivative_el = (error_el - prev_error_el) / dt

output_az = K_p * error_az + K_i * integral_az + K_d * derivative_az
output_el = K_p * error_el + K_i * integral_el + K_d * derivative_el

current_az += output_az
current_el += output_el
```

**Anti-Windup**:
```
if (|integral_az| > integral_limit):
    integral_az = sign(integral_az) * integral_limit
```

**Tuning Guidelines** (Ziegler-Nichols):

1. Set K_i = K_d = 0
2. Increase K_p until system oscillates
3. Critical gain K_u and period T_u
4. Set:
   ```
   K_p = 0.6 * K_u
   K_i = 2 * K_p / T_u
   K_d = K_p * T_u / 8
   ```

**Typical Values**:
- K_p: 0.5 - 2.0
- K_i: 0.1 - 0.5
- K_d: 0.01 - 0.1
- Update rate: 100 Hz
- Integral limit: 0.1 radians

**Advantages**:
- Smooth tracking with minimal overshoot
- Disturbance rejection
- Zero steady-state error (with integral term)

**Limitations**:
- Requires tuning for specific system
- Can be unstable with poor tuning
- Integral windup if not handled

### Signal Strength Mapping

**Purpose**: Build 2D map of signal strength across angular space for gradient estimation.

**Data Structure**:
```
map[azimuth_index][elevation_index] = signal_strength
```

**Interpolation** (Bilinear):
```
Given point (az, el) between grid points:

f(az, el) = f(0,0) * (1-x) * (1-y) +
            f(1,0) * x * (1-y) +
            f(0,1) * (1-x) * y +
            f(1,1) * x * y

where x, y are fractional positions within grid cell
```

**Scanning Strategy**:

1. **Coarse Scan**:
   - Large angular range (±5°)
   - Coarse resolution (0.5°)
   - Find approximate peak

2. **Fine Scan**:
   - Small range around peak (±1°)
   - Fine resolution (0.1°)
   - Refine peak location

**Update Strategy**:
- Periodic full scans (every 60 seconds)
- Continuous local updates during tracking
- Exponential decay of old measurements

### Misalignment Detection and Recovery

**Detection**:
```
if (signal_strength < threshold):
    misalignment_detected = true
    trigger_reacquisition()
```

**Threshold Setting**:
- Typical: 20% of nominal signal strength
- Adaptive: based on recent signal history
- Hysteresis: different thresholds for detection and recovery

**Reacquisition Procedure**:

1. **Expand Search**:
   - Increase search range progressively
   - Start: ±2°, then ±5°, then ±10°

2. **Spiral Scan**:
   ```
   for radius in [r1, r2, r3]:
       for angle in [0, 2π]:
           az = center_az + radius * cos(angle)
           el = center_el + radius * sin(angle)
           measure_signal(az, el)
   ```

3. **Peak Detection**:
   - Find maximum in scan results
   - Move to peak location

4. **Fine Alignment**:
   - Perform fine scan around peak
   - Resume normal tracking

**Recovery Time**:
- Typical: 1-5 seconds depending on search range
- Trade-off: speed vs. thoroughness

## Atmospheric Channel Models

### Log-Normal Fading

**Physical Basis**: Atmospheric turbulence causes random fluctuations in refractive index, leading to intensity scintillation.

**Rytov Variance**:
```
σ_χ² = 0.5 * C_n² * k^(7/6) * L^(11/6)
```

where:
- C_n²: Refractive index structure parameter (m^(-2/3))
- k = 2π/λ: Wave number
- L: Link distance (m)

**Typical C_n² Values**:
- Strong turbulence: 1e-13 m^(-2/3)
- Moderate turbulence: 1e-14 m^(-2/3)
- Weak turbulence: 1e-15 m^(-2/3)

**Scintillation Index**:
```
σ_I² = exp(4 * σ_χ²) - 1
```

**Fading Distribution**:
```
I ~ LogNormal(μ, σ²)
μ = -σ²/2  (to ensure E[I] = 1)
σ² = ln(σ_I² + 1)
```

**Generation**:
```
X ~ Normal(0, 1)
I = exp(μ + σ * X)
```

**Temporal Correlation** (AR(1) Process):
```
I(t) = ρ * I(t-Δt) + sqrt(1 - ρ²) * W(t)
```

where:
- ρ = exp(-Δt / τ_c): Correlation coefficient
- τ_c: Correlation time (typically 1-10 ms)
- W(t): White Gaussian noise

### Weather Attenuation Models

**Clear Air**:
```
α_clear ≈ 0.1 dB/km
```

**Fog** (Kim Model):
```
α_fog = 3.91 / V * (λ/550nm)^(-q)
```

where:
- V: Visibility (km)
- λ: Wavelength (nm)
- q: Size distribution parameter
  - q = 1.6 for V > 50 km
  - q = 1.3 for 6 km < V < 50 km
  - q = 0.16V + 0.34 for 1 km < V < 6 km
  - q = V - 0.5 for 0.5 km < V < 1 km

**Rain** (Carbonneau Model):
```
α_rain = 1.076 * R^0.67  (dB/km)
```

where R is rainfall rate (mm/hr)

**Typical Values**:
- Light rain (2.5 mm/hr): 1.5 dB/km
- Moderate rain (12.5 mm/hr): 4.2 dB/km
- Heavy rain (25 mm/hr): 6.7 dB/km

**Snow**:
```
α_snow = 0.5 * S  (dB/km)
```

where S is snowfall rate (mm/hr)

### Path Loss

**Free-Space Path Loss**:
```
L_fs(dB) = 20 * log10(4π * d / λ)
```

**Example** (1550 nm, 1 km):
```
L_fs = 20 * log10(4π * 1000 / 1.55e-6)
     ≈ 172 dB
```

**Geometric Loss** (Beam Divergence):
```
L_geo(dB) = 20 * log10(d * θ / D_rx)
```

where:
- θ: Beam divergence (radians)
- D_rx: Receiver aperture diameter (m)

**Total Link Budget**:
```
P_rx(dBm) = P_tx(dBm) - L_fs - L_geo - α*d - L_atm + G_tx + G_rx
```

where:
- P_tx: Transmit power
- L_fs: Free-space path loss
- L_geo: Geometric loss
- α*d: Atmospheric attenuation
- L_atm: Additional atmospheric effects
- G_tx, G_rx: Transmitter and receiver gains

## Signal Processing

### FFT Operations

**Algorithm**: FFTW (Fastest Fourier Transform in the West) library with OpenMP support.

**Complexity**: O(N log N) where N is FFT size.

**Parallelization Strategy**:
- FFTW automatically uses OpenMP threads
- Efficient for N ≥ 4096
- Batch processing for multiple FFTs

**Optimal FFT Sizes**:
- Powers of 2: 1024, 2048, 4096, 8192, 16384
- Avoid prime numbers (slower)

**Usage**:
```c
// Create plan (do once)
fftw_plan plan = fftw_plan_dft_r2c_1d(N, input, output, FFTW_MEASURE);

// Execute (can reuse plan)
fftw_execute(plan);

// Cleanup
fftw_destroy_plan(plan);
```

### Filtering

**Moving Average**:
```
y[n] = (1/W) * Σ(x[n-k]) for k = 0 to W-1
```

**Parallelization**:
```c
#pragma omp parallel for
for (int n = 0; n < length; n++) {
    double sum = 0.0;
    for (int k = 0; k < window; k++) {
        if (n - k >= 0) {
            sum += input[n - k];
        }
    }
    output[n] = sum / window;
}
```

**Adaptive LMS Filter**:
```
e[n] = d[n] - y[n]
w[n+1] = w[n] + μ * e[n] * x[n]
```

where:
- μ: Step size (learning rate)
- e[n]: Error signal
- w[n]: Filter weights

**Convergence**:
- 0 < μ < 2/λ_max where λ_max is largest eigenvalue of input autocorrelation
- Typical: μ = 0.01

## Configuration Parameters

### Modulation

| Parameter | OOK | PPM | DPSK |
|-----------|-----|-----|------|
| Symbol Rate | 1 Msps | 1 Msps | 1 Msps |
| Bits/Symbol | 1 | log₂(M) | 1 |
| Bandwidth | 1 MHz | M MHz | 1 MHz |
| Power Efficiency | Low | High | Medium |

### FEC

| Parameter | Reed-Solomon | LDPC |
|-----------|--------------|------|
| Code Rate | 0.5 - 0.9 | 0.5 - 0.9 |
| Block Size | 255 symbols | 1024 - 8192 bits |
| Latency | Low | Medium |
| Performance | Good | Excellent |

### Beam Tracking

| Parameter | Typical Value | Range |
|-----------|---------------|-------|
| Step Size | 0.001 rad | 0.0001 - 0.01 |
| Momentum | 0.9 | 0.0 - 0.99 |
| PID K_p | 1.0 | 0.1 - 10.0 |
| PID K_i | 0.2 | 0.01 - 1.0 |
| PID K_d | 0.05 | 0.001 - 0.5 |
| Update Rate | 100 Hz | 10 - 1000 Hz |

### Channel

| Parameter | Typical Value | Range |
|-----------|---------------|-------|
| C_n² | 1e-14 m^(-2/3) | 1e-17 - 1e-12 |
| Correlation Time | 5 ms | 1 - 100 ms |
| Visibility (fog) | 500 m | 50 - 10000 m |
| Rainfall Rate | 10 mm/hr | 0 - 100 mm/hr |

## Performance Tuning

### OpenMP Thread Count

**Guidelines**:
- Use number of physical cores (not hyperthreads)
- Leave 1-2 cores for OS
- Test with 1, 2, 4, 8 threads

**Setting**:
```bash
export OMP_NUM_THREADS=4
```

or in code:
```c
omp_set_num_threads(4);
```

### Memory Optimization

**Buffer Sizes**:
- FFT: Use power-of-2 sizes
- Filters: Pre-allocate buffers
- Thread-local storage: Minimize size

**Cache Optimization**:
- Process data in chunks that fit in L2 cache
- Typical L2: 256 KB - 1 MB
- Chunk size: 32K - 128K samples

### Latency Reduction

**Strategies**:
- Reduce FFT size (trade frequency resolution)
- Use smaller FEC block sizes
- Decrease interleaver depth
- Increase beam tracking update rate

**Target Latencies**:
- Modulation/Demodulation: < 1 ms
- FEC Encoding: < 2 ms
- FEC Decoding: < 5 ms
- Beam Tracking Update: < 1 ms
- Total End-to-End: < 10 ms

### Throughput Optimization

**Batch Processing**:
- Process multiple frames in parallel
- Use SIMD instructions where possible
- Minimize memory allocations

**Pipeline**:
```
Frame 1: [Encode] -> [Modulate] -> [Channel] -> [Demodulate] -> [Decode]
Frame 2:            [Encode] -> [Modulate] -> [Channel] -> [Demodulate]
Frame 3:                       [Encode] -> [Modulate] -> [Channel]
```

**Profiling**:
```bash
# Use gprof
gcc -pg ...
./program
gprof program gmon.out

# Use perf
perf record ./program
perf report
```
