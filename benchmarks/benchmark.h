/**
 * @file benchmark.h
 * @brief Benchmarking infrastructure for FSO Communication Suite
 * 
 * Provides high-resolution timing, memory tracking, and performance metrics
 * collection for benchmarking various components of the FSO system.
 */

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "../src/fso.h"
#include <stdint.h>
#include <time.h>

/* ============================================================================
 * Performance Metrics Structure
 * ============================================================================ */

/**
 * @brief Performance metrics for a benchmark run
 */
typedef struct {
    // Timing metrics
    double execution_time_ms;      /**< Total execution time in milliseconds */
    double min_time_ms;            /**< Minimum execution time across iterations */
    double max_time_ms;            /**< Maximum execution time across iterations */
    double avg_time_ms;            /**< Average execution time */
    double stddev_time_ms;         /**< Standard deviation of execution time */
    
    // Throughput metrics
    double throughput_mbps;        /**< Throughput in megabits per second */
    double throughput_samples_sec; /**< Throughput in samples per second */
    double throughput_ops_sec;     /**< Throughput in operations per second */
    
    // Memory metrics
    size_t peak_memory_bytes;      /**< Peak memory usage in bytes */
    size_t avg_memory_bytes;       /**< Average memory usage in bytes */
    
    // Speedup metrics (for parallel benchmarks)
    double speedup_factor;         /**< Speedup compared to serial version */
    double parallel_efficiency;    /**< Parallel efficiency (speedup / num_threads) */
    int num_threads;               /**< Number of threads used */
    
    // Additional metrics
    size_t data_size_bytes;        /**< Size of data processed */
    int iterations;                /**< Number of iterations performed */
    int success_count;             /**< Number of successful operations */
    int failure_count;             /**< Number of failed operations */
} PerformanceMetrics;

/**
 * @brief Benchmark configuration
 */
typedef struct {
    const char* name;              /**< Benchmark name */
    int iterations;                /**< Number of iterations to run */
    int warmup_iterations;         /**< Number of warmup iterations */
    int num_threads;               /**< Number of threads (0 = auto-detect) */
    size_t data_size;              /**< Size of data to process */
    int verbose;                   /**< Verbose output flag */
} BenchmarkConfig;

/**
 * @brief Benchmark timer for high-resolution timing
 */
typedef struct {
    struct timespec start_time;    /**< Start timestamp */
    struct timespec end_time;      /**< End timestamp */
    int is_running;                /**< Timer running flag */
} BenchmarkTimer;

/* ============================================================================
 * Timer Functions
 * ============================================================================ */

/**
 * @brief Initialize a benchmark timer
 * @param timer Pointer to timer structure
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_timer_init(BenchmarkTimer* timer);

/**
 * @brief Start the benchmark timer
 * @param timer Pointer to timer structure
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_timer_start(BenchmarkTimer* timer);

/**
 * @brief Stop the benchmark timer
 * @param timer Pointer to timer structure
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_timer_stop(BenchmarkTimer* timer);

/**
 * @brief Get elapsed time in milliseconds
 * @param timer Pointer to timer structure
 * @return Elapsed time in milliseconds
 */
double benchmark_timer_elapsed_ms(const BenchmarkTimer* timer);

/**
 * @brief Get elapsed time in microseconds
 * @param timer Pointer to timer structure
 * @return Elapsed time in microseconds
 */
double benchmark_timer_elapsed_us(const BenchmarkTimer* timer);

/**
 * @brief Get elapsed time in nanoseconds
 * @param timer Pointer to timer structure
 * @return Elapsed time in nanoseconds
 */
uint64_t benchmark_timer_elapsed_ns(const BenchmarkTimer* timer);

/* ============================================================================
 * Memory Tracking Functions
 * ============================================================================ */

/**
 * @brief Get current memory usage of the process
 * @return Memory usage in bytes, or 0 if unavailable
 */
size_t benchmark_get_memory_usage(void);

/**
 * @brief Get peak memory usage of the process
 * @return Peak memory usage in bytes, or 0 if unavailable
 */
