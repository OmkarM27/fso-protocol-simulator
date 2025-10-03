/**
 * @file ook.c
 * @brief On-Off Keying (OOK) modulation implementation
 * 
 * OOK is the simplest form of amplitude-shift keying (ASK) modulation.
 * Binary '1' is represented by light on (symbol value 1.0)
 * Binary '0' is represented by light off (symbol value 0.0)
 */

#include "modulation/modulation.h"
#include <string.h>
#include <math.h>

#define MODULE_NAME "OOK"

/* ============================================================================
 * OOK Modulation
 * ============================================================================ */

int ook_modulate(const uint8_t* data, size_t data_len,
                 double* symbols, size_t* symbol_len) {
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(symbol_len);
    FSO_CHECK_PARAM(data_len > 0);
    
    size_t num_bits = data_len * 8;
    size_t symbol_idx = 0;
    
    // Convert each bit to a symbol
    for (size_t byte_idx = 0; byte_idx < data_len; byte_idx++) {
        uint8_t byte = data[byte_idx];
        
        // Process each bit in the byte (MSB first)
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            int bit = (byte >> bit_idx) & 0x01;
            symbols[symbol_idx++] = bit ? 1.0 : 0.0;
        }
    }
    
    *symbol_len = num_bits;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Modulated %zu bytes to %zu OOK symbols", 
                 data_len, num_bits);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * OOK Demodulation
 * ============================================================================ */

double ook_calculate_threshold(double snr) {
    // For OOK with symbols at 0.0 and 1.0:
    // Optimal threshold depends on SNR and noise characteristics
    // For AWGN channel with equal probability of 0 and 1:
    // threshold = 0.5 is optimal for high SNR
    // For low SNR, adjust threshold based on noise variance
    
    if (snr >= 10.0) {
        // High SNR: use midpoint threshold
        return 0.5;
    } else {
        // Low SNR: adjust threshold slightly higher to reduce false alarms
        // Convert SNR from dB to linear
        double snr_linear = fso_db_to_linear(snr);
        
        // Noise variance (assuming signal power = 0.5 for OOK)
        double signal_power = 0.5;  // Average power with equal 0s and 1s
        double noise_variance = signal_power / snr_linear;
        
        // Adjust threshold based on noise
        // For low SNR, bias slightly toward 0 to reduce false positives
        double threshold = 0.5 + 0.1 * noise_variance;
        
        // Clamp threshold to reasonable range
        if (threshold > 0.7) threshold = 0.7;
        if (threshold < 0.3) threshold = 0.3;
        
        return threshold;
    }
}

int ook_demodulate(const double* symbols, size_t symbol_len,
                   uint8_t* data, size_t* data_len, double snr) {
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(data_len);
    FSO_CHECK_PARAM(symbol_len > 0);
    FSO_CHECK_PARAM(symbol_len % 8 == 0);  // Must be multiple of 8 bits
    
    // Calculate optimal threshold based on SNR
    double threshold = ook_calculate_threshold(snr);
    
    size_t num_bytes = symbol_len / 8;
    size_t symbol_idx = 0;
    
    // Convert symbols back to bytes
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        uint8_t byte = 0;
        
        // Process 8 symbols to form one byte (MSB first)
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            double symbol = symbols[symbol_idx++];
            
            // Threshold detection
            int bit = (symbol >= threshold) ? 1 : 0;
            
            if (bit) {
                byte |= (1 << bit_idx);
            }
        }
        
        data[byte_idx] = byte;
    }
    
    *data_len = num_bytes;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Demodulated %zu OOK symbols to %zu bytes (threshold=%.3f)",
                 symbol_len, num_bytes, threshold);
    
    return FSO_SUCCESS;
}
