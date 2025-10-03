/**
 * @file ldpc.h
 * @brief Low-Density Parity-Check (LDPC) error correction code implementation
 * 
 * This file contains the LDPC encoder and decoder implementation
 * using sparse parity-check matrices and belief propagation decoding.
 */

#ifndef LDPC_H
#define LDPC_H

#include "fec.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * LDPC Constants
 * ============================================================================ */

#define LDPC_MAX_CODE_LENGTH 8192        /**< Maximum code length */
#define LDPC_MAX_ITERATIONS 100          /**< Maximum decoding iterations */
#define LDPC_DEFAULT_CONVERGENCE_THRESHOLD 1e-6  /**< Default convergence threshold */

/* Standard LDPC code rates */
#define LDPC_RATE_1_2 0.5               /**< Code rate 1/2 */
#define LDPC_RATE_2_3 0.6666666666666666 /**< Code rate 2/3 */
#define LDPC_RATE_3_4 0.75              /**< Code rate 3/4 */
#define LDPC_RATE_5_6 0.8333333333333334 /**< Code rate 5/6 */

/* ============================================================================
 * LDPC Structures
 * ============================================================================ */

/**
 * @brief Sparse matrix element
 * 
 * Represents a non-zero element in the sparse parity-check matrix.
 */
typedef struct {
    int row;        /**< Row index */
    int col;        /**< Column index */
    int value;      /**< Value (typically 1 for binary LDPC) */
} SparseElement;

/**
 * @brief Sparse matrix representation
 * 
 * Compressed sparse row (CSR) format for efficient storage and access.
 */
typedef struct {
    int rows;                   /**< Number of rows */
    int cols;                   /**< Number of columns */
    int nnz;                    /**< Number of non-zero elements */
    SparseElement* elements;    /**< Array of non-zero elements */
    int* row_ptr;               /**< Row pointer array (CSR format) */
    int* col_indices;           /**< Column indices array (CSR format) */
    int* values;                /**< Values array (CSR format) */
} SparseMatrix;

/**
 * @brief LDPC codec state
 * 
 * Contains all state information for LDPC encoding and decoding.
 */
typedef struct {
    /* Code parameters */
    int n;                      /**< Code length (total bits) */
    int k;                      /**< Information bits */
    int m;                      /**< Parity bits (n - k) */
    double code_rate;           /**< Code rate (k/n) */
    
    /* Parity-check matrix */
    SparseMatrix* H;            /**< Parity-check matrix */
    
    /* Generator matrix (for systematic encoding) */
    SparseMatrix* G;            /**< Generator matrix */
    
    /* Decoding parameters */
    int max_iterations;         /**< Maximum decoding iterations */
    double convergence_threshold; /**< Convergence threshold */
    
    /* Workspace for decoding */
    double* variable_to_check;  /**< Variable-to-check messages */
    double* check_to_variable;  /**< Check-to-variable messages */
    double* posterior_llr;      /**< Posterior log-likelihood ratios */
    double* channel_llr;        /**< Channel log-likelihood ratios */
    int* decoded_bits;          /**< Decoded bit estimates */
    int* syndrome;              /**< Syndrome vector */
    
    /* Message passing graph connectivity */
    int* var_degree;            /**< Degree of each variable node */
    int* check_degree;          /**< Degree of each check node */
    int** var_to_check_edges;   /**< Variable-to-check edge lists */
    int** check_to_var_edges;   /**< Check-to-variable edge lists */
} LDPCCodec;

/* ============================================================================
 * LDPC Functions
 * ============================================================================ */

/**
 * @brief Initialize LDPC codec
 * 
 * @param ldpc Pointer to LDPC codec structure
 * @param config LDPC configuration parameters
 * @param n Code length
 * @param k Information length
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_init(LDPCCodec* ldpc, const LDPCConfig* config, int n, int k);

/**
 * @brief Free LDPC codec resources
 * 
 * @param ldpc Pointer to LDPC codec structure
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_free(LDPCCodec* ldpc);

/**
 * @brief Generate standard LDPC parity-check matrix
 * 
 * Creates a standard LDPC parity-check matrix for the specified code rate.
 * Uses structured construction for common rates (1/2, 2/3, 3/4, 5/6).
 * 
 * @param ldpc Pointer to LDPC codec
 * @param code_rate Desired code rate
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_generate_standard_matrix(LDPCCodec* ldpc, double code_rate);

/**
 * @brief Generate generator matrix from parity-check matrix
 * 
 * Computes the systematic generator matrix G from the parity-check matrix H
 * using Gaussian elimination to put H in systematic form [P | I].
 * 
 * @param ldpc Pointer to LDPC codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_generate_generator_matrix(LDPCCodec* ldpc);

/**
 * @brief Encode data using LDPC
 * 
 * Performs systematic LDPC encoding using the generator matrix.
 * The encoded output contains the information bits followed by parity bits.
 * 
 * @param ldpc Pointer to LDPC codec
 * @param data Input data bits
 * @param data_len Length of input data
 * @param encoded Output encoded bits
 * @param encoded_len Length of output buffer
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_encode(LDPCCodec* ldpc, const uint8_t* data, size_t data_len,
                         uint8_t* encoded, size_t* encoded_len);

/**
 * @brief Decode received codeword using belief propagation
 * 
 * Performs LDPC decoding using the sum-product algorithm (belief propagation).
 * Iteratively exchanges messages between variable and check nodes until
 * convergence or maximum iterations reached.
 * 
 * @param ldpc Pointer to LDPC codec
 * @param received Received codeword (may contain errors)
 * @param received_len Length of received codeword
 * @param decoded Output decoded data
 * @param decoded_len Length of output buffer
 * @param errors_corrected Pointer to store number of errors corrected (can be NULL)
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_decode(LDPCCodec* ldpc, const uint8_t* received, size_t received_len,
                         uint8_t* decoded, size_t decoded_len, int* errors_corrected);

/* ============================================================================
 * Sparse Matrix Functions
 * ============================================================================ */

