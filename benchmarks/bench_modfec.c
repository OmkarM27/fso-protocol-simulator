/**
 * @file bench_modfec.c
 * @brief Modulation and FEC throughput benchmarks
 * 
 * Benchmarks modulation schemes and FEC encoding/decoding with various
 * data sizes from 1KB to 100MB.
 */

#include "benchmark.h"
#include "../src/modulation/modulation.h"
#include "../src/fec/fec.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Benchmark Configuration
 * ============================================================================ */

static const size_t DATA_SIZES[] = {
    1024,           // 1 KB
    10240,          // 10 KB
    102400,         // 100 KB
    1048576,        // 1 MB
    10485760,       // 10 MB
    104857600       // 100 MB
};
static const int NUM_DATA_SIZES = 6;

static const int SMALL_ITERATIONS = 1000;
static const int MEDIUM_ITERATIONS = 100;
static const int LARGE_ITERATIONS = 10;

/* ============================================================================
 * Modulation Benchmarks
 * ============================================================================ */

/**
 * @brief Get number of iterations based on data size
 */
static int get_iterations_for_size(size_t data_size) {
    if (data_size < 100000) {
        return SMALL_ITERATIONS;
    } else if (data_size < 10000000) {
        return MEDIUM_ITERATIONS;
    } else {
        return LARGE_ITERATIONS;
    }
}

/**
 * @brief Benchmark OOK modulation
 */
static int benchmark_ook_modulation(size_t data_size, 
                                   PerformanceMetrics* encode_metrics,
                                   PerformanceMetrics* decode_metrics) {
    Modulator mod;
    uint8_t* data = NULL;
    double* symbols = NULL;
    uint8_t* decoded = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    int iterations = get_iterations_for_size(data_size);
    
    // Allocate memory
    data = (uint8_t*)malloc(data_size);
    symbols = (double*)malloc(data_size * 8 * sizeof(double));
    decoded = (uint8_t*)malloc(data_size);
    times = (double*)malloc(iterations * sizeof(double));
    
    if (!data || !symbols || !decoded || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate random data
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (uint8_t)fso_random_int(0, 255);
    }
    
    // Initialize modulator
    if (modulator_init(&mod, MOD_OOK, 1e6) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Benchmark encoding
    benchmark_metrics_init(encode_metrics);
    encode_metrics->data_size_bytes = data_size;
    encode_metrics->iterations = iterations;
    
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < iterations; i++) {
        size_t symbol_len;
        
        benchmark_timer_start(&timer);
        int mod_result = modulate(&mod, data, data_size, symbols, &symbol_len);
        benchmark_timer_stop(&timer);
        
        if (mod_result == FSO_SUCCESS) {
            encode_metrics->success_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &encode_metrics->avg_time_ms,
                                   &encode_metrics->stddev_time_ms,
                                   &encode_metrics->min_time_ms,
                                   &encode_metrics->max_time_ms);
    
    encode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, encode_metrics->avg_time_ms);
    
    // Benchmark decoding
    size_t symbol_len;
    modulate(&mod, data, data_size, symbols, &symbol_len);
    
    benchmark_metrics_init(decode_metrics);
    decode_metrics->data_size_bytes = data_size;
    decode_metrics->iterations = iterations;
    
    for (int i = 0; i < iterations; i++) {
        size_t decoded_len;
        
        benchmark_timer_start(&timer);
        int demod_result = demodulate(&mod, symbols, symbol_len, 
                                     decoded, &decoded_len, 20.0);
        benchmark_timer_stop(&timer);
        
        if (demod_result == FSO_SUCCESS) {
            decode_metrics->success_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &decode_metrics->avg_time_ms,
                                   &decode_metrics->stddev_time_ms,
                                   &decode_metrics->min_time_ms,
                                   &decode_metrics->max_time_ms);
    
    decode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, decode_metrics->avg_time_ms);
    
    modulator_free(&mod);
    
cleanup:
    free(data);
    free(symbols);
    free(decoded);
    free(times);
    
    return result;
}

/**
 * @brief Benchmark PPM modulation
 */
