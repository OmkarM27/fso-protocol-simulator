/**
 * @file fec.h
 * @brief Forward Error Correction (FEC) module header
 * 
 * This file contains the base structures and interfaces for Forward Error
 * Correction implementations including Reed-Solomon and LDPC codes.
 */

#ifndef FEC_H
#define FEC_H

#include "../fso.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * FEC Base Structures
 * ============================================================================ */

/**
 * @brief FEC codec structure
 * 
 * Base structure for all FEC codec implementations. Contains common
 * parameters and a pointer to codec-specific state.
 */
typedef struct {
    FECType type;           /**< Type of FEC codec (Reed-Solomon, LDPC) */
    int data_length;        /**< Information bits/symbols */
    int code_length;        /**< Total bits/symbols (data + parity) */
    double code_rate;       /**< Code rate (data_length / code_length) */
    void* codec_state;      /**< Codec-specific state and parameters */
    int is_initialized;     /**< Initialization flag */
} FECCodec;

/**
 * @brief FEC decoding statistics
 * 
 * Structure to track decoding performance and error correction statistics.
 */
typedef struct {
    int errors_detected;    /**< Number of errors detected */
    int errors_corrected;   /**< Number of errors corrected */
    int uncorrectable;      /**< Flag indicating uncorrectable errors */
    int iterations;         /**< Number of decoding iterations (for iterative codes) */
    double syndrome_weight; /**< Syndrome weight (for debugging) */
} FECStats;

/* ============================================================================
 * FEC Configuration Structures
 * ============================================================================ */

/**
 * @brief Reed-Solomon configuration parameters
 */
typedef struct {
    int symbol_size;        /**< Symbol size in bits (typically 8) */
    int num_roots;          /**< Number of parity symbols (2*t) */
    int first_root;         /**< First consecutive root (typically 1) */
    int primitive_poly;     /**< Primitive polynomial for GF construction */
    int fcr;                /**< First consecutive root index */
} RSConfig;

/**
 * @brief LDPC configuration parameters
 */
typedef struct {
    int num_variable_nodes; /**< Number of variable nodes (code length) */
    int num_check_nodes;    /**< Number of check nodes */
    int max_iterations;     /**< Maximum decoding iterations */
    double convergence_threshold; /**< Convergence threshold for belief propagation */
    int* parity_check_matrix; /**< Sparse parity check matrix representation */
    int matrix_rows;        /**< Number of rows in parity check matrix */
    int matrix_cols;        /**< Number of columns in parity check matrix */
} LDPCConfig;

/**
 * @brief Interleaver configuration parameters
 */
typedef struct {
    int block_size;         /**< Size of each interleaver block */
    int depth;              /**< Interleaver depth (number of blocks) */
    int* permutation_table; /**< Permutation table for interleaving */
} InterleaverConfig;

/* ============================================================================
 * FEC Function Prototypes
 * ============================================================================ */

/**
 * @brief Initialize FEC codec
 * 
 * Initializes a FEC codec with the specified type and configuration.
 * Allocates necessary memory and sets up codec-specific parameters.
 * 
 * @param codec Pointer to FEC codec structure
 * @param type Type of FEC codec to initialize
 * @param data_length Length of information data
 * @param code_length Total length including parity
 * @param config Codec-specific configuration (RSConfig* or LDPCConfig*)
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode fec_init(FECCodec* codec, FECType type, int data_length, 
                      int code_length, void* config);

/**
 * @brief Free FEC codec resources
 * 
 * Releases all memory and resources associated with the FEC codec.
 * 
 * @param codec Pointer to FEC codec structure
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode fec_free(FECCodec* codec);

/**
 * @brief Encode data using FEC
 * 
 * Encodes input data using the specified FEC codec. The output buffer
 * must be large enough to hold the encoded data (code_length bytes/symbols).
 * 
 * @param codec Pointer to initialized FEC codec
 * @param data Input data to encode
 * @param data_len Length of input data
 * @param encoded Output buffer for encoded data
 * @param encoded_len Pointer to store actual encoded length
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode fec_encode(FECCodec* codec, const uint8_t* data, size_t data_len,
                        uint8_t* encoded, size_t* encoded_len);

/**
 * @brief Decode data using FEC
 * 
 * Decodes received data using the specified FEC codec. Attempts to
 * correct errors and provides statistics about the decoding process.
 * 
 * @param codec Pointer to initialized FEC codec
 * @param received Received data to decode
 * @param received_len Length of received data
 * @param decoded Output buffer for decoded data
 * @param decoded_len Pointer to store actual decoded length
 * @param stats Pointer to store decoding statistics (can be NULL)
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode fec_decode(FECCodec* codec, const uint8_t* received, size_t received_len,
                        uint8_t* decoded, size_t* decoded_len, FECStats* stats);

/**
 * @brief Validate FEC configuration
 * 
 * Validates the configuration parameters for a FEC codec to ensure
 * they are within acceptable ranges and consistent.
 * 
 * @param type FEC codec type
 * @param data_length Information data length
 * @param code_length Total code length
 * @param config Codec-specific configuration
 * @return FSO_SUCCESS if valid, error code if invalid
 */
