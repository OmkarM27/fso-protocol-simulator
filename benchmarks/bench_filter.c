/**
 * @file bench_filter.c
 * @brief Filter performance benchmarks
 * 
 * Benchmarks filtering operations including moving average, adaptive filter,
 * and convolution with various window sizes and data lengths.
 */

#include "benchmark.h"
#include "../src/signal_processing/signal_processing.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Filter Benchmark Configuration
 * ============================================================================ */

static const size_t DATA_LENGTHS[] = {1024, 4096, 16384, 65536};
static const int NUM_DATA_LENGTHS = 4;

static const int WINDOW_SIZES[] = {8, 16, 32, 64, 128};
static const int NUM_WINDOW_SIZES = 5;

static const int FILTER_LENGTHS[] = {16, 32, 64, 128};
static const int NUM_FILTER_LENGTHS = 4;

static const int BENCHMARK_ITERATIONS = 100;
static const int WARMUP_ITERATIONS = 10;

/* ============================================================================
 * Moving Average Benchmark
 * ============================================================================ */

/**
 * @brief Run moving average benchmark
 */
static int run_moving_average_benchmark(size_t data_length, int window_size,
                                       int num_threads,
                                       PerformanceMetrics* metrics) {
    SignalProcessor sp;
    double* input = NULL;
    double* output = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    
    // Initialize metrics
    benchmark_metrics_init(metrics);
    metrics->num_threads = num_threads;
    metrics->data_size_bytes = data_length * sizeof(double);
    metrics->iterations = BENCHMARK_ITERATIONS;
    
    // Allocate memory
    input = (double*)malloc(data_length * sizeof(double));
    output = (double*)malloc(data_length * sizeof(double));
    times = (double*)malloc(BENCHMARK_ITERATIONS * sizeof(double));
    
    if (!input || !output || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate test signal
    for (size_t i = 0; i < data_length; i++) {
        input[i] = sin(2.0 * FSO_PI * i / 100.0) + 
                  0.5 * fso_random_gaussian(0.0, 0.1);
    }
    
    // Initialize signal processor
    if (sp_init(&sp, num_threads, data_length) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        sp_moving_average(&sp, input, output, data_length, window_size);
    }
    
    // Benchmark
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        benchmark_timer_start(&timer);
        
        int filter_result = sp_moving_average(&sp, input, output, 
                                             data_length, window_size);
        
        benchmark_timer_stop(&timer);
        
        if (filter_result == FSO_SUCCESS) {
            metrics->success_count++;
        } else {
            metrics->failure_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    // Calculate statistics
    benchmark_calculate_statistics(times, BENCHMARK_ITERATIONS,
                                   &metrics->avg_time_ms,
                                   &metrics->stddev_time_ms,
                                   &metrics->min_time_ms,
                                   &metrics->max_time_ms);
    
    // Calculate throughput
    metrics->throughput_samples_sec = 
        benchmark_calculate_throughput_samples(data_length, metrics->avg_time_ms);
    metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_length * sizeof(double),
                                           metrics->avg_time_ms);
    
    sp_free(&sp);
    
cleanup:
    free(input);
    free(output);
    free(times);
    
    return result;
}

/**
 * @brief Benchmark moving average with various configurations
 */
int benchmark_moving_average(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  Moving Average Filter Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    int max_threads = benchmark_get_num_cores();
    
    // Test different data lengths with fixed window
    printf("Testing different data lengths (window size = 32):\n");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < NUM_DATA_LENGTHS; i++) {
        size_t data_length = DATA_LENGTHS[i];
        
        // Serial
        PerformanceMetrics serial_metrics;
        run_moving_average_benchmark(data_length, 32, 1, &serial_metrics);
        
        // Parallel
        PerformanceMetrics parallel_metrics;
        run_moving_average_benchmark(data_length, 32, max_threads, 
                                    &parallel_metrics);
        
        parallel_metrics.speedup_factor = 
            benchmark_calculate_speedup(serial_metrics.avg_time_ms,
                                       parallel_metrics.avg_time_ms);
        parallel_metrics.parallel_efficiency = 
            benchmark_calculate_efficiency(parallel_metrics.speedup_factor,
                                          max_threads);
        
        printf("  Length %6zu: Serial %.3f ms, Parallel %.3f ms (%.2fx speedup)\n",
               data_length,
               serial_metrics.avg_time_ms,
               parallel_metrics.avg_time_ms,
               parallel_metrics.speedup_factor);
    }
    
    printf("\n");
    
    // Test different window sizes with fixed data length
    printf("Testing different window sizes (data length = 16384):\n");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < NUM_WINDOW_SIZES; i++) {
        int window_size = WINDOW_SIZES[i];
        
        // Serial
        PerformanceMetrics serial_metrics;
        run_moving_average_benchmark(16384, window_size, 1, &serial_metrics);
        
        // Parallel
        PerformanceMetrics parallel_metrics;
        run_moving_average_benchmark(16384, window_size, max_threads,
                                    &parallel_metrics);
        
        parallel_metrics.speedup_factor = 
            benchmark_calculate_speedup(serial_metrics.avg_time_ms,
                                       parallel_metrics.avg_time_ms);
        
        printf("  Window %3d: Serial %.3f ms, Parallel %.3f ms (%.2fx speedup)\n",
               window_size,
               serial_metrics.avg_time_ms,
               parallel_metrics.avg_time_ms,
               parallel_metrics.speedup_factor);
    }
    
    printf("\n");
    return FSO_SUCCESS;
}

