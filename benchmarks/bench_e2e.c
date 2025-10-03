/**
 * @file bench_e2e.c
 * @brief End-to-end latency benchmarks
 * 
 * Measures complete transmit-receive cycle time and verifies real-time
 * requirements (< 10ms per frame) with different system configurations.
 */

#include "benchmark.h"
#include "../src/modulation/modulation.h"
#include "../src/fec/fec.h"
#include "../src/signal_processing/signal_processing.h"
#include "../src/turbulence/channel.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * End-to-End Configuration
 * ============================================================================ */

#define FRAME_SIZE_BYTES 1024
#define NUM_FRAMES 1000
#define WARMUP_FRAMES 10
#define REALTIME_THRESHOLD_MS 10.0

/**
 * @brief End-to-end system configuration
 */
typedef struct {
    ModulationType modulation;
    FECType fec_type;
    int use_signal_processing;
    int use_channel_model;
    WeatherCondition weather;
    double snr_db;
} E2EConfig;

/* ============================================================================
 * End-to-End Benchmark
 * ============================================================================ */

/**
 * @brief Run end-to-end latency benchmark
 */
static int run_e2e_benchmark(const E2EConfig* config,
                            PerformanceMetrics* metrics) {
    // Components
    Modulator mod;
    FECCodec fec;
    SignalProcessor sp;
    ChannelModel channel;
    
    // Buffers
    uint8_t* tx_data = NULL;
    uint8_t* fec_encoded = NULL;
    double* modulated = NULL;
    double* channel_output = NULL;
    uint8_t* fec_decoded = NULL;
    uint8_t* rx_data = NULL;
    double* times = NULL;
    
    int result = FSO_SUCCESS;
    int components_initialized = 0;
    
    // Allocate buffers
    tx_data = (uint8_t*)malloc(FRAME_SIZE_BYTES);
    fec_encoded = (uint8_t*)malloc(FRAME_SIZE_BYTES * 2);
    modulated = (double*)malloc(FRAME_SIZE_BYTES * 16 * sizeof(double));
    channel_output = (double*)malloc(FRAME_SIZE_BYTES * 16 * sizeof(double));
    fec_decoded = (uint8_t*)malloc(FRAME_SIZE_BYTES * 2);
    rx_data = (uint8_t*)malloc(FRAME_SIZE_BYTES);
    times = (double*)malloc(NUM_FRAMES * sizeof(double));
    
    if (!tx_data || !fec_encoded || !modulated || !channel_output ||
        !fec_decoded || !rx_data || !times) {
        result = FSO_ERROR_MEMORY;
        goto cleanup;
    }
    
    // Initialize modulator
    if (modulator_init(&mod, config->modulation, 1e6) != FSO_SUCCESS) {
        FSO_LOG_ERROR("BENCH_E2E", "Failed to initialize modulator");
        result = FSO_ERROR_NOT_INITIALIZED;
        goto cleanup;
    }
    components_initialized |= 0x01;
    
    // Initialize FEC
    if (config->fec_type == FEC_REED_SOLOMON) {
        RSConfig rs_config = {
            .symbol_size = 8,
            .num_roots = 32,
            .first_root = 1,
            .primitive_poly = 0x11d,
            .fcr = 1
        };
        
        if (fec_init(&fec, FEC_REED_SOLOMON, 223, 255, 
                    &rs_config) != FSO_SUCCESS) {
            FSO_LOG_ERROR("BENCH_E2E", "Failed to initialize FEC");
            result = FSO_ERROR_NOT_INITIALIZED;
            goto cleanup;
        }
        components_initialized |= 0x02;
    }
    
    // Initialize signal processor
    if (config->use_signal_processing) {
        if (sp_init(&sp, 0, FRAME_SIZE_BYTES * 16) != FSO_SUCCESS) {
            FSO_LOG_ERROR("BENCH_E2E", "Failed to initialize signal processor");
            result = FSO_ERROR_NOT_INITIALIZED;
            goto cleanup;
        }
        components_initialized |= 0x04;
    }
    
    // Initialize channel model
    if (config->use_channel_model) {
        if (channel_init(&channel, 1000.0, 1550e-9, config->weather) != FSO_SUCCESS) {
            FSO_LOG_ERROR("BENCH_E2E", "Failed to initialize channel");
            result = FSO_ERROR_NOT_INITIALIZED;
            goto cleanup;
        }
        components_initialized |= 0x08;
    }
    
    // Initialize metrics
    benchmark_metrics_init(metrics);
    metrics->data_size_bytes = FRAME_SIZE_BYTES;
    metrics->iterations = NUM_FRAMES;
    
    // Warmup
    for (int i = 0; i < WARMUP_FRAMES; i++) {
        // Generate random data
        for (size_t j = 0; j < FRAME_SIZE_BYTES; j++) {
            tx_data[j] = (uint8_t)fso_random_int(0, 255);
        }
        
        // Transmit path
        size_t encoded_len, symbol_len;
        
        if (config->fec_type == FEC_REED_SOLOMON) {
            fec_encode(&fec, tx_data, FRAME_SIZE_BYTES, 
                      fec_encoded, &encoded_len);
            modulate(&mod, fec_encoded, encoded_len, modulated, &symbol_len);
        } else {
            modulate(&mod, tx_data, FRAME_SIZE_BYTES, modulated, &symbol_len);
        }
        
        // Channel
        if (config->use_channel_model) {
            for (size_t j = 0; j < symbol_len; j++) {
                channel_output[j] = channel_apply_fading(&channel, modulated[j]);
            }
        } else {
            memcpy(channel_output, modulated, symbol_len * sizeof(double));
        }
        
        // Receive path
        size_t decoded_len, rx_len;
        
        if (config->fec_type == FEC_REED_SOLOMON) {
            demodulate(&mod, channel_output, symbol_len,
                      fec_decoded, &decoded_len, config->snr_db);
            FECStats stats;
            fec_decode(&fec, fec_decoded, decoded_len,
                      rx_data, &rx_len, &stats);
        } else {
            demodulate(&mod, channel_output, symbol_len,
                      rx_data, &rx_len, config->snr_db);
        }
    }
    
    // Benchmark
    BenchmarkTimer timer;
    benchmark_timer_init(&timer);
    
    int successful_frames = 0;
    int realtime_frames = 0;
    
    for (int i = 0; i < NUM_FRAMES; i++) {
        // Generate random data
        for (size_t j = 0; j < FRAME_SIZE_BYTES; j++) {
            tx_data[j] = (uint8_t)fso_random_int(0, 255);
        }
        
        // Start timing
        benchmark_timer_start(&timer);
        
        // ===== TRANSMIT PATH =====
        size_t encoded_len, symbol_len;
        
        // FEC encoding
        if (config->fec_type == FEC_REED_SOLOMON) {
            if (fec_encode(&fec, tx_data, FRAME_SIZE_BYTES,
                          fec_encoded, &encoded_len) != FSO_SUCCESS) {
                continue;
            }
            
            // Modulation
            if (modulate(&mod, fec_encoded, encoded_len,
                        modulated, &symbol_len) != FSO_SUCCESS) {
                continue;
            }
        } else {
            // Modulation without FEC
            if (modulate(&mod, tx_data, FRAME_SIZE_BYTES,
                        modulated, &symbol_len) != FSO_SUCCESS) {
                continue;
            }
        }
        
        // ===== CHANNEL =====
        if (config->use_channel_model) {
            for (size_t j = 0; j < symbol_len; j++) {
                channel_output[j] = channel_apply_fading(&channel, modulated[j]);
            }
        } else {
            memcpy(channel_output, modulated, symbol_len * sizeof(double));
        }
        
        // ===== RECEIVE PATH =====
        size_t decoded_len, rx_len;
        
        // Demodulation
        if (demodulate(&mod, channel_output, symbol_len,
                      fec_decoded, &decoded_len, config->snr_db) != FSO_SUCCESS) {
            continue;
        }
        
        // FEC decoding
        if (config->fec_type == FEC_REED_SOLOMON) {
            FECStats stats;
            if (fec_decode(&fec, fec_decoded, decoded_len,
                          rx_data, &rx_len, &stats) != FSO_SUCCESS) {
                continue;
            }
        } else {
            memcpy(rx_data, fec_decoded, decoded_len);
            rx_len = decoded_len;
        }
        
        // Stop timing
        benchmark_timer_stop(&timer);
        
        times[i] = benchmark_timer_elapsed_ms(&timer);
        
        // Check if frame meets real-time requirement
        if (times[i] < REALTIME_THRESHOLD_MS) {
            realtime_frames++;
        }
        
        // Verify data integrity (for frames without channel)
        if (!config->use_channel_model && rx_len == FRAME_SIZE_BYTES) {
            if (memcmp(tx_data, rx_data, FRAME_SIZE_BYTES) == 0) {
                successful_frames++;
            }
        } else {
            successful_frames++;
        }
    }
    
    // Calculate statistics
    benchmark_calculate_statistics(times, NUM_FRAMES,
                                   &metrics->avg_time_ms,
                                   &metrics->stddev_time_ms,
                                   &metrics->min_time_ms,
                                   &metrics->max_time_ms);
    
    metrics->success_count = successful_frames;
    metrics->failure_count = NUM_FRAMES - successful_frames;
    
    // Calculate throughput
    metrics->throughput_mbps = 
        benchmark_calculate_throughput_mbps(FRAME_SIZE_BYTES, metrics->avg_time_ms);
    
    // Store real-time compliance rate
    metrics->parallel_efficiency = (double)realtime_frames / NUM_FRAMES;
    
    // Memory usage
    metrics->peak_memory_bytes = benchmark_get_peak_memory_usage();
    
cleanup:
    // Cleanup components
    if (components_initialized & 0x01) modulator_free(&mod);
    if (components_initialized & 0x02) fec_free(&fec);
    if (components_initialized & 0x04) sp_free(&sp);
    if (components_initialized & 0x08) channel_free(&channel);
    
    // Free buffers
    free(tx_data);
    free(fec_encoded);
    free(modulated);
    free(channel_output);
    free(fec_decoded);
    free(rx_data);
    free(times);
    
    return result;
}

