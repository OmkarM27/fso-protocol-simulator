/**
 * @file benchmark.c
 * @brief Implementation of benchmarking infrastructure
 */

#include "benchmark.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

/* ============================================================================
 * Timer Functions
 * ============================================================================ */

int benchmark_timer_init(BenchmarkTimer* timer) {
    FSO_CHECK_NULL(timer);
    
    memset(timer, 0, sizeof(BenchmarkTimer));
    timer->is_running = 0;
    
    return FSO_SUCCESS;
}

int benchmark_timer_start(BenchmarkTimer* timer) {
    FSO_CHECK_NULL(timer);
    
    if (clock_gettime(CLOCK_MONOTONIC, &timer->start_time) != 0) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to get start time");
        return FSO_ERROR_IO;
    }
    
    timer->is_running = 1;
    return FSO_SUCCESS;
}

int benchmark_timer_stop(BenchmarkTimer* timer) {
    FSO_CHECK_NULL(timer);
    
    if (!timer->is_running) {
        FSO_LOG_WARNING("BENCHMARK", "Timer not running");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (clock_gettime(CLOCK_MONOTONIC, &timer->end_time) != 0) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to get end time");
        return FSO_ERROR_IO;
    }
    
    timer->is_running = 0;
    return FSO_SUCCESS;
}

double benchmark_timer_elapsed_ms(const BenchmarkTimer* timer) {
    if (timer == NULL) return 0.0;
    
    uint64_t ns = benchmark_timer_elapsed_ns(timer);
    return ns / 1000000.0;
}

double benchmark_timer_elapsed_us(const BenchmarkTimer* timer) {
    if (timer == NULL) return 0.0;
    
    uint64_t ns = benchmark_timer_elapsed_ns(timer);
    return ns / 1000.0;
}

uint64_t benchmark_timer_elapsed_ns(const BenchmarkTimer* timer) {
    if (timer == NULL) return 0;
    
    struct timespec end = timer->is_running ? timer->start_time : timer->end_time;
    if (timer->is_running) {
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    
    uint64_t start_ns = (uint64_t)timer->start_time.tv_sec * 1000000000ULL + 
                        timer->start_time.tv_nsec;
    uint64_t end_ns = (uint64_t)end.tv_sec * 1000000000ULL + end.tv_nsec;
    
    return end_ns - start_ns;
}

/* ============================================================================
 * Memory Tracking Functions
 * ============================================================================ */

size_t benchmark_get_memory_usage(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss is in kilobytes on Linux, bytes on macOS
        #ifdef __APPLE__
        return usage.ru_maxrss;
        #else
        return usage.ru_maxrss * 1024;
        #endif
    }
    return 0;
#endif
}

size_t benchmark_get_peak_memory_usage(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        #ifdef __APPLE__
        return usage.ru_maxrss;
        #else
        return usage.ru_maxrss * 1024;
        #endif
    }
    return 0;
#endif
}

/* ============================================================================
 * Metrics Functions
 * ============================================================================ */

int benchmark_metrics_init(PerformanceMetrics* metrics) {
    FSO_CHECK_NULL(metrics);
    
    memset(metrics, 0, sizeof(PerformanceMetrics));
    metrics->min_time_ms = INFINITY;
    metrics->max_time_ms = 0.0;
    
    return FSO_SUCCESS;
}

double benchmark_calculate_throughput_mbps(size_t data_size_bytes, double time_ms) {
    if (time_ms <= 0.0) return 0.0;
    
    double bits = data_size_bytes * 8.0;
    double seconds = time_ms / 1000.0;
    double mbps = (bits / seconds) / 1000000.0;
    
    return mbps;
}

double benchmark_calculate_throughput_samples(size_t num_samples, double time_ms) {
    if (time_ms <= 0.0) return 0.0;
    
    double seconds = time_ms / 1000.0;
    return num_samples / seconds;
}

double benchmark_calculate_speedup(double serial_time_ms, double parallel_time_ms) {
    if (parallel_time_ms <= 0.0) return 0.0;
    return serial_time_ms / parallel_time_ms;
}

double benchmark_calculate_efficiency(double speedup, int num_threads) {
    if (num_threads <= 0) return 0.0;
    return speedup / num_threads;
}

int benchmark_calculate_statistics(const double* times, int count,
                                   double* avg, double* stddev,
                                   double* min, double* max) {
    FSO_CHECK_NULL(times);
    FSO_CHECK_NULL(avg);
    FSO_CHECK_NULL(stddev);
    FSO_CHECK_NULL(min);
    FSO_CHECK_NULL(max);
    FSO_CHECK_PARAM(count > 0);
    
    // Calculate average
    double sum = 0.0;
    *min = times[0];
    *max = times[0];
    
    for (int i = 0; i < count; i++) {
        sum += times[i];
        if (times[i] < *min) *min = times[i];
        if (times[i] > *max) *max = times[i];
    }
    
    *avg = sum / count;
    
    // Calculate standard deviation
    double variance_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = times[i] - *avg;
        variance_sum += diff * diff;
    }
    
    *stddev = sqrt(variance_sum / count);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Logging and Reporting Functions
 * ============================================================================ */