static int benchmark_ppm_modulation(size_t data_size, int ppm_order,
                                   PerformanceMetrics* encode_metrics,
                                   PerformanceMetrics* decode_metrics) {
    Modulator mod;
    uint8_t* data = NULL;
    double* symbols = NULL;
    uint8_t* decoded = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    int iterations = get_iterations_for_size(data_size);
    
    // Allocate memory
    data = (uint8_t*)malloc(data_size);
    symbols = (double*)malloc(data_size * ppm_order * 8 * sizeof(double));
    decoded = (uint8_t*)malloc(data_size);
    times = (double*)malloc(iterations * sizeof(double));
    
    if (!data || !symbols || !decoded || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate random data
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (uint8_t)fso_random_int(0, 255);
    }
    
    // Initialize modulator
    if (modulator_init_ppm(&mod, 1e6, ppm_order) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Benchmark encoding
    benchmark_metrics_init(encode_metrics);
    encode_metrics->data_size_bytes = data_size;
    encode_metrics->iterations = iterations;
    
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < iterations; i++) {
        size_t symbol_len;
        
        benchmark_timer_start(&timer);
        int mod_result = modulate(&mod, data, data_size, symbols, &symbol_len);
        benchmark_timer_stop(&timer);
        
        if (mod_result == FSO_SUCCESS) {
            encode_metrics->success_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &encode_metrics->avg_time_ms,
                                   &encode_metrics->stddev_time_ms,
                                   &encode_metrics->min_time_ms,
                                   &encode_metrics->max_time_ms);
    
    encode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, encode_metrics->avg_time_ms);
    
    // Benchmark decoding
    size_t symbol_len;
    modulate(&mod, data, data_size, symbols, &symbol_len);
    
    benchmark_metrics_init(decode_metrics);
    decode_metrics->data_size_bytes = data_size;
    decode_metrics->iterations = iterations;
    
    for (int i = 0; i < iterations; i++) {
        size_t decoded_len;
        
        benchmark_timer_start(&timer);
        int demod_result = demodulate(&mod, symbols, symbol_len,
                                     decoded, &decoded_len, 20.0);
        benchmark_timer_stop(&timer);
        
        if (demod_result == FSO_SUCCESS) {
            decode_metrics->success_count++;
        }
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &decode_metrics->avg_time_ms,
                                   &decode_metrics->stddev_time_ms,
                                   &decode_metrics->min_time_ms,
                                   &decode_metrics->max_time_ms);
    
    decode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, decode_metrics->avg_time_ms);
    
    modulator_free(&mod);
    
cleanup:
    free(data);
    free(symbols);
    free(decoded);
    free(times);
    
    return result;
}

/**
 * @brief Run comprehensive modulation benchmarks
 */
