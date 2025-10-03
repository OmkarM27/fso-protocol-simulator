/**
 * @file ppm.c
 * @brief Pulse Position Modulation (PPM) implementation
 * 
 * PPM is an M-ary modulation scheme where the position of a pulse
 * within a symbol period encodes log2(M) bits of information.
 * 
 * For M-PPM:
 * - Each symbol consists of M time slots
 * - Exactly one slot contains a pulse (value 1.0)
 * - All other slots are empty (value 0.0)
 * - log2(M) bits are encoded per symbol
 * 
 * Example for 4-PPM (M=4, 2 bits per symbol):
 * - Bits 00 -> pulse in slot 0: [1.0, 0.0, 0.0, 0.0]
 * - Bits 01 -> pulse in slot 1: [0.0, 1.0, 0.0, 0.0]
 * - Bits 10 -> pulse in slot 2: [0.0, 0.0, 1.0, 0.0]
 * - Bits 11 -> pulse in slot 3: [0.0, 0.0, 0.0, 1.0]
 */

#include "modulation/modulation.h"
#include <string.h>
#include <math.h>

#define MODULE_NAME "PPM"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate bits per symbol for given PPM order
 */
static int ppm_bits_per_symbol(int ppm_order) {
    switch (ppm_order) {
        case 2:  return 1;
        case 4:  return 2;
        case 8:  return 3;
        case 16: return 4;
        default: return -1;
    }
}

/**
 * @brief Extract bits from data buffer
 * @param data Data buffer
 * @param bit_offset Bit offset from start of buffer
 * @param num_bits Number of bits to extract (1-8)
 * @return Extracted bits as integer
 */
static uint8_t extract_bits(const uint8_t* data, size_t bit_offset, int num_bits) {
    size_t byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    
    uint8_t result = 0;
    int bits_extracted = 0;
    
    while (bits_extracted < num_bits) {
        int bits_available = 8 - bit_in_byte;
        int bits_to_extract = FSO_MIN(num_bits - bits_extracted, bits_available);
        
        // Extract bits from current byte
        uint8_t mask = ((1 << bits_to_extract) - 1);
        uint8_t bits = (data[byte_idx] >> (bits_available - bits_to_extract)) & mask;
        
        // Add to result
        result = (result << bits_to_extract) | bits;
        
        bits_extracted += bits_to_extract;
        byte_idx++;
        bit_in_byte = 0;
    }
    
    return result;
}

/**
 * @brief Insert bits into data buffer
 * @param data Data buffer
 * @param bit_offset Bit offset from start of buffer
 * @param bits Bits to insert
 * @param num_bits Number of bits to insert (1-8)
 */
static void insert_bits(uint8_t* data, size_t bit_offset, uint8_t bits, int num_bits) {
    size_t byte_idx = bit_offset / 8;
    int bit_in_byte = bit_offset % 8;
    
    int bits_inserted = 0;
    
    while (bits_inserted < num_bits) {
        int bits_available = 8 - bit_in_byte;
        int bits_to_insert = FSO_MIN(num_bits - bits_inserted, bits_available);
        
        // Extract bits to insert
        uint8_t mask = ((1 << bits_to_insert) - 1);
        uint8_t bits_value = (bits >> (num_bits - bits_inserted - bits_to_insert)) & mask;
        
        // Clear target bits
        uint8_t clear_mask = ~(mask << (bits_available - bits_to_insert));
        data[byte_idx] &= clear_mask;
        
        // Insert bits
        data[byte_idx] |= (bits_value << (bits_available - bits_to_insert));
        
        bits_inserted += bits_to_insert;
        byte_idx++;
        bit_in_byte = 0;
    }
}

/* ============================================================================
 * PPM Modulation
 * ============================================================================ */