void benchmark_print_header(const BenchmarkConfig* config) {
    if (config == NULL) return;
    
    printf("\n");
    printf("================================================================================\n");
    printf("  Benchmark: %s\n", config->name);
    printf("================================================================================\n");
    printf("  Iterations:        %d\n", config->iterations);
    printf("  Warmup iterations: %d\n", config->warmup_iterations);
    printf("  Threads:           %d\n", config->num_threads);
    
    char size_buf[32];
    benchmark_format_bytes(config->data_size, size_buf);
    printf("  Data size:         %s\n", size_buf);
    printf("================================================================================\n");
    printf("\n");
}

void benchmark_print_metrics(const PerformanceMetrics* metrics,
                            const BenchmarkConfig* config) {
    if (metrics == NULL) return;
    
    char time_buf[32], mem_buf[32];
    
    printf("Results:\n");
    printf("--------\n");
    
    // Timing metrics
    benchmark_format_time(metrics->avg_time_ms, time_buf);
    printf("  Average time:      %s\n", time_buf);
    
    benchmark_format_time(metrics->min_time_ms, time_buf);
    printf("  Min time:          %s\n", time_buf);
    
    benchmark_format_time(metrics->max_time_ms, time_buf);
    printf("  Max time:          %s\n", time_buf);
    
    benchmark_format_time(metrics->stddev_time_ms, time_buf);
    printf("  Std deviation:     %s\n", time_buf);
    
    // Throughput metrics
    if (metrics->throughput_mbps > 0.0) {
        printf("  Throughput:        %.2f Mbps\n", metrics->throughput_mbps);
    }
    
    if (metrics->throughput_samples_sec > 0.0) {
        printf("  Throughput:        %.2f Msamples/sec\n", 
               metrics->throughput_samples_sec / 1000000.0);
    }
    
    if (metrics->throughput_ops_sec > 0.0) {
        printf("  Throughput:        %.2f Mops/sec\n",
               metrics->throughput_ops_sec / 1000000.0);
    }
    
    // Memory metrics
    if (metrics->peak_memory_bytes > 0) {
        benchmark_format_bytes(metrics->peak_memory_bytes, mem_buf);
        printf("  Peak memory:       %s\n", mem_buf);
    }
    
    // Speedup metrics
    if (metrics->speedup_factor > 0.0) {
        printf("  Speedup:           %.2fx\n", metrics->speedup_factor);
        printf("  Efficiency:        %.1f%%\n", metrics->parallel_efficiency * 100.0);
        printf("  Threads:           %d\n", metrics->num_threads);
    }
    
    // Success/failure counts
    if (metrics->iterations > 0) {
        printf("  Success rate:      %d/%d (%.1f%%)\n",
               metrics->success_count, metrics->iterations,
               (metrics->success_count * 100.0) / metrics->iterations);
    }
    
    printf("\n");
}

void benchmark_print_summary(const PerformanceMetrics* metrics,
                            int count, const char* title) {
    if (metrics == NULL || count <= 0) return;
    
    printf("\n");
    printf("================================================================================\n");
    printf("  %s\n", title ? title : "Benchmark Summary");
    printf("================================================================================\n");
    printf("\n");
    
    printf("%-20s %12s %12s %12s %12s\n",
           "Configuration", "Avg Time", "Throughput", "Speedup", "Efficiency");
    printf("%-20s %12s %12s %12s %12s\n",
           "--------------------", "------------", "------------", "------------", "------------");
    
    for (int i = 0; i < count; i++) {
        char time_buf[32];
        benchmark_format_time(metrics[i].avg_time_ms, time_buf);
        
        char throughput_str[32] = "-";
        if (metrics[i].throughput_mbps > 0.0) {
            snprintf(throughput_str, sizeof(throughput_str), "%.2f Mbps", 
                    metrics[i].throughput_mbps);
        } else if (metrics[i].throughput_samples_sec > 0.0) {
            snprintf(throughput_str, sizeof(throughput_str), "%.2f MS/s",
                    metrics[i].throughput_samples_sec / 1000000.0);
        }
        
        char speedup_str[32] = "-";
        char efficiency_str[32] = "-";
        if (metrics[i].speedup_factor > 0.0) {
            snprintf(speedup_str, sizeof(speedup_str), "%.2fx", 
                    metrics[i].speedup_factor);
            snprintf(efficiency_str, sizeof(efficiency_str), "%.1f%%",
                    metrics[i].parallel_efficiency * 100.0);
        }
        
        printf("%-20s %12s %12s %12s %12s\n",
               metrics[i].num_threads > 0 ? 
                   (char[32]){0} : "Serial",
               time_buf, throughput_str, speedup_str, efficiency_str);
        
        if (metrics[i].num_threads > 0) {
            char label[32];
            snprintf(label, sizeof(label), "%d threads", metrics[i].num_threads);
            printf("%-20s %12s %12s %12s %12s\n",
                   label, time_buf, throughput_str, speedup_str, efficiency_str);
        }
    }
    
    printf("\n");
}

