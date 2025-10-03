/**
 * @file reed_solomon.h
 * @brief Reed-Solomon error correction code implementation
 * 
 * This file contains the Reed-Solomon encoder and decoder implementation
 * using Galois Field GF(2^m) arithmetic.
 */

#ifndef REED_SOLOMON_H
#define REED_SOLOMON_H

#include "fec.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Reed-Solomon Constants
 * ============================================================================ */

#define RS_MAX_SYMBOL_SIZE 16        /**< Maximum symbol size in bits */
#define RS_MAX_FIELD_SIZE 65536      /**< Maximum field size (2^16) */
#define RS_MAX_CODE_LENGTH 65535     /**< Maximum code length (2^m - 1) */
#define RS_MAX_PARITY_SYMBOLS 255    /**< Maximum parity symbols */

/* Common Reed-Solomon configurations */
#define RS_SYMBOL_SIZE_8 8           /**< 8-bit symbols (most common) */
#define RS_PRIMITIVE_POLY_8 0x11d    /**< Primitive polynomial for GF(256) */

/* ============================================================================
 * Reed-Solomon Structures
 * ============================================================================ */

/**
 * @brief Galois Field structure
 * 
 * Contains precomputed tables for efficient Galois Field arithmetic
 * in GF(2^m).
 */
typedef struct {
    int symbol_size;        /**< Symbol size in bits (m) */
    int field_size;         /**< Field size (2^m) */
    int primitive_poly;     /**< Primitive polynomial */
    
    /* Precomputed tables for fast arithmetic */
    int* exp_table;         /**< Exponential table (alpha^i) */
    int* log_table;         /**< Logarithm table (log_alpha(x)) */
    int* inv_table;         /**< Multiplicative inverse table */
} GaloisField;

/**
 * @brief Reed-Solomon codec state
 * 
 * Contains all state information for Reed-Solomon encoding and decoding.
 */
typedef struct {
    GaloisField* gf;        /**< Galois Field instance */
    
    /* Code parameters */
    int n;                  /**< Code length (total symbols) */
    int k;                  /**< Information symbols */
    int t;                  /**< Error correction capability */
    int num_roots;          /**< Number of generator polynomial roots */
    int first_root;         /**< First consecutive root */
    int fcr;                /**< First consecutive root index */
    
    /* Generator polynomial */
    int* generator_poly;    /**< Generator polynomial coefficients */
    int gen_poly_degree;    /**< Degree of generator polynomial */
    
    /* Workspace for encoding/decoding */
    int* syndrome;          /**< Syndrome calculation workspace */
    int* error_locator;     /**< Error locator polynomial */
    int* error_evaluator;   /**< Error evaluator polynomial */
    int* error_positions;   /**< Error position array */
    int* error_values;      /**< Error value array */
} RSCodec;

/* ============================================================================
 * Galois Field Functions
 * ============================================================================ */

/**
 * @brief Initialize Galois Field
 * 
 * Creates and initializes a Galois Field GF(2^m) with the specified
 * primitive polynomial. Precomputes lookup tables for efficient arithmetic.
 * 
 * @param gf Pointer to Galois Field structure
 * @param symbol_size Symbol size in bits (m)
 * @param primitive_poly Primitive polynomial
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode gf_init(GaloisField* gf, int symbol_size, int primitive_poly);

/**
 * @brief Free Galois Field resources
 * 
 * @param gf Pointer to Galois Field structure
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode gf_free(GaloisField* gf);

/**
 * @brief Galois Field addition (XOR)
 * 
 * @param gf Pointer to Galois Field
 * @param a First operand
 * @param b Second operand
 * @return a + b in GF(2^m)
 */
static inline int gf_add(const GaloisField* gf, int a, int b) {
    (void)gf; /* Addition is just XOR in GF(2^m) */
    return a ^ b;
}

/**
 * @brief Galois Field subtraction (same as addition in GF(2^m))
 * 
 * @param gf Pointer to Galois Field
 * @param a First operand
 * @param b Second operand
 * @return a - b in GF(2^m)
 */
static inline int gf_sub(const GaloisField* gf, int a, int b) {
    return gf_add(gf, a, b);
}

/**
 * @brief Galois Field multiplication
 * 
 * @param gf Pointer to Galois Field
 * @param a First operand
 * @param b Second operand
 * @return a * b in GF(2^m)
 */
int gf_mul(const GaloisField* gf, int a, int b);

/**
 * @brief Galois Field division
 * 
 * @param gf Pointer to Galois Field
 * @param a Dividend
 * @param b Divisor
 * @return a / b in GF(2^m)
 */
int gf_div(const GaloisField* gf, int a, int b);

/**
 * @brief Galois Field power
 * 
 * @param gf Pointer to Galois Field
 * @param base Base value
 * @param exponent Exponent
 * @return base^exponent in GF(2^m)
 */
int gf_pow(const GaloisField* gf, int base, int exponent);

/**
 * @brief Galois Field multiplicative inverse
 * 
 * @param gf Pointer to Galois Field
 * @param a Value to invert
 * @return a^(-1) in GF(2^m)
 */
int gf_inv(const GaloisField* gf, int a);

/* ============================================================================
 * Polynomial Functions
 * ============================================================================ */

/**
 * @brief Evaluate polynomial at given point
 * 
 * @param gf Pointer to Galois Field
 * @param poly Polynomial coefficients (highest degree first)
 * @param degree Degree of polynomial
 * @param x Point to evaluate at
 * @return poly(x) in GF(2^m)
 */
int poly_eval(const GaloisField* gf, const int* poly, int degree, int x);

