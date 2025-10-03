/**
 * @file modulation.h
 * @brief Optical modulation schemes for FSO communication
 * 
 * This module implements various optical modulation schemes including:
 * - On-Off Keying (OOK)
 * - Pulse Position Modulation (PPM)
 * - Differential Phase Shift Keying (DPSK)
 */

#ifndef MODULATION_H
#define MODULATION_H

#include "fso.h"

/* ============================================================================
 * Modulation Structures
 * ============================================================================ */

/**
 * @brief PPM-specific configuration
 */
typedef struct {
    int order;           /**< PPM order (2, 4, 8, 16) */
    int slots_per_symbol; /**< Number of time slots per symbol */
} PPMConfig;

/**
 * @brief DPSK-specific configuration
 */
typedef struct {
    double last_phase;   /**< Last transmitted/received phase */
    int initialized;     /**< Whether phase tracking is initialized */
} DPSKState;

/**
 * @brief Modulator structure
 */
typedef struct {
    ModulationType type;      /**< Modulation type */
    double symbol_rate;       /**< Symbol rate in symbols/second */
    int bits_per_symbol;      /**< Number of bits per symbol */
    
    union {
        PPMConfig ppm;        /**< PPM-specific configuration */
        DPSKState dpsk;       /**< DPSK-specific state */
    } config;
    
    int initialized;          /**< Initialization flag */
} Modulator;

/* ============================================================================
 * Initialization and Cleanup Functions
 * ============================================================================ */

/**
 * @brief Initialize a modulator
 * @param mod Pointer to modulator structure
 * @param type Modulation type
 * @param symbol_rate Symbol rate in symbols/second
 * @return FSO_SUCCESS on success, error code otherwise
 */
int modulator_init(Modulator* mod, ModulationType type, double symbol_rate);

/**
 * @brief Initialize a PPM modulator with specific order
 * @param mod Pointer to modulator structure
 * @param symbol_rate Symbol rate in symbols/second
 * @param ppm_order PPM order (2, 4, 8, or 16)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int modulator_init_ppm(Modulator* mod, double symbol_rate, int ppm_order);

/**
 * @brief Free resources associated with a modulator
 * @param mod Pointer to modulator structure
 */
void modulator_free(Modulator* mod);

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

/**
 * @brief Modulate data to symbols
 * @param mod Pointer to initialized modulator
 * @param data Input data bytes
 * @param data_len Length of input data in bytes
 * @param symbols Output symbol array (must be pre-allocated)
 * @param symbol_len Pointer to store number of output symbols
 * @return FSO_SUCCESS on success, error code otherwise
 */
int modulate(Modulator* mod, const uint8_t* data, size_t data_len,
             double* symbols, size_t* symbol_len);

/**
 * @brief Demodulate symbols to data
 * @param mod Pointer to initialized modulator
 * @param symbols Input symbol array
 * @param symbol_len Number of input symbols
 * @param data Output data bytes (must be pre-allocated)
 * @param data_len Pointer to store length of output data in bytes
 * @param snr Signal-to-noise ratio in dB (used for threshold calculation)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int demodulate(Modulator* mod, const double* symbols, size_t symbol_len,
               uint8_t* data, size_t* data_len, double snr);

/* ============================================================================
 * OOK-Specific Functions
 * ============================================================================ */

/**
 * @brief Modulate data using On-Off Keying
 * @param data Input data bytes
 * @param data_len Length of input data in bytes
 * @param symbols Output symbol array (must be pre-allocated, size >= data_len * 8)
 * @param symbol_len Pointer to store number of output symbols
 * @return FSO_SUCCESS on success, error code otherwise
 */
int ook_modulate(const uint8_t* data, size_t data_len,
                 double* symbols, size_t* symbol_len);

/**
 * @brief Demodulate OOK symbols to data
 * @param symbols Input symbol array
 * @param symbol_len Number of input symbols
 * @param data Output data bytes (must be pre-allocated)
 * @param data_len Pointer to store length of output data in bytes
 * @param snr Signal-to-noise ratio in dB
 * @return FSO_SUCCESS on success, error code otherwise
 */
int ook_demodulate(const double* symbols, size_t symbol_len,
                   uint8_t* data, size_t* data_len, double snr);

/**
 * @brief Calculate optimal OOK detection threshold based on SNR
 * @param snr Signal-to-noise ratio in dB
 * @return Optimal threshold value
 */
double ook_calculate_threshold(double snr);

/* ============================================================================
 * PPM-Specific Functions
 * ============================================================================ */

/**
 * @brief Modulate data using Pulse Position Modulation
 * @param data Input data bytes
 * @param data_len Length of input data in bytes
 * @param symbols Output symbol array (must be pre-allocated)
 * @param symbol_len Pointer to store number of output symbols
 * @param ppm_order PPM order (2, 4, 8, or 16)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int ppm_modulate(const uint8_t* data, size_t data_len,
                 double* symbols, size_t* symbol_len, int ppm_order);

/**
 * @brief Demodulate PPM symbols to data
 * @param symbols Input symbol array
 * @param symbol_len Number of input symbols
 * @param data Output data bytes (must be pre-allocated)
 * @param data_len Pointer to store length of output data in bytes
 * @param ppm_order PPM order (2, 4, 8, or 16)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int ppm_demodulate(const double* symbols, size_t symbol_len,
                   uint8_t* data, size_t* data_len, int ppm_order);

/* ============================================================================
 * DPSK-Specific Functions
 * ============================================================================ */

/**
 * @brief Modulate data using Differential Phase Shift Keying
 * @param data Input data bytes
 * @param data_len Length of input data in bytes
 * @param symbols Output complex symbol array (must be pre-allocated)
 * @param symbol_len Pointer to store number of output symbols
 * @param state DPSK state for phase tracking
 * @return FSO_SUCCESS on success, error code otherwise
 */
int dpsk_modulate(const uint8_t* data, size_t data_len,
                  ComplexSample* symbols, size_t* symbol_len,
                  DPSKState* state);

/**
 * @brief Demodulate DPSK symbols to data
 * @param symbols Input complex symbol array
 * @param symbol_len Number of input symbols
 * @param data Output data bytes (must be pre-allocated)
 * @param data_len Pointer to store length of output data in bytes
 * @param state DPSK state for phase tracking
 * @return FSO_SUCCESS on success, error code otherwise
 */
int dpsk_demodulate(const ComplexSample* symbols, size_t symbol_len,
                    uint8_t* data, size_t* data_len,
                    DPSKState* state);

#endif /* MODULATION_H */
