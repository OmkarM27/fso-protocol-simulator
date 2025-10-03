# FSO Benchmarking Framework

This directory contains comprehensive performance benchmarks for the FSO Communication Suite.

## Overview

The benchmarking framework measures performance across all major components:
- **FFT Operations**: 1K, 4K, 16K, 64K point FFTs with 1-16 threads
- **Filtering**: Moving average, adaptive filters, convolution
- **Modulation**: OOK, PPM (2/4/8/16), DPSK throughput
- **FEC**: Reed-Solomon and LDPC encoding/decoding
- **End-to-End**: Complete transmit-receive cycle latency

## Building

```bash
make benchmarks
```

## Running Benchmarks

### Run all benchmarks
```bash
./bin/fso_benchmark --all
```

### Run specific benchmarks
```bash
./bin/fso_benchmark --fft          # FFT only
./bin/fso_benchmark --filters      # Filters only
./bin/fso_benchmark --modulation   # Modulation only
./bin/fso_benchmark --fec          # FEC only
./bin/fso_benchmark --e2e          # End-to-end only
```

### Quick mode (faster)
```bash
./bin/fso_benchmark --quick
```

### Generate reports
```bash
./bin/fso_benchmark --all --report
```

## Output Files

- `*_benchmark_results.csv` - Detailed results in CSV format
- `*_benchmark_results.json` - Results in JSON format
- `benchmark_summary.txt` - Text summary
- `benchmark_report.html` - HTML report with visualizations
- `*.gnu` - Gnuplot scripts for visualization

## Visualization

Generate plots using gnuplot:
```bash
gnuplot speedup_plot.gnu
gnuplot efficiency_plot.gnu
gnuplot throughput_plot.gnu
```

## Metrics Collected

- Execution time (avg, min, max, stddev)
- Throughput (Mbps, samples/sec, ops/sec)
- Memory usage (peak, average)
- Speedup factor (parallel vs serial)
- Parallel efficiency
- Real-time compliance (for end-to-end)