/**
 * @brief Initialize sparse matrix
 * 
 * @param matrix Pointer to sparse matrix structure
 * @param rows Number of rows
 * @param cols Number of columns
 * @param nnz Number of non-zero elements
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode sparse_matrix_init(SparseMatrix* matrix, int rows, int cols, int nnz);

/**
 * @brief Free sparse matrix resources
 * 
 * @param matrix Pointer to sparse matrix structure
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode sparse_matrix_free(SparseMatrix* matrix);

/**
 * @brief Set element in sparse matrix
 * 
 * @param matrix Pointer to sparse matrix
 * @param row Row index
 * @param col Column index
 * @param value Value to set
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode sparse_matrix_set(SparseMatrix* matrix, int row, int col, int value);

/**
 * @brief Get element from sparse matrix
 * 
 * @param matrix Pointer to sparse matrix
 * @param row Row index
 * @param col Column index
 * @return Element value (0 if not found)
 */
int sparse_matrix_get(const SparseMatrix* matrix, int row, int col);

/**
 * @brief Convert sparse matrix to CSR format
 * 
 * @param matrix Pointer to sparse matrix
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode sparse_matrix_to_csr(SparseMatrix* matrix);

/**
 * @brief Multiply sparse matrix by vector
 * 
 * Computes y = A * x where A is sparse matrix and x, y are vectors.
 * 
 * @param matrix Pointer to sparse matrix A
 * @param x Input vector
 * @param y Output vector
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode sparse_matrix_vector_multiply(const SparseMatrix* matrix, 
                                           const double* x, double* y);

/* ============================================================================
 * Belief Propagation Functions
 * ============================================================================ */

/**
 * @brief Initialize message passing graph
 * 
 * Sets up the connectivity information for variable and check nodes
 * based on the parity-check matrix structure.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_init_message_graph(LDPCCodec* ldpc);

/**
 * @brief Update variable-to-check messages
 * 
 * Computes messages sent from variable nodes to check nodes
 * in the belief propagation algorithm.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_update_variable_messages(LDPCCodec* ldpc);

/**
 * @brief Update check-to-variable messages
 * 
 * Computes messages sent from check nodes to variable nodes
 * in the belief propagation algorithm.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_update_check_messages(LDPCCodec* ldpc);

/**
 * @brief Update posterior probabilities
 * 
 * Computes the posterior log-likelihood ratios for each bit
 * based on channel information and check node messages.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_update_posteriors(LDPCCodec* ldpc);

/**
 * @brief Check convergence of belief propagation
 * 
 * Determines if the belief propagation algorithm has converged
 * by checking syndrome or message stability.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return 1 if converged, 0 if not converged
 */
int ldpc_check_convergence(const LDPCCodec* ldpc);

/**
 * @brief Calculate syndrome
 * 
 * Computes the syndrome s = H * c for the current bit estimates.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_calculate_syndrome(LDPCCodec* ldpc);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get standard LDPC parameters for code rate
 * 
 * Returns standard parameters (n, k) for common LDPC code rates.
 * 
 * @param code_rate Desired code rate
 * @param n Pointer to store code length
 * @param k Pointer to store information length
 * @return FSO_SUCCESS if standard rate found, error code otherwise
 */
FSOErrorCode ldpc_get_standard_params(double code_rate, int* n, int* k);

/**
 * @brief Validate LDPC configuration
 * 
 * Checks if LDPC configuration parameters are valid and consistent.
 * 
 * @param config LDPC configuration to validate
 * @param n Code length
 * @param k Information length
 * @return FSO_SUCCESS if valid, error code if invalid
 */
FSOErrorCode ldpc_validate_config(const LDPCConfig* config, int n, int k);

/**
 * @brief Calculate theoretical minimum distance
 * 
 * Estimates the minimum distance of the LDPC code based on
 * the parity-check matrix structure.
 * 
 * @param ldpc Pointer to LDPC codec
 * @return Estimated minimum distance
 */
int ldpc_estimate_min_distance(const LDPCCodec* ldpc);

/**
 * @brief Convert soft bits to hard bits
 * 
 * Converts log-likelihood ratios to hard bit decisions.
 * 
 * @param llr Array of log-likelihood ratios
 * @param bits Output array of hard bits
 * @param length Number of bits
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_soft_to_hard(const double* llr, uint8_t* bits, size_t length);

/**
 * @brief Convert hard bits to soft bits
 * 
 * Converts hard bit decisions to log-likelihood ratios assuming
 * a given signal-to-noise ratio.
 * 
 * @param bits Array of hard bits
 * @param llr Output array of log-likelihood ratios
 * @param length Number of bits
 * @param snr_db Signal-to-noise ratio in dB
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode ldpc_hard_to_soft(const uint8_t* bits, double* llr, size_t length, double snr_db);

#endif /* LDPC_H */