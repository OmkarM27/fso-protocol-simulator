/**
 * @file ppm_comparison.c
 * @brief PPM performance comparison example
 * 
 * This example compares the performance of different PPM orders (2-PPM, 4-PPM,
 * 8-PPM, 16-PPM) under various SNR conditions.
 * 
 * Compile:
 *   gcc -I../src ppm_comparison.c -L../build -lfso -lm -lfftw3 -fopenmp -o ppm_comparison
 * 
 * Run:
 *   ./ppm_comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fso.h"
#include "modulation/modulation.h"

#define DATA_SIZE 1000  // 1000 bytes
#define NUM_SNR_POINTS 10
#define MIN_SNR_DB 0.0
#define MAX_SNR_DB 20.0

// Function to simulate transmission at given SNR
double simulate_ppm_ber(int ppm_order, double snr_db, const uint8_t* data, size_t data_len) {
    Modulator modulator;
    
    // Initialize PPM modulator
    if (modulator_init_ppm(&modulator, 1e6, ppm_order) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize %d-PPM modulator\n", ppm_order);
        return -1.0;
    }
    
    // Modulate data
    double symbols[16384];
    size_t symbol_len;
    if (modulate(&modulator, data, data_len, symbols, &symbol_len) != FSO_SUCCESS) {
        fprintf(stderr, "Modulation failed\n");
        modulator_free(&modulator);
        return -1.0;
    }
    
    // Add AWGN noise
    double snr_linear = fso_db_to_linear(snr_db);
    double noise_stddev = 1.0 / sqrt(2.0 * snr_linear);
    
    double noisy_symbols[16384];
    for (size_t i = 0; i < symbol_len; i++) {
        double noise = fso_random_gaussian(0.0, noise_stddev);
        noisy_symbols[i] = symbols[i] + noise;
        // Ensure non-negative (optical intensity)
        if (noisy_symbols[i] < 0.0) {
            noisy_symbols[i] = 0.0;
        }
    }
    
    // Demodulate
    uint8_t demod_data[DATA_SIZE];
    size_t demod_len;
    if (demodulate(&modulator, noisy_symbols, symbol_len, 
                   demod_data, &demod_len, snr_db) != FSO_SUCCESS) {
        fprintf(stderr, "Demodulation failed\n");
        modulator_free(&modulator);
        return -1.0;
    }
    
    // Count bit errors
    int bit_errors = 0;
    size_t compare_len = (data_len < demod_len) ? data_len : demod_len;
    
    for (size_t i = 0; i < compare_len; i++) {
        uint8_t diff = data[i] ^ demod_data[i];
        for (int b = 0; b < 8; b++) {
            if (diff & (1 << b)) {
                bit_errors++;
            }
        }
    }
    
    double ber = (double)bit_errors / (data_len * 8);
    
    modulator_free(&modulator);
    return ber;
}

int main(void) {
    printf("=== PPM Performance Comparison ===\n\n");
    
    // Set log level to reduce output
    fso_set_log_level(LOG_WARNING);
    
    // Initialize random number generator
    fso_random_init(42);  // Fixed seed for reproducibility
    
    // Generate random test data
    uint8_t test_data[DATA_SIZE];
    for (int i = 0; i < DATA_SIZE; i++) {
        test_data[i] = (uint8_t)(fso_random_int(0, 255));
    }
    
    // PPM orders to test
    int ppm_orders[] = {2, 4, 8, 16};
    int num_orders = sizeof(ppm_orders) / sizeof(ppm_orders[0]);
    
    // SNR points to test
    double snr_points[NUM_SNR_POINTS];
    for (int i = 0; i < NUM_SNR_POINTS; i++) {
        snr_points[i] = MIN_SNR_DB + (MAX_SNR_DB - MIN_SNR_DB) * i / (NUM_SNR_POINTS - 1);
    }
    
    // Results storage
    double ber_results[4][NUM_SNR_POINTS];
    
    // Run simulations
    printf("Running simulations...\n");
    printf("Data size: %d bytes\n", DATA_SIZE);
    printf("SNR range: %.1f to %.1f dB\n\n", MIN_SNR_DB, MAX_SNR_DB);
    
    for (int order_idx = 0; order_idx < num_orders; order_idx++) {
        int order = ppm_orders[order_idx];
        printf("Testing %d-PPM:\n", order);
        
        for (int snr_idx = 0; snr_idx < NUM_SNR_POINTS; snr_idx++) {
            double snr_db = snr_points[snr_idx];
            double ber = simulate_ppm_ber(order, snr_db, test_data, DATA_SIZE);
            ber_results[order_idx][snr_idx] = ber;
            
            printf("  SNR = %5.1f dB: BER = %.2e\n", snr_db, ber);
        }
        printf("\n");
    }
    
    // Print summary table
    printf("=== Summary Table ===\n\n");
    printf("SNR (dB) |");
    for (int i = 0; i < num_orders; i++) {
        printf(" %d-PPM    |", ppm_orders[i]);
    }
    printf("\n");
    printf("---------|");
    for (int i = 0; i < num_orders; i++) {
        printf("----------|");
    }
    printf("\n");
    
    for (int snr_idx = 0; snr_idx < NUM_SNR_POINTS; snr_idx++) {
        printf("  %5.1f  |", snr_points[snr_idx]);
        for (int order_idx = 0; order_idx < num_orders; order_idx++) {
            printf(" %.2e |", ber_results[order_idx][snr_idx]);
        }
        printf("\n");
    }
    
    // Analysis
    printf("\n=== Analysis ===\n\n");
    
    // Find SNR required for BER < 1e-3 for each order
    printf("SNR required for BER < 1e-3:\n");
    for (int order_idx = 0; order_idx < num_orders; order_idx++) {
        int order = ppm_orders[order_idx];
        double required_snr = -1.0;
        
        for (int snr_idx = 0; snr_idx < NUM_SNR_POINTS; snr_idx++) {
            if (ber_results[order_idx][snr_idx] < 1e-3) {
                required_snr = snr_points[snr_idx];
                break;
            }
        }
        
        if (required_snr >= 0) {
            printf("  %d-PPM: %.1f dB\n", order, required_snr);
        } else {
            printf("  %d-PPM: > %.1f dB\n", order, MAX_SNR_DB);
        }
    }
    
    // Bandwidth efficiency
    printf("\nBandwidth efficiency (bits/symbol):\n");
    for (int order_idx = 0; order_idx < num_orders; order_idx++) {
        int order = ppm_orders[order_idx];
        double bits_per_symbol = log2(order);
        printf("  %d-PPM: %.1f bits/symbol\n", order, bits_per_symbol);
    }
    
    // Power efficiency (qualitative)
    printf("\nPower efficiency (qualitative):\n");
    printf("  Higher-order PPM (16-PPM) is more power-efficient\n");
    printf("  but requires more bandwidth and synchronization.\n");
    printf("  Lower-order PPM (2-PPM) is simpler but less efficient.\n");
    
    // Recommendations
    printf("\n=== Recommendations ===\n\n");
    printf("Choose PPM order based on your requirements:\n\n");
    printf("2-PPM:\n");
    printf("  + Simple implementation\n");
    printf("  + Easy synchronization\n");
    printf("  - Lower spectral efficiency (1 bit/symbol)\n");
    printf("  Best for: Simple systems, high data rate not critical\n\n");
    
    printf("4-PPM:\n");
    printf("  + Good balance of complexity and efficiency\n");
    printf("  + Moderate bandwidth (2 bits/symbol)\n");
    printf("  Best for: General-purpose FSO links\n\n");
    
    printf("8-PPM:\n");
    printf("  + Better power efficiency\n");
    printf("  + Higher spectral efficiency (3 bits/symbol)\n");
    printf("  - More complex synchronization\n");
    printf("  Best for: Power-constrained systems\n\n");
    
    printf("16-PPM:\n");
    printf("  + Best power efficiency\n");
    printf("  + Highest spectral efficiency (4 bits/symbol)\n");
    printf("  - Most complex implementation\n");
    printf("  - Requires precise timing\n");
    printf("  Best for: Deep space, very low power applications\n\n");
    
    printf("=== Simulation Complete ===\n");
    
    return 0;
}
