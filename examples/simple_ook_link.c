/**
 * @file simple_ook_link.c
 * @brief Simple OOK link simulation example
 * 
 * This example demonstrates a basic FSO link using On-Off Keying modulation
 * with Reed-Solomon error correction over an atmospheric channel.
 * 
 * Compile:
 *   gcc -I../src simple_ook_link.c -L../build -lfso -lm -lfftw3 -fopenmp -o simple_ook_link
 * 
 * Run:
 *   ./simple_ook_link
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fso.h"
#include "modulation/modulation.h"
#include "fec/fec.h"
#include "turbulence/channel.h"

#define DATA_SIZE 100  // 100 bytes of data
#define LINK_DISTANCE 1000.0  // 1 km
#define WAVELENGTH 1.55e-6  // 1550 nm
#define TRANSMIT_POWER 0.1  // 100 mW
#define NOISE_POWER 1e-9  // 1 nW

int main(void) {
    printf("=== Simple OOK Link Simulation ===\n\n");
    
    // Set log level
    fso_set_log_level(LOG_INFO);
    
    // Initialize random number generator
    fso_random_init(0);  // 0 = time-based seed
    
    // ========================================================================
    // Step 1: Generate random data
    // ========================================================================
    uint8_t tx_data[DATA_SIZE];
    printf("Generating %d bytes of random data...\n", DATA_SIZE);
    for (int i = 0; i < DATA_SIZE; i++) {
        tx_data[i] = (uint8_t)(fso_random_int(0, 255));
    }
    
    // ========================================================================
    // Step 2: Initialize FEC encoder (Reed-Solomon)
    // ========================================================================
    printf("Initializing Reed-Solomon FEC (223, 255)...\n");
    FECCodec fec_codec;
    RSConfig rs_config = {
        .symbol_size = 8,
        .num_roots = 32,  // Can correct 16 symbol errors
        .first_root = 1,
        .primitive_poly = 0x11d,  // Standard for GF(256)
        .fcr = 1
    };
    
    if (fec_init(&fec_codec, FEC_REED_SOLOMON, 223, 255, &rs_config) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize FEC codec\n");
        return 1;
    }
    
    // Encode data
    uint8_t encoded_data[512];
    size_t encoded_len;
    printf("Encoding data with FEC...\n");
    if (fec_encode(&fec_codec, tx_data, DATA_SIZE, encoded_data, &encoded_len) != FSO_SUCCESS) {
        fprintf(stderr, "FEC encoding failed\n");
        fec_free(&fec_codec);
        return 1;
    }
    printf("  Encoded length: %zu bytes (code rate: %.3f)\n", 
           encoded_len, (double)DATA_SIZE / encoded_len);
    
    // ========================================================================
    // Step 3: Initialize OOK modulator
    // ========================================================================
    printf("Initializing OOK modulator (1 Msps)...\n");
    Modulator modulator;
    if (modulator_init(&modulator, MOD_OOK, 1e6) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize modulator\n");
        fec_free(&fec_codec);
        return 1;
    }
    
    // Modulate encoded data
    double symbols[8192];
    size_t symbol_len;
    printf("Modulating data...\n");
    if (modulate(&modulator, encoded_data, encoded_len, symbols, &symbol_len) != FSO_SUCCESS) {
        fprintf(stderr, "Modulation failed\n");
        modulator_free(&modulator);
        fec_free(&fec_codec);
        return 1;
    }
    printf("  Symbol count: %zu\n", symbol_len);
    
    // ========================================================================
    // Step 4: Initialize atmospheric channel
    // ========================================================================
    printf("Initializing atmospheric channel...\n");
    printf("  Distance: %.1f m\n", LINK_DISTANCE);
    printf("  Wavelength: %.0f nm\n", WAVELENGTH * 1e9);
    printf("  Weather: Clear\n");
    
    ChannelModel channel;
    if (channel_init(&channel, LINK_DISTANCE, WAVELENGTH, WEATHER_CLEAR) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize channel\n");
        modulator_free(&modulator);
        fec_free(&fec_codec);
        return 1;
    }
    
    // ========================================================================
    // Step 5: Transmit through channel
    // ========================================================================
    printf("Transmitting through atmospheric channel...\n");
    
    // Apply channel effects to each symbol
    double received_symbols[8192];
    double total_signal_power = 0.0;
    double total_noise_power = 0.0;
    
    for (size_t i = 0; i < symbol_len; i++) {
        // Symbol power
        double symbol_power = symbols[i] * symbols[i] * TRANSMIT_POWER;
        
        // Apply channel effects (fading, attenuation, path loss)
        double received_power = channel_apply_effects(&channel, symbol_power, 
                                                       NOISE_POWER, 1e-6);
        
        // Convert back to amplitude
        received_symbols[i] = sqrt(received_power / TRANSMIT_POWER);
        
        // Track power for SNR calculation
        total_signal_power += symbol_power;
        total_noise_power += NOISE_POWER;
    }
    
    double avg_snr_linear = total_signal_power / total_noise_power;
    double avg_snr_db = fso_linear_to_db(avg_snr_linear);
    printf("  Average SNR: %.2f dB\n", avg_snr_db);
    
    // ========================================================================
    // Step 6: Demodulate received signal
    // ========================================================================
    printf("Demodulating received signal...\n");
    uint8_t demod_data[512];
    size_t demod_len;
    
    if (demodulate(&modulator, received_symbols, symbol_len, 
                   demod_data, &demod_len, avg_snr_db) != FSO_SUCCESS) {
        fprintf(stderr, "Demodulation failed\n");
        channel_free(&channel);
        modulator_free(&modulator);
        fec_free(&fec_codec);
        return 1;
    }
    printf("  Demodulated length: %zu bytes\n", demod_len);
    
    // ========================================================================
    // Step 7: FEC decode
    // ========================================================================
    printf("Decoding with FEC...\n");
    uint8_t decoded_data[DATA_SIZE];
    size_t decoded_len;
    FECStats fec_stats;
    
    if (fec_decode(&fec_codec, demod_data, demod_len, 
                   decoded_data, &decoded_len, &fec_stats) != FSO_SUCCESS) {
        fprintf(stderr, "FEC decoding failed\n");
        channel_free(&channel);
        modulator_free(&modulator);
        fec_free(&fec_codec);
        return 1;
    }
    
    printf("  Decoded length: %zu bytes\n", decoded_len);
    printf("  Errors detected: %d\n", fec_stats.errors_detected);
    printf("  Errors corrected: %d\n", fec_stats.errors_corrected);
    printf("  Uncorrectable: %s\n", fec_stats.uncorrectable ? "YES" : "NO");
    
    // ========================================================================
    // Step 8: Compare transmitted and received data
    // ========================================================================
    printf("\nComparing transmitted and received data...\n");
    int bit_errors = 0;
    int byte_errors = 0;
    
    for (size_t i = 0; i < DATA_SIZE && i < decoded_len; i++) {
        if (tx_data[i] != decoded_data[i]) {
            byte_errors++;
            // Count bit errors
            uint8_t diff = tx_data[i] ^ decoded_data[i];
            for (int b = 0; b < 8; b++) {
                if (diff & (1 << b)) {
                    bit_errors++;
                }
            }
        }
    }
    
    double ber = (double)bit_errors / (DATA_SIZE * 8);
    double byte_error_rate = (double)byte_errors / DATA_SIZE;
    
    printf("  Bit errors: %d / %d\n", bit_errors, DATA_SIZE * 8);
    printf("  Bit Error Rate (BER): %.2e\n", ber);
    printf("  Byte errors: %d / %d\n", byte_errors, DATA_SIZE);
    printf("  Byte Error Rate: %.2e\n", byte_error_rate);
    
    // ========================================================================
    // Cleanup
    // ========================================================================
    channel_free(&channel);
    modulator_free(&modulator);
    fec_free(&fec_codec);
    
    // ========================================================================
    // Summary
    // ========================================================================
    printf("\n=== Simulation Complete ===\n");
    if (bit_errors == 0) {
        printf("SUCCESS: All data transmitted correctly!\n");
        return 0;
    } else {
        printf("PARTIAL SUCCESS: %d bit errors detected\n", bit_errors);
        return 0;
    }
}