int benchmark_save_csv(const char* filename,
                      const PerformanceMetrics* metrics,
                      int count,
                      const char** labels) {
    FSO_CHECK_NULL(filename);
    FSO_CHECK_NULL(metrics);
    FSO_CHECK_PARAM(count > 0);
    
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to open file: %s", filename);
        return FSO_ERROR_IO;
    }
    
    // Write header
    fprintf(fp, "Label,Threads,AvgTime_ms,MinTime_ms,MaxTime_ms,StdDev_ms,");
    fprintf(fp, "Throughput_Mbps,Throughput_Samples_sec,Throughput_Ops_sec,");
    fprintf(fp, "PeakMemory_bytes,Speedup,Efficiency,Iterations,SuccessCount\n");
    
    // Write data
    for (int i = 0; i < count; i++) {
        const char* label = (labels && labels[i]) ? labels[i] : "Unknown";
        
        fprintf(fp, "%s,%d,%.6f,%.6f,%.6f,%.6f,",
                label,
                metrics[i].num_threads,
                metrics[i].avg_time_ms,
                metrics[i].min_time_ms,
                metrics[i].max_time_ms,
                metrics[i].stddev_time_ms);
        
        fprintf(fp, "%.6f,%.6f,%.6f,",
                metrics[i].throughput_mbps,
                metrics[i].throughput_samples_sec,
                metrics[i].throughput_ops_sec);
        
        fprintf(fp, "%zu,%.6f,%.6f,%d,%d\n",
                metrics[i].peak_memory_bytes,
                metrics[i].speedup_factor,
                metrics[i].parallel_efficiency,
                metrics[i].iterations,
                metrics[i].success_count);
    }
    
    fclose(fp);
    FSO_LOG_INFO("BENCHMARK", "Saved CSV results to: %s", filename);
    
    return FSO_SUCCESS;
}

int benchmark_save_json(const char* filename,
                       const PerformanceMetrics* metrics,
                       int count,
                       const char** labels) {
    FSO_CHECK_NULL(filename);
    FSO_CHECK_NULL(metrics);
    FSO_CHECK_PARAM(count > 0);
    
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to open file: %s", filename);
        return FSO_ERROR_IO;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"benchmarks\": [\n");
    
    for (int i = 0; i < count; i++) {
        const char* label = (labels && labels[i]) ? labels[i] : "Unknown";
        
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"label\": \"%s\",\n", label);
        fprintf(fp, "      \"threads\": %d,\n", metrics[i].num_threads);
        fprintf(fp, "      \"timing\": {\n");
        fprintf(fp, "        \"avg_ms\": %.6f,\n", metrics[i].avg_time_ms);
        fprintf(fp, "        \"min_ms\": %.6f,\n", metrics[i].min_time_ms);
        fprintf(fp, "        \"max_ms\": %.6f,\n", metrics[i].max_time_ms);
        fprintf(fp, "        \"stddev_ms\": %.6f\n", metrics[i].stddev_time_ms);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"throughput\": {\n");
        fprintf(fp, "        \"mbps\": %.6f,\n", metrics[i].throughput_mbps);
        fprintf(fp, "        \"samples_per_sec\": %.6f,\n", metrics[i].throughput_samples_sec);
        fprintf(fp, "        \"ops_per_sec\": %.6f\n", metrics[i].throughput_ops_sec);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"memory\": {\n");
        fprintf(fp, "        \"peak_bytes\": %zu\n", metrics[i].peak_memory_bytes);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"parallel\": {\n");
        fprintf(fp, "        \"speedup\": %.6f,\n", metrics[i].speedup_factor);
        fprintf(fp, "        \"efficiency\": %.6f\n", metrics[i].parallel_efficiency);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"iterations\": %d,\n", metrics[i].iterations);
        fprintf(fp, "      \"success_count\": %d\n", metrics[i].success_count);
        fprintf(fp, "    }%s\n", (i < count - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    FSO_LOG_INFO("BENCHMARK", "Saved JSON results to: %s", filename);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

char* benchmark_format_bytes(size_t bytes, char* buffer) {
    if (buffer == NULL) return NULL;
    
    if (bytes < 1024) {
        snprintf(buffer, 32, "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, 32, "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, 32, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, 32, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    
    return buffer;
}

char* benchmark_format_time(double time_ms, char* buffer) {
    if (buffer == NULL) return NULL;
    
    if (time_ms < 0.001) {
        snprintf(buffer, 32, "%.3f ns", time_ms * 1000000.0);
    } else if (time_ms < 1.0) {
        snprintf(buffer, 32, "%.3f us", time_ms * 1000.0);
    } else if (time_ms < 1000.0) {
        snprintf(buffer, 32, "%.3f ms", time_ms);
    } else {
        snprintf(buffer, 32, "%.3f s", time_ms / 1000.0);
    }
    
    return buffer;
}

int benchmark_get_num_cores(void) {
#ifdef _OPENMP
    return omp_get_num_procs();
#elif defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}
