/**
 * @file bench_fft.c
 * @brief FFT performance benchmarks
 * 
 * Benchmarks FFT operations with various sizes and thread counts.
 * Tests 1K, 4K, 16K, and 64K point FFTs with 1, 2, 4, 8, and 16 threads.
 */

#include "benchmark.h"
#include "../src/signal_processing/signal_processing.h"
#include <stdlib.h>
#include <string.h>
#include <complex.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* ============================================================================
 * FFT Benchmark Configuration
 * ============================================================================ */

static const size_t FFT_SIZES[] = {1024, 4096, 16384, 65536};
static const int NUM_FFT_SIZES = 4;

static const int THREAD_COUNTS[] = {1, 2, 4, 8, 16};
static const int NUM_THREAD_COUNTS = 5;

static const int BENCHMARK_ITERATIONS = 100;
static const int WARMUP_ITERATIONS = 10;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Generate test signal for FFT
 */
static void generate_test_signal(double* signal, size_t length) {
    // Generate a mix of sine waves
    for (size_t i = 0; i < length; i++) {
        double t = (double)i / length;
        signal[i] = sin(2.0 * FSO_PI * 10.0 * t) +
                   0.5 * sin(2.0 * FSO_PI * 25.0 * t) +
                   0.25 * sin(2.0 * FSO_PI * 50.0 * t);
    }
}

/**
 * @brief Run FFT benchmark for a specific configuration
 */