/**
 * @brief Multiply two polynomials
 * 
 * @param gf Pointer to Galois Field
 * @param a First polynomial
 * @param a_degree Degree of first polynomial
 * @param b Second polynomial
 * @param b_degree Degree of second polynomial
 * @param result Result polynomial (must be pre-allocated)
 * @param result_degree Pointer to store result degree
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode poly_mul(const GaloisField* gf, const int* a, int a_degree,
                      const int* b, int b_degree, int* result, int* result_degree);

/**
 * @brief Divide polynomials (with remainder)
 * 
 * @param gf Pointer to Galois Field
 * @param dividend Dividend polynomial
 * @param dividend_degree Degree of dividend
 * @param divisor Divisor polynomial
 * @param divisor_degree Degree of divisor
 * @param quotient Quotient polynomial (can be NULL)
 * @param quotient_degree Pointer to store quotient degree (can be NULL)
 * @param remainder Remainder polynomial (can be NULL)
 * @param remainder_degree Pointer to store remainder degree (can be NULL)
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode poly_div(const GaloisField* gf, const int* dividend, int dividend_degree,
                      const int* divisor, int divisor_degree,
                      int* quotient, int* quotient_degree,
                      int* remainder, int* remainder_degree);

/* ============================================================================
 * Reed-Solomon Functions
 * ============================================================================ */

/**
 * @brief Initialize Reed-Solomon codec
 * 
 * @param rs Pointer to Reed-Solomon codec structure
 * @param config Reed-Solomon configuration
 * @param n Code length
 * @param k Information length
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_init(RSCodec* rs, const RSConfig* config, int n, int k);

/**
 * @brief Free Reed-Solomon codec resources
 * 
 * @param rs Pointer to Reed-Solomon codec structure
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_free(RSCodec* rs);

/**
 * @brief Generate Reed-Solomon generator polynomial
 * 
 * Creates the generator polynomial g(x) = (x - α^fcr) * (x - α^(fcr+1)) * ... * (x - α^(fcr+num_roots-1))
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_generate_generator_polynomial(RSCodec* rs);

/**
 * @brief Encode data using Reed-Solomon
 * 
 * Performs systematic Reed-Solomon encoding. The input data is treated as
 * the information part of the codeword, and parity symbols are appended.
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @param data Input data symbols
 * @param data_len Length of input data
 * @param encoded Output encoded symbols
 * @param encoded_len Length of output buffer
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_encode(RSCodec* rs, const uint8_t* data, size_t data_len,
                       uint8_t* encoded, size_t* encoded_len);

/**
 * @brief Calculate syndrome for received codeword
 * 
 * Computes the syndrome S_i = r(α^(fcr+i)) for i = 0, 1, ..., num_roots-1
 * where r(x) is the received polynomial.
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @param received Received codeword
 * @param received_len Length of received codeword
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_calculate_syndrome(RSCodec* rs, const uint8_t* received, size_t received_len);

/**
 * @brief Check if syndrome indicates errors
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @return 1 if errors detected, 0 if no errors
 */
int rs_has_errors(const RSCodec* rs);

/**
 * @brief Berlekamp-Massey algorithm for error locator polynomial
 * 
 * Computes the error locator polynomial Λ(x) from the syndrome.
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @return Number of errors found, or -1 on failure
 */
int rs_berlekamp_massey(RSCodec* rs);

/**
 * @brief Chien search for error locations
 * 
 * Finds the roots of the error locator polynomial to determine
 * error positions in the received codeword.
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @param num_errors Number of errors from Berlekamp-Massey
 * @return Number of error positions found
 */
int rs_chien_search(RSCodec* rs, int num_errors);

/**
 * @brief Forney algorithm for error values
 * 
 * Computes the error values at the error positions using the
 * error locator and error evaluator polynomials.
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @param num_errors Number of errors found
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_forney_algorithm(RSCodec* rs, int num_errors);

/**
 * @brief Decode received codeword using Reed-Solomon
 * 
 * Performs Reed-Solomon decoding with error correction. The function
 * calculates syndromes, finds error locations using Berlekamp-Massey
 * and Chien search, computes error values using Forney algorithm,
 * and corrects the errors.
 * 
 * @param rs Pointer to Reed-Solomon codec
 * @param received Received codeword (may contain errors)
 * @param received_len Length of received codeword
 * @param decoded Output decoded data
 * @param decoded_len Length of output buffer
 * @param errors_corrected Pointer to store number of errors corrected (can be NULL)
 * @return FSO_SUCCESS on success, error code on failure
 */
FSOErrorCode rs_decode(RSCodec* rs, const uint8_t* received, size_t received_len,
                       uint8_t* decoded, size_t decoded_len, int* errors_corrected);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if primitive polynomial is valid
 * 
 * @param symbol_size Symbol size in bits
 * @param primitive_poly Primitive polynomial to check
 * @return 1 if valid, 0 if invalid
 */
int rs_is_primitive_poly_valid(int symbol_size, int primitive_poly);

/**
 * @brief Get default primitive polynomial for symbol size
 * 
 * @param symbol_size Symbol size in bits
 * @return Default primitive polynomial, or 0 if unsupported
 */
int rs_get_default_primitive_poly(int symbol_size);

/**
 * @brief Calculate maximum correctable errors
 * 
 * @param num_parity_symbols Number of parity symbols
 * @return Maximum correctable errors (t = num_parity_symbols / 2)
 */
static inline int rs_max_correctable_errors(int num_parity_symbols) {
    return num_parity_symbols / 2;
}

/**
 * @brief Calculate minimum parity symbols needed
 * 
 * @param max_errors Maximum errors to correct
 * @return Minimum parity symbols needed (2 * max_errors)
 */
static inline int rs_min_parity_symbols(int max_errors) {
    return 2 * max_errors;
}

#endif /* REED_SOLOMON_H */