/* ============================================================================
 * Adaptive Filter Benchmark
 * ============================================================================ */

/**
 * @brief Run adaptive filter benchmark
 */
static int run_adaptive_filter_benchmark(size_t data_length, int filter_length,
                                        int num_threads,
                                        PerformanceMetrics* metrics) {
    SignalProcessor sp;
    double* input = NULL;
    double* desired = NULL;
    double* output = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    
    // Initialize metrics
    benchmark_metrics_init(metrics);
    metrics->num_threads = num_threads;
    metrics->data_size_bytes = data_length * sizeof(double);
    metrics->iterations = BENCHMARK_ITERATIONS;
    
    // Allocate memory
    input = (double*)malloc(data_length * sizeof(double));
    desired = (double*)malloc(data_length * sizeof(double));
    output = (double*)malloc(data_length * sizeof(double));
    times = (double*)malloc(BENCHMARK_ITERATIONS * sizeof(double));
    
    if (!input || !desired || !output || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate test signals
    for (size_t i = 0; i < data_length; i++) {
        double t = (double)i / data_length;
        input[i] = sin(2.0 * FSO_PI * 10.0 * t) + 
                  fso_random_gaussian(0.0, 0.1);
        desired[i] = sin(2.0 * FSO_PI * 10.0 * t);
    }
    
    // Initialize signal processor with filter
    if (sp_init(&sp, num_threads, data_length) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    sp.filter_length = filter_length;
    sp.filter_coeffs = (double*)calloc(filter_length, sizeof(double));
    if (!sp.filter_coeffs) {
        result = FSO_ERROR_MEMORY;
        sp_free(&sp);
        goto cleanup;
    }
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        sp_adaptive_filter(&sp, input, desired, output, data_length, 0.01);
    }
    
    // Benchmark
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        // Reset filter coefficients
        memset(sp.filter_coeffs, 0, filter_length * sizeof(double));
        
        benchmark_timer_start(&timer);
        
        int filter_result = sp_adaptive_filter(&sp, input, desired, output,
                                              data_length, 0.01);
        
        benchmark_timer_stop(&timer);
        
        if (filter_result == FSO_SUCCESS) {
            metrics->success_count++;
        } else {
            metrics->failure_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    // Calculate statistics
    benchmark_calculate_statistics(times, BENCHMARK_ITERATIONS,
                                   &metrics->avg_time_ms,
                                   &metrics->stddev_time_ms,
                                   &metrics->min_time_ms,
                                   &metrics->max_time_ms);
    
    // Calculate throughput
    metrics->throughput_samples_sec = 
        benchmark_calculate_throughput_samples(data_length, metrics->avg_time_ms);
    
    sp_free(&sp);
    
cleanup:
    free(input);
    free(desired);
    free(output);
    free(times);
    
    return result;
}

/**
 * @brief Benchmark adaptive filter with various configurations
 */
int benchmark_adaptive_filter(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  Adaptive Filter Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    int max_threads = benchmark_get_num_cores();
    
    // Test different data lengths
    printf("Testing different data lengths (filter length = 32):\n");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < NUM_DATA_LENGTHS; i++) {
        size_t data_length = DATA_LENGTHS[i];
        
        // Serial
        PerformanceMetrics serial_metrics;
        run_adaptive_filter_benchmark(data_length, 32, 1, &serial_metrics);
        
        // Parallel
        PerformanceMetrics parallel_metrics;
        run_adaptive_filter_benchmark(data_length, 32, max_threads,
                                     &parallel_metrics);
        
        parallel_metrics.speedup_factor = 
            benchmark_calculate_speedup(serial_metrics.avg_time_ms,
                                       parallel_metrics.avg_time_ms);
        
        printf("  Length %6zu: Serial %.3f ms, Parallel %.3f ms (%.2fx speedup)\n",
               data_length,
               serial_metrics.avg_time_ms,
               parallel_metrics.avg_time_ms,
               parallel_metrics.speedup_factor);
    }
    
    printf("\n");
    
    // Test different filter lengths
    printf("Testing different filter lengths (data length = 16384):\n");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < NUM_FILTER_LENGTHS; i++) {
        int filter_length = FILTER_LENGTHS[i];
        
        // Serial
        PerformanceMetrics serial_metrics;
        run_adaptive_filter_benchmark(16384, filter_length, 1, &serial_metrics);
        
        // Parallel
        PerformanceMetrics parallel_metrics;
        run_adaptive_filter_benchmark(16384, filter_length, max_threads,
                                     &parallel_metrics);
        
        parallel_metrics.speedup_factor = 
            benchmark_calculate_speedup(serial_metrics.avg_time_ms,
                                       parallel_metrics.avg_time_ms);
        
        printf("  Filter %3d: Serial %.3f ms, Parallel %.3f ms (%.2fx speedup)\n",
               filter_length,
               serial_metrics.avg_time_ms,
               parallel_metrics.avg_time_ms,
               parallel_metrics.speedup_factor);
    }
    
    printf("\n");
    return FSO_SUCCESS;
}

