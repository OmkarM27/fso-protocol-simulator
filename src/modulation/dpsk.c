/**
 * @file dpsk.c
 * @brief Differential Phase Shift Keying (DPSK) implementation
 * 
 * DPSK encodes information in the phase difference between consecutive symbols
 * rather than absolute phase. This makes it more robust to phase noise and
 * eliminates the need for carrier phase recovery.
 * 
 * For binary DPSK (DBPSK):
 * - Bit 0: No phase change (Δφ = 0)
 * - Bit 1: Phase change of π (Δφ = π)
 * 
 * The current symbol phase is: φ_n = φ_(n-1) + Δφ_n
 * 
 * Demodulation uses differential detection:
 * - Multiply current symbol by conjugate of previous symbol
 * - Check the real part: positive -> bit 0, negative -> bit 1
 */

#include "modulation/modulation.h"
#include <string.h>
#include <math.h>

#define MODULE_NAME "DPSK"

/* ============================================================================
 * DPSK Modulation
 * ============================================================================ */

int dpsk_modulate(const uint8_t* data, size_t data_len,
                  ComplexSample* symbols, size_t* symbol_len,
                  DPSKState* state) {
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(symbol_len);
    FSO_CHECK_NULL(state);
    FSO_CHECK_PARAM(data_len > 0);
    
    size_t num_bits = data_len * 8;
    double current_phase;
    
    // Initialize phase if this is the first call
    if (!state->initialized) {
        state->last_phase = 0.0;
        state->initialized = 1;
        current_phase = 0.0;
        FSO_LOG_DEBUG(MODULE_NAME, "Initialized DPSK state with phase 0.0");
    } else {
        current_phase = state->last_phase;
    }
    
    size_t symbol_idx = 0;
    
    // Process each bit
    for (size_t byte_idx = 0; byte_idx < data_len; byte_idx++) {
        uint8_t byte = data[byte_idx];
        
        // Process each bit in the byte (MSB first)
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            int bit = (byte >> bit_idx) & 0x01;
            
            // Calculate phase change based on bit
            double phase_change = bit ? FSO_PI : 0.0;
            
            // Update current phase
            current_phase += phase_change;
            
            // Wrap phase to [-π, π]
            while (current_phase > FSO_PI) {
                current_phase -= 2.0 * FSO_PI;
            }
            while (current_phase < -FSO_PI) {
                current_phase += 2.0 * FSO_PI;
            }
            
            // Generate complex symbol with unit magnitude
            symbols[symbol_idx] = fso_complex_from_polar(1.0, current_phase);
            symbol_idx++;
        }
    }
    
    // Save last phase for next call
    state->last_phase = current_phase;
    
    *symbol_len = num_bits;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Modulated %zu bytes to %zu DPSK symbols (final phase=%.3f rad)",
                 data_len, num_bits, current_phase);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * DPSK Demodulation
 * ============================================================================ */

int dpsk_demodulate(const ComplexSample* symbols, size_t symbol_len,
                    uint8_t* data, size_t* data_len,
                    DPSKState* state) {
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(data_len);
    FSO_CHECK_NULL(state);
    FSO_CHECK_PARAM(symbol_len > 0);
    FSO_CHECK_PARAM(symbol_len % 8 == 0);  // Must be multiple of 8 bits
    
    size_t num_bytes = symbol_len / 8;
    ComplexSample prev_symbol;
    
    // Initialize previous symbol
    if (!state->initialized) {
        // First symbol: assume reference phase of 0
        prev_symbol = fso_complex_from_polar(1.0, 0.0);
        state->initialized = 1;
        FSO_LOG_DEBUG(MODULE_NAME, "Initialized DPSK demodulation state");
    } else {
        // Use last phase from state
        prev_symbol = fso_complex_from_polar(1.0, state->last_phase);
    }
    
    size_t symbol_idx = 0;
    
    // Process symbols to recover bits
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        uint8_t byte = 0;
        
        // Process 8 symbols to form one byte (MSB first)
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            ComplexSample current_symbol = symbols[symbol_idx];
            
            // Differential detection: multiply by conjugate of previous symbol
            // This gives us the phase difference
            ComplexSample diff = fso_complex_mul(current_symbol, 
                                                 fso_complex_conjugate(prev_symbol));
            
            // Decision based on real part:
            // If real part is positive: phase change ≈ 0 -> bit = 0
            // If real part is negative: phase change ≈ π -> bit = 1
            int bit = (diff.real < 0.0) ? 1 : 0;
            
            if (bit) {
                byte |= (1 << bit_idx);
            }
            
            // Update previous symbol for next iteration
            prev_symbol = current_symbol;
            symbol_idx++;
        }
        
        data[byte_idx] = byte;
    }
    
    // Save last symbol phase for next call
    state->last_phase = fso_complex_phase(prev_symbol);
    
    *data_len = num_bytes;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Demodulated %zu DPSK symbols to %zu bytes (final phase=%.3f rad)",
                 symbol_len, num_bytes, state->last_phase);
    
    return FSO_SUCCESS;
}