/* ============================================================================
 * Public Benchmark Functions
 * ============================================================================ */

/**
 * @brief Run comprehensive end-to-end latency benchmarks
 */
int benchmark_e2e_comprehensive(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  End-to-End Latency Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    
    printf("Frame size: %d bytes\n", FRAME_SIZE_BYTES);
    printf("Number of frames: %d\n", NUM_FRAMES);
    printf("Real-time threshold: %.1f ms\n", REALTIME_THRESHOLD_MS);
    printf("\n");
    
    // Test configurations
    E2EConfig configs[] = {
        // Minimal configuration
        {
            .modulation = MOD_OOK,
            .fec_type = FEC_REED_SOLOMON,
            .use_signal_processing = 0,
            .use_channel_model = 0,
            .weather = WEATHER_CLEAR,
            .snr_db = 20.0
        },
        // With FEC
        {
            .modulation = MOD_OOK,
            .fec_type = FEC_REED_SOLOMON,
            .use_signal_processing = 0,
            .use_channel_model = 0,
            .weather = WEATHER_CLEAR,
            .snr_db = 20.0
        },
        // With channel model
        {
            .modulation = MOD_OOK,
            .fec_type = FEC_REED_SOLOMON,
            .use_signal_processing = 0,
            .use_channel_model = 1,
            .weather = WEATHER_CLEAR,
            .snr_db = 20.0
        },
        // Full system - clear weather
        {
            .modulation = MOD_OOK,
            .fec_type = FEC_REED_SOLOMON,
            .use_signal_processing = 1,
            .use_channel_model = 1,
            .weather = WEATHER_CLEAR,
            .snr_db = 20.0
        },
        // Full system - foggy weather
        {
            .modulation = MOD_OOK,
            .fec_type = FEC_REED_SOLOMON,
            .use_signal_processing = 1,
            .use_channel_model = 1,
            .weather = WEATHER_FOG,
            .snr_db = 15.0
        },
        // PPM modulation
        {
            .modulation = MOD_PPM,
            .fec_type = FEC_REED_SOLOMON,
            .use_signal_processing = 1,
            .use_channel_model = 1,
            .weather = WEATHER_CLEAR,
            .snr_db = 20.0
        }
    };
    
    const char* config_names[] = {
        "OOK only",
        "OOK + RS FEC",
        "OOK + RS FEC + Channel",
        "Full (Clear)",
        "Full (Fog)",
        "PPM + RS FEC + Channel"
    };
    
    int num_configs = sizeof(configs) / sizeof(configs[0]);
    
    printf("%-25s %12s %12s %12s %12s\n",
           "Configuration", "Avg (ms)", "Min (ms)", "Max (ms)", "RT Frames");
    printf("%-25s %12s %12s %12s %12s\n",
           "-------------------------", "------------", "------------", 
           "------------", "------------");
    
    for (int i = 0; i < num_configs; i++) {
        printf("Running: %s...\n", config_names[i]);
        
        PerformanceMetrics metrics;
        if (run_e2e_benchmark(&configs[i], &metrics) != FSO_SUCCESS) {
            printf("  FAILED\n");
            continue;
        }
        
        int rt_frames = (int)(metrics.parallel_efficiency * NUM_FRAMES);
        double rt_percentage = metrics.parallel_efficiency * 100.0;
        
        printf("%-25s %12.3f %12.3f %12.3f %9d/%d\n",
               config_names[i],
               metrics.avg_time_ms,
               metrics.min_time_ms,
               metrics.max_time_ms,
               rt_frames,
               NUM_FRAMES);
        
        // Check real-time compliance
        if (metrics.avg_time_ms < REALTIME_THRESHOLD_MS) {
            printf("  ✓ Meets real-time requirement (%.1f%% frames < %.1f ms)\n",
                   rt_percentage, REALTIME_THRESHOLD_MS);
        } else {
            printf("  ✗ Does NOT meet real-time requirement (avg %.3f ms > %.1f ms)\n",
                   metrics.avg_time_ms, REALTIME_THRESHOLD_MS);
        }
        
        printf("  Throughput: %.2f Mbps\n", metrics.throughput_mbps);
        printf("\n");
    }
    
    return FSO_SUCCESS;
}