/* ============================================================================
 * Convolution Benchmark
 * ============================================================================ */

/**
 * @brief Run convolution benchmark
 */
static int run_convolution_benchmark(size_t signal_length, size_t kernel_length,
                                    int num_threads,
                                    PerformanceMetrics* metrics) {
    SignalProcessor sp;
    double* signal = NULL;
    double* kernel = NULL;
    double* output = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    
    size_t output_length = signal_length + kernel_length - 1;
    
    // Initialize metrics
    benchmark_metrics_init(metrics);
    metrics->num_threads = num_threads;
    metrics->data_size_bytes = signal_length * sizeof(double);
    metrics->iterations = BENCHMARK_ITERATIONS;
    
    // Allocate memory
    signal = (double*)malloc(signal_length * sizeof(double));
    kernel = (double*)malloc(kernel_length * sizeof(double));
    output = (double*)malloc(output_length * sizeof(double));
    times = (double*)malloc(BENCHMARK_ITERATIONS * sizeof(double));
    
    if (!signal || !kernel || !output || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate test data
    for (size_t i = 0; i < signal_length; i++) {
        signal[i] = sin(2.0 * FSO_PI * i / 100.0);
    }
    
    // Generate Gaussian kernel
    double sigma = kernel_length / 6.0;
    double sum = 0.0;
    for (size_t i = 0; i < kernel_length; i++) {
        double x = (double)i - (kernel_length - 1) / 2.0;
        kernel[i] = exp(-x * x / (2.0 * sigma * sigma));
        sum += kernel[i];
    }
    // Normalize
    for (size_t i = 0; i < kernel_length; i++) {
        kernel[i] /= sum;
    }
    
    // Initialize signal processor
    if (sp_init(&sp, num_threads, signal_length) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        sp_convolution(&sp, signal, kernel, output, 
                      signal_length, kernel_length);
    }
    
    // Benchmark
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        benchmark_timer_start(&timer);
        
        int conv_result = sp_convolution(&sp, signal, kernel, output,
                                        signal_length, kernel_length);
        
        benchmark_timer_stop(&timer);
        
        if (conv_result == FSO_SUCCESS) {
            metrics->success_count++;
        } else {
            metrics->failure_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    // Calculate statistics
    benchmark_calculate_statistics(times, BENCHMARK_ITERATIONS,
                                   &metrics->avg_time_ms,
                                   &metrics->stddev_time_ms,
                                   &metrics->min_time_ms,
                                   &metrics->max_time_ms);
    
    // Calculate throughput
    metrics->throughput_samples_sec = 
        benchmark_calculate_throughput_samples(signal_length, metrics->avg_time_ms);
    
    sp_free(&sp);
    
cleanup:
    free(signal);
    free(kernel);
    free(output);
    free(times);
    
    return result;
}

/**
 * @brief Benchmark convolution with various configurations
 */
int benchmark_convolution(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  Convolution Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    int max_threads = benchmark_get_num_cores();
    
    printf("Testing convolution performance:\n");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < NUM_DATA_LENGTHS; i++) {
        size_t signal_length = DATA_LENGTHS[i];
        size_t kernel_length = 128;
        
        // Serial
        PerformanceMetrics serial_metrics;
        run_convolution_benchmark(signal_length, kernel_length, 1, 
                                 &serial_metrics);
        
        // Parallel
        PerformanceMetrics parallel_metrics;
        run_convolution_benchmark(signal_length, kernel_length, max_threads,
                                 &parallel_metrics);
        
        parallel_metrics.speedup_factor = 
            benchmark_calculate_speedup(serial_metrics.avg_time_ms,
                                       parallel_metrics.avg_time_ms);
        
        printf("  Signal %6zu: Serial %.3f ms, Parallel %.3f ms (%.2fx speedup)\n",
               signal_length,
               serial_metrics.avg_time_ms,
               parallel_metrics.avg_time_ms,
               parallel_metrics.speedup_factor);
    }
    
    printf("\n");
    return FSO_SUCCESS;
}

/**
 * @brief Run all filter benchmarks
 */
int benchmark_filters_comprehensive(void) {
    int result;
    
    result = benchmark_moving_average();
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    result = benchmark_adaptive_filter();
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    result = benchmark_convolution();
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    return FSO_SUCCESS;
}
