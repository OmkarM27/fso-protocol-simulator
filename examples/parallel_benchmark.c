/**
 * @file parallel_benchmark.c
 * @brief Parallel processing benchmark example
 * 
 * This example demonstrates OpenMP parallelization benefits by benchmarking
 * signal processing operations with different thread counts.
 * 
 * Compile:
 *   gcc -I../src parallel_benchmark.c -L../build -lfso -lm -lfftw3 -lfftw3_omp -fopenmp -o parallel_benchmark
 * 
 * Run:
 *   ./parallel_benchmark
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "fso.h"
#include "signal_processing/signal_processing.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#define SIGNAL_LENGTH 16384  // 16K samples
#define NUM_ITERATIONS 100   // Repeat for timing accuracy

// High-resolution timer
double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// Benchmark FFT operation
double benchmark_fft(SignalProcessor* sp, int num_threads) {
    // Generate test signal
    double* input = (double*)malloc(SIGNAL_LENGTH * sizeof(double));
    double complex* output = (double complex*)malloc((SIGNAL_LENGTH/2 + 1) * sizeof(double complex));
    
    // Fill with test data (sine wave + noise)
    for (int i = 0; i < SIGNAL_LENGTH; i++) {
        input[i] = sin(2.0 * FSO_PI * 10.0 * i / SIGNAL_LENGTH) + 
                   fso_random_gaussian(0.0, 0.1);
    }
    
    // Warm-up
    sp_fft(sp, input, output, SIGNAL_LENGTH);
    
    // Benchmark
    double start_time = get_time();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        sp_fft(sp, input, output, SIGNAL_LENGTH);
    }
    double end_time = get_time();
    
    double elapsed = end_time - start_time;
    double avg_time = elapsed / NUM_ITERATIONS;
    
    free(input);
    free(output);
    
    return avg_time;
}

// Benchmark moving average filter
double benchmark_moving_average(SignalProcessor* sp, int window_size) {
    // Generate test signal
    double* input = (double*)malloc(SIGNAL_LENGTH * sizeof(double));
    double* output = (double*)malloc(SIGNAL_LENGTH * sizeof(double));
    
    for (int i = 0; i < SIGNAL_LENGTH; i++) {
        input[i] = fso_random_gaussian(0.0, 1.0);
    }
    
    // Warm-up
    sp_moving_average(sp, input, output, SIGNAL_LENGTH, window_size);
    
    // Benchmark
    double start_time = get_time();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        sp_moving_average(sp, input, output, SIGNAL_LENGTH, window_size);
    }
    double end_time = get_time();
    
    double elapsed = end_time - start_time;
    double avg_time = elapsed / NUM_ITERATIONS;
    
    free(input);
    free(output);
    
    return avg_time;
}

// Benchmark convolution
double benchmark_convolution(SignalProcessor* sp, int kernel_size) {
    // Generate test signal and kernel
    double* signal = (double*)malloc(SIGNAL_LENGTH * sizeof(double));
    double* kernel = (double*)malloc(kernel_size * sizeof(double));
    double* output = (double*)malloc((SIGNAL_LENGTH + kernel_size - 1) * sizeof(double));
    
    for (int i = 0; i < SIGNAL_LENGTH; i++) {
        signal[i] = fso_random_gaussian(0.0, 1.0);
    }
    
    for (int i = 0; i < kernel_size; i++) {
        kernel[i] = exp(-0.5 * (i - kernel_size/2) * (i - kernel_size/2) / 10.0);
    }
    
    // Warm-up
    sp_convolution(sp, signal, kernel, output, SIGNAL_LENGTH, kernel_size);
    
    // Benchmark
    double start_time = get_time();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        sp_convolution(sp, signal, kernel, output, SIGNAL_LENGTH, kernel_size);
    }
    double end_time = get_time();
    
    double elapsed = end_time - start_time;
    double avg_time = elapsed / NUM_ITERATIONS;
    
    free(signal);
    free(kernel);
    free(output);
    
    return avg_time;
}

int main(void) {
    printf("=== Parallel Processing Benchmark ===\n\n");
    
    // Check OpenMP availability
#ifdef _OPENMP
    printf("OpenMP is ENABLED\n");
    printf("Max threads available: %d\n\n", omp_get_max_threads());
#else
    printf("OpenMP is NOT ENABLED\n");
    printf("Only serial execution will be tested\n\n");
#endif
    
    // Set log level
    fso_set_log_level(LOG_WARNING);
    
    // Initialize random number generator
    fso_random_init(42);
    
    // Thread counts to test
    int thread_counts[] = {1, 2, 4, 8};
    int num_thread_counts = sizeof(thread_counts) / sizeof(thread_counts[0]);
    
    // Limit to available threads
#ifdef _OPENMP
    int max_threads = omp_get_max_threads();
    for (int i = 0; i < num_thread_counts; i++) {
        if (thread_counts[i] > max_threads) {
            num_thread_counts = i;
            break;
        }
    }
#else
    num_thread_counts = 1;  // Only test 1 thread
#endif
    
    printf("Testing with signal length: %d samples\n", SIGNAL_LENGTH);
    printf("Iterations per test: %d\n\n", NUM_ITERATIONS);
    
    // ========================================================================
    // Benchmark 1: FFT Operations
    // ========================================================================
    printf("--- Benchmark 1: FFT Operations ---\n\n");
    
    double fft_times[8];
    double fft_baseline = 0.0;
    
    for (int i = 0; i < num_thread_counts; i++) {
        int num_threads = thread_counts[i];
        
        SignalProcessor sp;
        if (sp_init(&sp, num_threads, SIGNAL_LENGTH) != FSO_SUCCESS) {
            fprintf(stderr, "Failed to initialize signal processor\n");
            continue;
        }
        
        double avg_time = benchmark_fft(&sp, num_threads);
        fft_times[i] = avg_time;
        
        if (i == 0) {
            fft_baseline = avg_time;
        }
        
        double speedup = fft_baseline / avg_time;
        double efficiency = speedup / num_threads * 100.0;
        
        printf("Threads: %d\n", num_threads);
        printf("  Average time: %.3f ms\n", avg_time * 1000.0);
        printf("  Speedup: %.2fx\n", speedup);
        printf("  Efficiency: %.1f%%\n\n", efficiency);
        
        sp_free(&sp);
    }
    
    // ========================================================================
    // Benchmark 2: Moving Average Filter
    // ========================================================================
    printf("--- Benchmark 2: Moving Average Filter (window=64) ---\n\n");
    
    double ma_times[8];
    double ma_baseline = 0.0;
    int window_size = 64;
    
    for (int i = 0; i < num_thread_counts; i++) {
        int num_threads = thread_counts[i];
        
        SignalProcessor sp;
        if (sp_init(&sp, num_threads, SIGNAL_LENGTH) != FSO_SUCCESS) {
            fprintf(stderr, "Failed to initialize signal processor\n");
            continue;
        }
        
        double avg_time = benchmark_moving_average(&sp, window_size);
        ma_times[i] = avg_time;
        
        if (i == 0) {
            ma_baseline = avg_time;
        }
        
        double speedup = ma_baseline / avg_time;
        double efficiency = speedup / num_threads * 100.0;
        
        printf("Threads: %d\n", num_threads);
        printf("  Average time: %.3f ms\n", avg_time * 1000.0);
        printf("  Speedup: %.2fx\n", speedup);
        printf("  Efficiency: %.1f%%\n\n", efficiency);
        
        sp_free(&sp);
    }
    
    // ========================================================================
    // Benchmark 3: Convolution
    // ========================================================================
    printf("--- Benchmark 3: Convolution (kernel=128) ---\n\n");
    
    double conv_times[8];
    double conv_baseline = 0.0;
    int kernel_size = 128;
    
    for (int i = 0; i < num_thread_counts; i++) {
        int num_threads = thread_counts[i];
        
        SignalProcessor sp;
        if (sp_init(&sp, num_threads, SIGNAL_LENGTH) != FSO_SUCCESS) {
            fprintf(stderr, "Failed to initialize signal processor\n");
            continue;
        }
        
        double avg_time = benchmark_convolution(&sp, kernel_size);
        conv_times[i] = avg_time;
        
        if (i == 0) {
            conv_baseline = avg_time;
        }
        
        double speedup = conv_baseline / avg_time;
        double efficiency = speedup / num_threads * 100.0;
        
        printf("Threads: %d\n", num_threads);
        printf("  Average time: %.3f ms\n", avg_time * 1000.0);
        printf("  Speedup: %.2fx\n", speedup);
        printf("  Efficiency: %.1f%%\n\n", efficiency);
        
        sp_free(&sp);
    }
    
    // ========================================================================
    // Summary Table
    // ========================================================================
    printf("=== Summary Table ===\n\n");
    
    printf("Threads |   FFT   | Moving Avg | Convolution\n");
    printf("--------|---------|------------|------------\n");
    
    for (int i = 0; i < num_thread_counts; i++) {
        int num_threads = thread_counts[i];
        double fft_speedup = fft_baseline / fft_times[i];
        double ma_speedup = ma_baseline / ma_times[i];
        double conv_speedup = conv_baseline / conv_times[i];
        
        printf("   %d    | %.2fx    | %.2fx       | %.2fx\n",
               num_threads, fft_speedup, ma_speedup, conv_speedup);
    }
    
    // ========================================================================
    // Analysis
    // ========================================================================
    printf("\n=== Analysis ===\n\n");
    
    if (num_thread_counts >= 3) {
        int idx_4threads = -1;
        for (int i = 0; i < num_thread_counts; i++) {
            if (thread_counts[i] == 4) {
                idx_4threads = i;
                break;
            }
        }
        
        if (idx_4threads >= 0) {
            double fft_speedup_4t = fft_baseline / fft_times[idx_4threads];
            double ma_speedup_4t = ma_baseline / ma_times[idx_4threads];
            double conv_speedup_4t = conv_baseline / conv_times[idx_4threads];
            
            printf("Speedup with 4 threads:\n");
            printf("  FFT: %.2fx\n", fft_speedup_4t);
            printf("  Moving Average: %.2fx\n", ma_speedup_4t);
            printf("  Convolution: %.2fx\n\n", conv_speedup_4t);
            
            if (fft_speedup_4t >= 3.0) {
                printf("✓ FFT achieves good parallel speedup (>= 3x)\n");
            } else {
                printf("⚠ FFT speedup is below target (< 3x)\n");
            }
            
            if (ma_speedup_4t >= 3.0) {
                printf("✓ Moving Average achieves good parallel speedup (>= 3x)\n");
            } else {
                printf("⚠ Moving Average speedup is below target (< 3x)\n");
            }
            
            if (conv_speedup_4t >= 3.0) {
                printf("✓ Convolution achieves good parallel speedup (>= 3x)\n");
            } else {
                printf("⚠ Convolution speedup is below target (< 3x)\n");
            }
        }
    }
    
    printf("\n=== Recommendations ===\n\n");
    printf("1. Use 4 threads for best performance/efficiency trade-off\n");
    printf("2. FFT benefits most from parallelization\n");
    printf("3. Ensure FFTW is compiled with OpenMP support\n");
    printf("4. For small data sizes, serial may be faster due to overhead\n");
    printf("5. Profile your specific workload to find optimal thread count\n\n");
    
    printf("=== Benchmark Complete ===\n");
    
    return 0;
}