/**
 * @brief Run quick end-to-end latency test
 */
int benchmark_e2e_quick(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("  Quick End-to-End Latency Test\n");
    printf("================================================================================\n");
    printf("\n");
    
    E2EConfig config = {
        .modulation = MOD_OOK,
        .fec_type = FEC_REED_SOLOMON,
        .use_signal_processing = 1,
        .use_channel_model = 1,
        .weather = WEATHER_CLEAR,
        .snr_db = 20.0
    };
    
    PerformanceMetrics metrics;
    if (run_e2e_benchmark(&config, &metrics) != FSO_SUCCESS) {
        printf("Benchmark failed!\n");
        return FSO_ERROR_IO;
    }
    
    printf("Results:\n");
    printf("--------\n");
    printf("  Average latency:   %.3f ms\n", metrics.avg_time_ms);
    printf("  Min latency:       %.3f ms\n", metrics.min_time_ms);
    printf("  Max latency:       %.3f ms\n", metrics.max_time_ms);
    printf("  Std deviation:     %.3f ms\n", metrics.stddev_time_ms);
    printf("  Throughput:        %.2f Mbps\n", metrics.throughput_mbps);
    printf("\n");
    
    int rt_frames = (int)(metrics.parallel_efficiency * NUM_FRAMES);
    printf("  Real-time frames:  %d/%d (%.1f%%)\n",
           rt_frames, NUM_FRAMES, metrics.parallel_efficiency * 100.0);
    
    if (metrics.avg_time_ms < REALTIME_THRESHOLD_MS) {
        printf("\n  ✓ System meets real-time requirement (< %.1f ms)\n",
               REALTIME_THRESHOLD_MS);
    } else {
        printf("\n  ✗ System does NOT meet real-time requirement\n");
        printf("    Average latency %.3f ms exceeds threshold of %.1f ms\n",
               metrics.avg_time_ms, REALTIME_THRESHOLD_MS);
    }
    
    printf("\n");
    return FSO_SUCCESS;
}