FSOErrorCode fec_validate_config(FECType type, int data_length, int code_length, 
                                 void* config);

/**
 * @brief Get FEC codec information
 * 
 * Retrieves information about the FEC codec including type, parameters,
 * and current state.
 * 
 * @param codec Pointer to FEC codec
 * @param type Pointer to store codec type (can be NULL)
 * @param data_length Pointer to store data length (can be NULL)
 * @param code_length Pointer to store code length (can be NULL)
 * @param code_rate Pointer to store code rate (can be NULL)
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode fec_get_info(const FECCodec* codec, FECType* type, int* data_length,
                          int* code_length, double* code_rate);

/**
 * @brief Calculate theoretical code rate
 * 
 * Calculates the theoretical code rate for given parameters.
 * 
 * @param data_length Information data length
 * @param code_length Total code length
 * @return Code rate (data_length / code_length)
 */
double fec_calculate_code_rate(int data_length, int code_length);

/**
 * @brief Check if codec is initialized
 * 
 * Checks whether a FEC codec has been properly initialized.
 * 
 * @param codec Pointer to FEC codec
 * @return 1 if initialized, 0 if not initialized
 */
int fec_is_initialized(const FECCodec* codec);

/* ============================================================================
 * Interleaving Functions
 * ============================================================================ */

/**
 * @brief Initialize interleaver
 * 
 * Initializes an interleaver with the specified configuration.
 * 
 * @param config Pointer to interleaver configuration
 * @param block_size Size of each block to interleave
 * @param depth Interleaver depth
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode interleaver_init(InterleaverConfig* config, int block_size, int depth);

/**
 * @brief Free interleaver resources
 * 
 * Releases memory allocated for interleaver configuration.
 * 
 * @param config Pointer to interleaver configuration
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode interleaver_free(InterleaverConfig* config);

/**
 * @brief Interleave data
 * 
 * Applies block interleaving to input data to distribute burst errors.
 * 
 * @param config Pointer to interleaver configuration
 * @param input Input data to interleave
 * @param input_len Length of input data
 * @param output Output buffer for interleaved data
 * @param output_len Length of output buffer
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode interleave(const InterleaverConfig* config, const uint8_t* input,
                        size_t input_len, uint8_t* output, size_t output_len);

/**
 * @brief Deinterleave data
 * 
 * Reverses the interleaving process to restore original data order.
 * 
 * @param config Pointer to interleaver configuration
 * @param input Interleaved input data
 * @param input_len Length of input data
 * @param output Output buffer for deinterleaved data
 * @param output_len Length of output buffer
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode deinterleave(const InterleaverConfig* config, const uint8_t* input,
                          size_t input_len, uint8_t* output, size_t output_len);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get string representation of FEC type
 * 
 * @param type FEC type
 * @return String representation of FEC type
 */
const char* fec_type_string(FECType type);

/**
 * @brief Calculate minimum code length for given parameters
 * 
 * Calculates the minimum code length required for the specified
 * data length and error correction capability.
 * 
 * @param type FEC type
 * @param data_length Information data length
 * @param error_correction_capability Number of errors to correct
 * @return Minimum code length required
 */
int fec_calculate_min_code_length(FECType type, int data_length, 
                                  int error_correction_capability);

#endif /* FEC_H */