size_t benchmark_get_peak_memory_usage(void);

/* ============================================================================
 * Metrics Functions
 * ============================================================================ */

/**
 * @brief Initialize performance metrics structure
 * @param metrics Pointer to metrics structure
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_metrics_init(PerformanceMetrics* metrics);

/**
 * @brief Calculate throughput in megabits per second
 * @param data_size_bytes Size of data processed in bytes
 * @param time_ms Time taken in milliseconds
 * @return Throughput in Mbps
 */
double benchmark_calculate_throughput_mbps(size_t data_size_bytes, double time_ms);

/**
 * @brief Calculate throughput in samples per second
 * @param num_samples Number of samples processed
 * @param time_ms Time taken in milliseconds
 * @return Throughput in samples/sec
 */
double benchmark_calculate_throughput_samples(size_t num_samples, double time_ms);

/**
 * @brief Calculate speedup factor
 * @param serial_time_ms Serial execution time in milliseconds
 * @param parallel_time_ms Parallel execution time in milliseconds
 * @return Speedup factor
 */
double benchmark_calculate_speedup(double serial_time_ms, double parallel_time_ms);

/**
 * @brief Calculate parallel efficiency
 * @param speedup Speedup factor
 * @param num_threads Number of threads used
 * @return Parallel efficiency (0.0 to 1.0)
 */
double benchmark_calculate_efficiency(double speedup, int num_threads);

/**
 * @brief Calculate statistics from multiple timing measurements
 * @param times Array of timing measurements in milliseconds
 * @param count Number of measurements
 * @param avg Output: average time
 * @param stddev Output: standard deviation
 * @param min Output: minimum time
 * @param max Output: maximum time
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_calculate_statistics(const double* times, int count,
                                   double* avg, double* stddev,
                                   double* min, double* max);

/* ============================================================================
 * Logging and Reporting Functions
 * ============================================================================ */

/**
 * @brief Print performance metrics to stdout
 * @param metrics Pointer to metrics structure
 * @param config Pointer to benchmark configuration
 */
void benchmark_print_metrics(const PerformanceMetrics* metrics,
                            const BenchmarkConfig* config);

/**
 * @brief Print benchmark header
 * @param config Pointer to benchmark configuration
 */
void benchmark_print_header(const BenchmarkConfig* config);

/**
 * @brief Print benchmark summary
 * @param metrics Array of metrics for different configurations
 * @param count Number of metrics entries
 * @param title Summary title
 */
void benchmark_print_summary(const PerformanceMetrics* metrics,
                            int count, const char* title);

/**
 * @brief Save metrics to CSV file
 * @param filename Output filename
 * @param metrics Array of metrics
 * @param count Number of metrics entries
 * @param labels Array of labels for each metric entry
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_save_csv(const char* filename,
                      const PerformanceMetrics* metrics,
                      int count,
                      const char** labels);

/**
 * @brief Save metrics to JSON file
 * @param filename Output filename
 * @param metrics Array of metrics
 * @param count Number of metrics entries
 * @param labels Array of labels for each metric entry
 * @return FSO_SUCCESS on success, error code otherwise
 */
int benchmark_save_json(const char* filename,
                       const PerformanceMetrics* metrics,
                       int count,
                       const char** labels);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Format bytes to human-readable string (KB, MB, GB)
 * @param bytes Number of bytes
 * @param buffer Output buffer (min 32 bytes)
 * @return Pointer to buffer
 */
char* benchmark_format_bytes(size_t bytes, char* buffer);

/**
 * @brief Format time to human-readable string (ms, us, ns)
 * @param time_ms Time in milliseconds
 * @param buffer Output buffer (min 32 bytes)
 * @return Pointer to buffer
 */
char* benchmark_format_time(double time_ms, char* buffer);

/**
 * @brief Get number of available CPU cores
 * @return Number of CPU cores, or 1 if unavailable
 */
int benchmark_get_num_cores(void);

#endif /* BENCHMARK_H */