int benchmark_modulation_comprehensive(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  Modulation Throughput Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    // OOK benchmarks
    printf("On-Off Keying (OOK):\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("%-12s %15s %15s %15s %15s\n", 
           "Data Size", "Encode (ms)", "Encode (Mbps)", "Decode (ms)", "Decode (Mbps)");
    printf("%-12s %15s %15s %15s %15s\n",
           "------------", "---------------", "---------------", "---------------", "---------------");
    
    for (int i = 0; i < NUM_DATA_SIZES; i++) {
        size_t data_size = DATA_SIZES[i];
        PerformanceMetrics encode_metrics, decode_metrics;
        
        if (benchmark_ook_modulation(data_size, &encode_metrics, 
                                    &decode_metrics) != FSO_SUCCESS) {
            continue;
        }
        
        char size_buf[32];
        benchmark_format_bytes(data_size, size_buf);
        
        printf("%-12s %15.3f %15.2f %15.3f %15.2f\n",
               size_buf,
               encode_metrics.avg_time_ms,
               encode_metrics.throughput_mbps,
               decode_metrics.avg_time_ms,
               decode_metrics.throughput_mbps);
    }
    
    printf("\n");
    
    // PPM benchmarks
    int ppm_orders[] = {2, 4, 8, 16};
    for (int order_idx = 0; order_idx < 4; order_idx++) {
        int ppm_order = ppm_orders[order_idx];
        
        printf("%d-PPM:\n", ppm_order);
        printf("--------------------------------------------------------------------------------\n");
        printf("%-12s %15s %15s %15s %15s\n",
               "Data Size", "Encode (ms)", "Encode (Mbps)", "Decode (ms)", "Decode (Mbps)");
        printf("%-12s %15s %15s %15s %15s\n",
               "------------", "---------------", "---------------", "---------------", "---------------");
        
        for (int i = 0; i < NUM_DATA_SIZES; i++) {
            size_t data_size = DATA_SIZES[i];
            PerformanceMetrics encode_metrics, decode_metrics;
            
            if (benchmark_ppm_modulation(data_size, ppm_order,
                                        &encode_metrics, 
                                        &decode_metrics) != FSO_SUCCESS) {
                continue;
            }
            
            char size_buf[32];
            benchmark_format_bytes(data_size, size_buf);
            
            printf("%-12s %15.3f %15.2f %15.3f %15.2f\n",
                   size_buf,
                   encode_metrics.avg_time_ms,
                   encode_metrics.throughput_mbps,
                   decode_metrics.avg_time_ms,
                   decode_metrics.throughput_mbps);
        }
        
        printf("\n");
    }
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * FEC Benchmarks
 * ============================================================================ */

/**
 * @brief Benchmark Reed-Solomon FEC
 */
static int benchmark_reed_solomon(size_t data_size,
                                 PerformanceMetrics* encode_metrics,
                                 PerformanceMetrics* decode_metrics) {
    FECCodec codec;
    uint8_t* data = NULL;
    uint8_t* encoded = NULL;
    uint8_t* decoded = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    int iterations = get_iterations_for_size(data_size);
    
    // RS(255, 223) - 32 parity symbols
    int data_len = 223;
    int code_len = 255;
    int num_blocks = (data_size + data_len - 1) / data_len;
    
    // Allocate memory
    data = (uint8_t*)malloc(num_blocks * data_len);
    encoded = (uint8_t*)malloc(num_blocks * code_len);
    decoded = (uint8_t*)malloc(num_blocks * data_len);
    times = (double*)malloc(iterations * sizeof(double));
    
    if (!data || !encoded || !decoded || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate random data
    for (size_t i = 0; i < num_blocks * data_len; i++) {
        data[i] = (uint8_t)fso_random_int(0, 255);
    }
    
    // Initialize codec
    RSConfig rs_config = {
        .symbol_size = 8,
        .num_roots = 32,
        .first_root = 1,
        .primitive_poly = 0x11d,
        .fcr = 1
    };
    
    if (fec_init(&codec, FEC_REED_SOLOMON, data_len, code_len, 
                &rs_config) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Benchmark encoding
    benchmark_metrics_init(encode_metrics);
    encode_metrics->data_size_bytes = data_size;
    encode_metrics->iterations = iterations;
    
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < iterations; i++) {
        benchmark_timer_start(&timer);
        
        for (int block = 0; block < num_blocks; block++) {
            size_t encoded_len;
            fec_encode(&codec, data + block * data_len, data_len,
                      encoded + block * code_len, &encoded_len);
        }
        
        benchmark_timer_stop(&timer);
        encode_metrics->success_count++;
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &encode_metrics->avg_time_ms,
                                   &encode_metrics->stddev_time_ms,
                                   &encode_metrics->min_time_ms,
                                   &encode_metrics->max_time_ms);
    
    encode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, encode_metrics->avg_time_ms);
    
    // Encode once for decoding benchmark
    for (int block = 0; block < num_blocks; block++) {
        size_t encoded_len;
        fec_encode(&codec, data + block * data_len, data_len,
                  encoded + block * code_len, &encoded_len);
    }
    
    // Benchmark decoding
    benchmark_metrics_init(decode_metrics);
    decode_metrics->data_size_bytes = data_size;
    decode_metrics->iterations = iterations;
    
    for (int i = 0; i < iterations; i++) {
        benchmark_timer_start(&timer);
        
        for (int block = 0; block < num_blocks; block++) {
            size_t decoded_len;
            FECStats stats;
            fec_decode(&codec, encoded + block * code_len, code_len,
                      decoded + block * data_len, &decoded_len, &stats);
        }
        
        benchmark_timer_stop(&timer);
        decode_metrics->success_count++;
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &decode_metrics->avg_time_ms,
                                   &decode_metrics->stddev_time_ms,
                                   &decode_metrics->min_time_ms,
                                   &decode_metrics->max_time_ms);
    
    decode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, decode_metrics->avg_time_ms);
    
    fec_free(&codec);
    
cleanup:
    free(data);
    free(encoded);
    free(decoded);
    free(times);
    
    return result;
}

/**
 * @brief Benchmark LDPC FEC
 */