int ppm_modulate(const uint8_t* data, size_t data_len,
                 double* symbols, size_t* symbol_len, int ppm_order) {
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(symbol_len);
    FSO_CHECK_PARAM(data_len > 0);
    FSO_CHECK_PARAM(ppm_order == 2 || ppm_order == 4 || 
                    ppm_order == 8 || ppm_order == 16);
    
    int bits_per_sym = ppm_bits_per_symbol(ppm_order);
    size_t total_bits = data_len * 8;
    size_t num_symbols = (total_bits + bits_per_sym - 1) / bits_per_sym;  // Ceiling division
    size_t total_slots = num_symbols * ppm_order;
    
    // Initialize all slots to 0.0
    memset(symbols, 0, total_slots * sizeof(double));
    
    size_t bit_offset = 0;
    size_t slot_offset = 0;
    
    for (size_t sym_idx = 0; sym_idx < num_symbols; sym_idx++) {
        // Extract bits for this symbol
        uint8_t bits = 0;
        if (bit_offset + bits_per_sym <= total_bits) {
            bits = extract_bits(data, bit_offset, bits_per_sym);
        } else {
            // Last symbol may have fewer bits - pad with zeros
            int remaining_bits = total_bits - bit_offset;
            if (remaining_bits > 0) {
                bits = extract_bits(data, bit_offset, remaining_bits);
                bits <<= (bits_per_sym - remaining_bits);  // Pad with zeros
            }
        }
        
        // Place pulse in the slot corresponding to the bit pattern
        int pulse_slot = bits;
        symbols[slot_offset + pulse_slot] = 1.0;
        
        bit_offset += bits_per_sym;
        slot_offset += ppm_order;
    }
    
    *symbol_len = total_slots;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Modulated %zu bytes to %zu %d-PPM symbols (%zu slots)",
                 data_len, num_symbols, ppm_order, total_slots);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * PPM Demodulation
 * ============================================================================ */

int ppm_demodulate(const double* symbols, size_t symbol_len,
                   uint8_t* data, size_t* data_len, int ppm_order) {
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(data_len);
    FSO_CHECK_PARAM(symbol_len > 0);
    FSO_CHECK_PARAM(symbol_len % ppm_order == 0);  // Must be multiple of order
    FSO_CHECK_PARAM(ppm_order == 2 || ppm_order == 4 || 
                    ppm_order == 8 || ppm_order == 16);
    
    int bits_per_sym = ppm_bits_per_symbol(ppm_order);
    size_t num_symbols = symbol_len / ppm_order;
    size_t total_bits = num_symbols * bits_per_sym;
    size_t num_bytes = (total_bits + 7) / 8;  // Ceiling division
    
    // Initialize output buffer
    memset(data, 0, num_bytes);
    
    size_t bit_offset = 0;
    size_t slot_offset = 0;
    
    for (size_t sym_idx = 0; sym_idx < num_symbols; sym_idx++) {
        // Maximum likelihood detection: find slot with highest energy
        int max_slot = 0;
        double max_value = symbols[slot_offset];
        
        for (int slot = 1; slot < ppm_order; slot++) {
            if (symbols[slot_offset + slot] > max_value) {
                max_value = symbols[slot_offset + slot];
                max_slot = slot;
            }
        }
        
        // The slot index directly gives us the bit pattern
        uint8_t bits = (uint8_t)max_slot;
        
        // Insert bits into output buffer
        if (bit_offset + bits_per_sym <= total_bits) {
            insert_bits(data, bit_offset, bits, bits_per_sym);
        } else {
            // Last symbol - only insert remaining bits
            int remaining_bits = total_bits - bit_offset;
            if (remaining_bits > 0) {
                bits >>= (bits_per_sym - remaining_bits);
                insert_bits(data, bit_offset, bits, remaining_bits);
            }
        }
        
        bit_offset += bits_per_sym;
        slot_offset += ppm_order;
    }
    
    *data_len = num_bytes;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Demodulated %zu %d-PPM symbols (%zu slots) to %zu bytes",
                 num_symbols, ppm_order, symbol_len, num_bytes);
    
    return FSO_SUCCESS;
}