static int run_fft_benchmark(size_t fft_size, int num_threads,
                            PerformanceMetrics* metrics) {
    SignalProcessor sp;
    double* input = NULL;
    double complex* output = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    
    // Initialize metrics
    benchmark_metrics_init(metrics);
    metrics->num_threads = num_threads;
    metrics->data_size_bytes = fft_size * sizeof(double);
    metrics->iterations = BENCHMARK_ITERATIONS;
    
    // Allocate memory
    input = (double*)malloc(fft_size * sizeof(double));
    output = (double complex*)malloc((fft_size / 2 + 1) * sizeof(double complex));
    times = (double*)malloc(BENCHMARK_ITERATIONS * sizeof(double));
    
    if (!input || !output || !times) {
        FSO_LOG_ERROR("BENCH_FFT", "Memory allocation failed");
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate test signal
    generate_test_signal(input, fft_size);
    
    // Initialize signal processor
    if (sp_init(&sp, num_threads, fft_size) != FSO_SUCCESS) {
        FSO_LOG_ERROR("BENCH_FFT", "Failed to initialize signal processor");
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Warmup iterations
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        sp_fft(&sp, input, output, fft_size);
    }
    
    // Benchmark iterations
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    size_t mem_before = benchmark_get_memory_usage();
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        benchmark_timer_start(&timer);
        
        int fft_result = sp_fft(&sp, input, output, fft_size);
        
        benchmark_timer_stop(&timer);
        
        if (fft_result == FSO_SUCCESS) {
            metrics->success_count++;
        } else {
            metrics->failure_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    size_t mem_after = benchmark_get_memory_usage();
    metrics->peak_memory_bytes = mem_after > mem_before ? 
                                 mem_after - mem_before : 0;
    
    // Calculate statistics
    benchmark_calculate_statistics(times, BENCHMARK_ITERATIONS,
                                   &metrics->avg_time_ms,
                                   &metrics->stddev_time_ms,
                                   &metrics->min_time_ms,
                                   &metrics->max_time_ms);
    
    // Calculate throughput (samples per second)
    metrics->throughput_samples_sec = 
        benchmark_calculate_throughput_samples(fft_size, metrics->avg_time_ms);
    
    // Calculate throughput in Mbps (treating each sample as 8 bytes)
    metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(fft_size * sizeof(double), 
                                           metrics->avg_time_ms);
    
    // Cleanup
    sp_free(&sp);
    
cleanup:
    free(input);
    free(output);
    free(times);
    
    return result;
}

/**
 * @brief Run serial FFT benchmark (baseline)
 */
static int run_serial_fft_benchmark(size_t fft_size, 
                                   PerformanceMetrics* metrics) {
    return run_fft_benchmark(fft_size, 1, metrics);
}

/**
 * @brief Calculate speedup metrics relative to serial baseline
 */
static void calculate_speedup_metrics(PerformanceMetrics* parallel_metrics,
                                     const PerformanceMetrics* serial_metrics) {
    if (serial_metrics->avg_time_ms > 0.0) {
        parallel_metrics->speedup_factor = 
            benchmark_calculate_speedup(serial_metrics->avg_time_ms,
                                       parallel_metrics->avg_time_ms);
        
        parallel_metrics->parallel_efficiency = 
            benchmark_calculate_efficiency(parallel_metrics->speedup_factor,
                                          parallel_metrics->num_threads);
    }
}

/* ============================================================================
 * Public Benchmark Functions
 * ============================================================================ */

/**
 * @brief Run comprehensive FFT benchmarks
 */
int benchmark_fft_comprehensive(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  FFT Performance Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    int max_threads = benchmark_get_num_cores();
    printf("System information:\n");
    printf("  Available CPU cores: %d\n", max_threads);
    printf("  OpenMP available: %s\n", 
#ifdef _OPENMP
           "Yes"
#else
           "No"
#endif
    );
    printf("\n");
    
    // Allocate results storage
    int total_configs = NUM_FFT_SIZES * NUM_THREAD_COUNTS;
    PerformanceMetrics* all_metrics = 
        (PerformanceMetrics*)calloc(total_configs, sizeof(PerformanceMetrics));
    char** labels = (char**)calloc(total_configs, sizeof(char*));
    
    if (!all_metrics || !labels) {
        FSO_LOG_ERROR("BENCH_FFT", "Failed to allocate results storage");
        free(all_metrics);
        free(labels);
        return FSO_ERROR_MEMORY;
    }
    
    int config_idx = 0;
    
    // Run benchmarks for each FFT size
    for (int size_idx = 0; size_idx < NUM_FFT_SIZES; size_idx++) {
        size_t fft_size = FFT_SIZES[size_idx];
        
        printf("--------------------------------------------------------------------------------\n");
        printf("FFT Size: %zu points\n", fft_size);
        printf("--------------------------------------------------------------------------------\n");
        
        // Run serial baseline
        PerformanceMetrics serial_metrics;
        printf("Running serial baseline...\n");
        if (run_serial_fft_benchmark(fft_size, &serial_metrics) != FSO_SUCCESS) {
            FSO_LOG_ERROR("BENCH_FFT", "Serial benchmark failed for size %zu", fft_size);
            continue;
        }
        
        // Store serial results
        all_metrics[config_idx] = serial_metrics;
        labels[config_idx] = (char*)malloc(64);
        snprintf(labels[config_idx], 64, "%zu_serial", fft_size);
        config_idx++;
        
        printf("  Serial: %.3f ms (%.2f Msamples/sec)\n",
               serial_metrics.avg_time_ms,
               serial_metrics.throughput_samples_sec / 1000000.0);
        
        // Run parallel benchmarks
        for (int thread_idx = 0; thread_idx < NUM_THREAD_COUNTS; thread_idx++) {
            int num_threads = THREAD_COUNTS[thread_idx];
            
            // Skip if more threads than available cores
            if (num_threads > max_threads) {
                continue;
            }
            
            // Skip serial (already done)
            if (num_threads == 1) {
                continue;
            }
            
            printf("Running with %d threads...\n", num_threads);
            
            PerformanceMetrics parallel_metrics;
            if (run_fft_benchmark(fft_size, num_threads, 
                                 &parallel_metrics) != FSO_SUCCESS) {
                FSO_LOG_ERROR("BENCH_FFT", 
                            "Parallel benchmark failed for size %zu, threads %d",
                            fft_size, num_threads);
                continue;
            }
            
            // Calculate speedup
            calculate_speedup_metrics(&parallel_metrics, &serial_metrics);
            
            // Store results
            all_metrics[config_idx] = parallel_metrics;
            labels[config_idx] = (char*)malloc(64);
            snprintf(labels[config_idx], 64, "%zu_%dthreads", 
                    fft_size, num_threads);
            config_idx++;
            
            printf("  %d threads: %.3f ms (%.2f Msamples/sec, %.2fx speedup, %.1f%% efficiency)\n",
                   num_threads,
                   parallel_metrics.avg_time_ms,
                   parallel_metrics.throughput_samples_sec / 1000000.0,
                   parallel_metrics.speedup_factor,
                   parallel_metrics.parallel_efficiency * 100.0);
        }
        
        printf("\n");
    }
    
    // Save results
    printf("Saving results...\n");
    benchmark_save_csv("fft_benchmark_results.csv", all_metrics, 
                      config_idx, (const char**)labels);
    benchmark_save_json("fft_benchmark_results.json", all_metrics,
                       config_idx, (const char**)labels);
    
    // Print summary
    benchmark_print_summary(all_metrics, config_idx, "FFT Benchmark Summary");
    
    // Cleanup
    for (int i = 0; i < config_idx; i++) {
        free(labels[i]);
    }
    free(labels);
    free(all_metrics);
    
    printf("FFT benchmarks completed successfully!\n");
    printf("\n");
    
    return FSO_SUCCESS;
}

/**
 * @brief Run quick FFT benchmark (single size, multiple threads)
 */
int benchmark_fft_quick(size_t fft_size) {
    BenchmarkConfig config = {
        .name = "FFT Quick Benchmark",
        .iterations = BENCHMARK_ITERATIONS,
        .warmup_iterations = WARMUP_ITERATIONS,
        .num_threads = 0,
        .data_size = fft_size * sizeof(double),
        .verbose = 1
    };
    
    benchmark_print_header(&config);
    
    // Run serial baseline
    PerformanceMetrics serial_metrics;
    printf("Running serial baseline...\n");
    if (run_serial_fft_benchmark(fft_size, &serial_metrics) != FSO_SUCCESS) {
        FSO_LOG_ERROR("BENCH_FFT", "Serial benchmark failed");
        return FSO_ERROR_IO;
    }
    
    printf("\nSerial Results:\n");
    benchmark_print_metrics(&serial_metrics, &config);
    
    // Run with maximum threads
    int max_threads = benchmark_get_num_cores();
    PerformanceMetrics parallel_metrics;
    
    printf("Running with %d threads...\n", max_threads);
    if (run_fft_benchmark(fft_size, max_threads, 
                         &parallel_metrics) != FSO_SUCCESS) {
        FSO_LOG_ERROR("BENCH_FFT", "Parallel benchmark failed");
        return FSO_ERROR_IO;
    }
    
    calculate_speedup_metrics(&parallel_metrics, &serial_metrics);
    
    printf("\nParallel Results (%d threads):\n", max_threads);
    benchmark_print_metrics(&parallel_metrics, &config);
    
    return FSO_SUCCESS;
}