static int benchmark_ldpc(size_t data_size,
                         PerformanceMetrics* encode_metrics,
                         PerformanceMetrics* decode_metrics) {
    FECCodec codec;
    uint8_t* data = NULL;
    uint8_t* encoded = NULL;
    uint8_t* decoded = NULL;
    double* times = NULL;
    int result = FSO_SUCCESS;
    int iterations = get_iterations_for_size(data_size);
    
    // LDPC(1024, 512) - rate 1/2
    int data_len = 512;
    int code_len = 1024;
    int num_blocks = (data_size + data_len - 1) / data_len;
    
    // Allocate memory
    data = (uint8_t*)malloc(num_blocks * data_len);
    encoded = (uint8_t*)malloc(num_blocks * code_len);
    decoded = (uint8_t*)malloc(num_blocks * data_len);
    times = (double*)malloc(iterations * sizeof(double));
    
    if (!data || !encoded || !decoded || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Generate random data
    for (size_t i = 0; i < num_blocks * data_len; i++) {
        data[i] = (uint8_t)fso_random_int(0, 255);
    }
    
    // Initialize codec
    LDPCConfig ldpc_config = {
        .num_variable_nodes = code_len,
        .num_check_nodes = code_len - data_len,
        .max_iterations = 50,
        .convergence_threshold = 0.001,
        .parity_check_matrix = NULL,
        .matrix_rows = code_len - data_len,
        .matrix_cols = code_len
    };
    
    if (fec_init(&codec, FEC_LDPC, data_len, code_len,
                &ldpc_config) != FSO_SUCCESS) {
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    
    // Benchmark encoding
    benchmark_metrics_init(encode_metrics);
    encode_metrics->data_size_bytes = data_size;
    encode_metrics->iterations = iterations;
    
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    for (int i = 0; i < iterations; i++) {
        benchmark_timer_start(&timer);
        
        for (int block = 0; block < num_blocks; block++) {
            size_t encoded_len;
            fec_encode(&codec, data + block * data_len, data_len,
                      encoded + block * code_len, &encoded_len);
        }
        
        benchmark_timer_stop(&timer);
        encode_metrics->success_count++;
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &encode_metrics->avg_time_ms,
                                   &encode_metrics->stddev_time_ms,
                                   &encode_metrics->min_time_ms,
                                   &encode_metrics->max_time_ms);
    
    encode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, encode_metrics->avg_time_ms);
    
    // Encode once for decoding benchmark
    for (int block = 0; block < num_blocks; block++) {
        size_t encoded_len;
        fec_encode(&codec, data + block * data_len, data_len,
                  encoded + block * code_len, &encoded_len);
    }
    
    // Benchmark decoding
    benchmark_metrics_init(decode_metrics);
    decode_metrics->data_size_bytes = data_size;
    decode_metrics->iterations = iterations;
    
    for (int i = 0; i < iterations; i++) {
        benchmark_timer_start(&timer);
        
        for (int block = 0; block < num_blocks; block++) {
            size_t decoded_len;
            FECStats stats;
            fec_decode(&codec, encoded + block * code_len, code_len,
                      decoded + block * data_len, &decoded_len, &stats);
        }
        
        benchmark_timer_stop(&timer);
        decode_metrics->success_count++;
        times[i] = benchmark_timer_elapsed_ms(&timer);
    }
    
    benchmark_calculate_statistics(times, iterations,
                                   &decode_metrics->avg_time_ms,
                                   &decode_metrics->stddev_time_ms,
                                   &decode_metrics->min_time_ms,
                                   &decode_metrics->max_time_ms);
    
    decode_metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(data_size, decode_metrics->avg_time_ms);
    
    fec_free(&codec);
    
cleanup:
    free(data);
    free(encoded);
    free(decoded);
    free(times);
    
    return result;
}

/**
 * @brief Run comprehensive FEC benchmarks
 */
int benchmark_fec_comprehensive(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  FEC Throughput Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    // Reed-Solomon benchmarks
    printf("Reed-Solomon RS(255, 223):\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("%-12s %15s %15s %15s %15s\n",
           "Data Size", "Encode (ms)", "Encode (Mbps)", "Decode (ms)", "Decode (Mbps)");
    printf("%-12s %15s %15s %15s %15s\n",
           "------------", "---------------", "---------------", "---------------", "---------------");
    
    for (int i = 0; i < NUM_DATA_SIZES; i++) {
        size_t data_size = DATA_SIZES[i];
        PerformanceMetrics encode_metrics, decode_metrics;
        
        if (benchmark_reed_solomon(data_size, &encode_metrics,
                                  &decode_metrics) != FSO_SUCCESS) {
            continue;
        }
        
        char size_buf[32];
        benchmark_format_bytes(data_size, size_buf);
        
        printf("%-12s %15.3f %15.2f %15.3f %15.2f\n",
               size_buf,
               encode_metrics.avg_time_ms,
               encode_metrics.throughput_mbps,
               decode_metrics.avg_time_ms,
               decode_metrics.throughput_mbps);
    }
    
    printf("\n");
    
    // LDPC benchmarks
    printf("LDPC(1024, 512) - Rate 1/2:\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("%-12s %15s %15s %15s %15s\n",
           "Data Size", "Encode (ms)", "Encode (Mbps)", "Decode (ms)", "Decode (Mbps)");
    printf("%-12s %15s %15s %15s %15s\n",
           "------------", "---------------", "---------------", "---------------", "---------------");
    
    for (int i = 0; i < NUM_DATA_SIZES; i++) {
        size_t data_size = DATA_SIZES[i];
        PerformanceMetrics encode_metrics, decode_metrics;
        
        if (benchmark_ldpc(data_size, &encode_metrics,
                          &decode_metrics) != FSO_SUCCESS) {
            continue;
        }
        
        char size_buf[32];
        benchmark_format_bytes(data_size, size_buf);
        
        printf("%-12s %15.3f %15.2f %15.3f %15.2f\n",
               size_buf,
               encode_metrics.avg_time_ms,
               encode_metrics.throughput_mbps,
               decode_metrics.avg_time_ms,
               decode_metrics.throughput_mbps);
    }
    
    printf("\n");
    return FSO_SUCCESS;
}
