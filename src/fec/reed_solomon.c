/**
 * @file reed_solomon.c
 * @brief Reed-Solomon error correction code implementation
 * 
 * This file implements Reed-Solomon encoding using Galois Field arithmetic
 * and systematic encoding.
 */

#include "reed_solomon.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define RS_MODULE "RS"

/* Primitive polynomials for common symbol sizes */
static const struct {
    int symbol_size;
    int primitive_poly;
} primitive_polys[] = {
    {3, 0x0b},    /* x^3 + x + 1 */
    {4, 0x13},    /* x^4 + x + 1 */
    {5, 0x25},    /* x^5 + x^2 + 1 */
    {6, 0x43},    /* x^6 + x + 1 */
    {7, 0x89},    /* x^7 + x^3 + 1 */
    {8, 0x11d},   /* x^8 + x^4 + x^3 + x^2 + 1 */
    {9, 0x211},   /* x^9 + x^4 + 1 */
    {10, 0x409},  /* x^10 + x^3 + 1 */
    {11, 0x805},  /* x^11 + x^2 + 1 */
    {12, 0x1053}, /* x^12 + x^6 + x^4 + x + 1 */
    {0, 0}        /* Terminator */
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static FSOErrorCode gf_build_tables(GaloisField* gf);
static int gf_mul_no_table(int a, int b, int primitive_poly, int field_size);

/* ============================================================================
 * Galois Field Functions
 * ============================================================================ */

FSOErrorCode gf_init(GaloisField* gf, int symbol_size, int primitive_poly)
{
    FSO_CHECK_NULL(gf);
    FSO_CHECK_PARAM(symbol_size >= 3 && symbol_size <= RS_MAX_SYMBOL_SIZE);
    FSO_CHECK_PARAM(primitive_poly > 0);
    
    memset(gf, 0, sizeof(GaloisField));
    
    gf->symbol_size = symbol_size;
    gf->field_size = 1 << symbol_size;  /* 2^m */
    gf->primitive_poly = primitive_poly;
    
    /* Allocate lookup tables */
    gf->exp_table = (int*)calloc(2 * gf->field_size, sizeof(int));
    gf->log_table = (int*)calloc(gf->field_size, sizeof(int));
    gf->inv_table = (int*)calloc(gf->field_size, sizeof(int));
    
    if (!gf->exp_table || !gf->log_table || !gf->inv_table) {
        FSO_LOG_ERROR(RS_MODULE, "Failed to allocate Galois Field tables");
        gf_free(gf);
        return FSO_ERROR_MEMORY;
    }
    
    /* Build lookup tables */
    FSOErrorCode result = gf_build_tables(gf);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(RS_MODULE, "Failed to build Galois Field tables");
        gf_free(gf);
        return result;
    }
    
    FSO_LOG_INFO(RS_MODULE, "Galois Field GF(2^%d) initialized with primitive poly 0x%x",
                symbol_size, primitive_poly);
    
    return FSO_SUCCESS;
}

FSOErrorCode gf_free(GaloisField* gf)
{
    FSO_CHECK_NULL(gf);
    
    if (gf->exp_table) {
        free(gf->exp_table);
        gf->exp_table = NULL;
    }
    
    if (gf->log_table) {
        free(gf->log_table);
        gf->log_table = NULL;
    }
    
    if (gf->inv_table) {
        free(gf->inv_table);
        gf->inv_table = NULL;
    }
    
    memset(gf, 0, sizeof(GaloisField));
    FSO_LOG_DEBUG(RS_MODULE, "Galois Field freed");
    
    return FSO_SUCCESS;
}

int gf_mul(const GaloisField* gf, int a, int b)
{
    if (a == 0 || b == 0) return 0;
    
    /* Use logarithm tables: log(a*b) = log(a) + log(b) */
    int log_sum = gf->log_table[a] + gf->log_table[b];
    if (log_sum >= gf->field_size - 1) {
        log_sum -= gf->field_size - 1;
    }
    
    return gf->exp_table[log_sum];
}

int gf_div(const GaloisField* gf, int a, int b)
{
    if (a == 0) return 0;
    if (b == 0) return 0; /* Division by zero - should not happen in valid RS */
    
    /* Use logarithm tables: log(a/b) = log(a) - log(b) */
    int log_diff = gf->log_table[a] - gf->log_table[b];
    if (log_diff < 0) {
        log_diff += gf->field_size - 1;
    }
    
    return gf->exp_table[log_diff];
}

int gf_pow(const GaloisField* gf, int base, int exponent)
{
    if (base == 0) return 0;
    if (exponent == 0) return 1;
    
    /* Use logarithm tables: log(a^n) = n * log(a) */
    int log_result = (gf->log_table[base] * exponent) % (gf->field_size - 1);
    return gf->exp_table[log_result];
}

int gf_inv(const GaloisField* gf, int a)
{
    if (a == 0) return 0; /* 0 has no inverse */
    return gf->inv_table[a];
}

/* ============================================================================
 * Polynomial Functions
 * ============================================================================ */

int poly_eval(const GaloisField* gf, const int* poly, int degree, int x)
{
    if (degree < 0) return 0;
    
    /* Horner's method: p(x) = a_n*x^n + ... + a_1*x + a_0 */
    int result = poly[0];
    for (int i = 1; i <= degree; i++) {
        result = gf_add(gf, gf_mul(gf, result, x), poly[i]);
    }
    
    return result;
}

FSOErrorCode poly_mul(const GaloisField* gf, const int* a, int a_degree,
                      const int* b, int b_degree, int* result, int* result_degree)
{
    FSO_CHECK_NULL(gf);
    FSO_CHECK_NULL(a);
    FSO_CHECK_NULL(b);
    FSO_CHECK_NULL(result);
    FSO_CHECK_NULL(result_degree);
    
    if (a_degree < 0 || b_degree < 0) {
        *result_degree = -1;
        return FSO_SUCCESS;
    }
    
    *result_degree = a_degree + b_degree;
    
    /* Initialize result to zero */
    for (int i = 0; i <= *result_degree; i++) {
        result[i] = 0;
    }
    
    /* Multiply polynomials */
    for (int i = 0; i <= a_degree; i++) {
        for (int j = 0; j <= b_degree; j++) {
            result[i + j] = gf_add(gf, result[i + j], gf_mul(gf, a[i], b[j]));
        }
    }
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Reed-Solomon Functions
 * ============================================================================ */

FSOErrorCode rs_init(RSCodec* rs, const RSConfig* config, int n, int k)
{
    FSO_CHECK_NULL(rs);
    FSO_CHECK_NULL(config);
    FSO_CHECK_PARAM(n > k && k > 0);
    FSO_CHECK_PARAM(n <= (1 << config->symbol_size) - 1);
    
    memset(rs, 0, sizeof(RSCodec));
    
    /* Initialize parameters */
    rs->n = n;
    rs->k = k;
    rs->num_roots = n - k;
    rs->t = rs->num_roots / 2;
    rs->first_root = config->first_root;
    rs->fcr = config->fcr;
    
    /* Initialize Galois Field */
    rs->gf = (GaloisField*)malloc(sizeof(GaloisField));
    if (!rs->gf) {
        FSO_LOG_ERROR(RS_MODULE, "Failed to allocate Galois Field");
        return FSO_ERROR_MEMORY;
    }
    
    FSOErrorCode result = gf_init(rs->gf, config->symbol_size, config->primitive_poly);
    if (result != FSO_SUCCESS) {
        free(rs->gf);
        rs->gf = NULL;
        return result;
    }
    
    /* Allocate workspace */
    rs->generator_poly = (int*)calloc(rs->num_roots + 1, sizeof(int));
    rs->syndrome = (int*)calloc(rs->num_roots, sizeof(int));
    rs->error_locator = (int*)calloc(rs->t + 1, sizeof(int));
    rs->error_evaluator = (int*)calloc(rs->t, sizeof(int));
    rs->error_positions = (int*)calloc(rs->t, sizeof(int));
    rs->error_values = (int*)calloc(rs->t, sizeof(int));
    
    if (!rs->generator_poly || !rs->syndrome || !rs->error_locator ||
        !rs->error_evaluator || !rs->error_positions || !rs->error_values) {
        FSO_LOG_ERROR(RS_MODULE, "Failed to allocate Reed-Solomon workspace");
        rs_free(rs);
        return FSO_ERROR_MEMORY;
    }
    
    /* Generate generator polynomial */
    result = rs_generate_generator_polynomial(rs);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(RS_MODULE, "Failed to generate generator polynomial");
        rs_free(rs);
        return result;
    }
    
    FSO_LOG_INFO(RS_MODULE, "Reed-Solomon codec initialized: RS(%d,%d) t=%d",
                n, k, rs->t);
    
    return FSO_SUCCESS;
}

FSOErrorCode rs_free(RSCodec* rs)
{
    FSO_CHECK_NULL(rs);
    
    if (rs->gf) {
        gf_free(rs->gf);
        free(rs->gf);
        rs->gf = NULL;
    }
    
    if (rs->generator_poly) {
        free(rs->generator_poly);
        rs->generator_poly = NULL;
    }
    
    if (rs->syndrome) {
        free(rs->syndrome);
        rs->syndrome = NULL;
    }
    
    if (rs->error_locator) {
        free(rs->error_locator);
        rs->error_locator = NULL;
    }
    
    if (rs->error_evaluator) {
        free(rs->error_evaluator);
        rs->error_evaluator = NULL;
    }
    
    if (rs->error_positions) {
        free(rs->error_positions);
        rs->error_positions = NULL;
    }
    
    if (rs->error_values) {
        free(rs->error_values);
        rs->error_values = NULL;
    }
    
    memset(rs, 0, sizeof(RSCodec));
    FSO_LOG_DEBUG(RS_MODULE, "Reed-Solomon codec freed");
    
    return FSO_SUCCESS;
}

FSOErrorCode rs_generate_generator_polynomial(RSCodec* rs)
{
    FSO_CHECK_NULL(rs);
    FSO_CHECK_NULL(rs->gf);
    FSO_CHECK_NULL(rs->generator_poly);
    
    /* Initialize generator polynomial to 1 */
    rs->generator_poly[0] = 1;
    rs->gen_poly_degree = 0;
    
    /* Build g(x) = (x - α^fcr) * (x - α^(fcr+1)) * ... * (x - α^(fcr+num_roots-1)) */
    FSO_LOG_INFO(RS_MODULE, "Generating generator polynomial with %d roots...", rs->num_roots);
    for (int i = 0; i < rs->num_roots; i++) {
        int root = gf_pow(rs->gf, 2, rs->fcr + i); /* α^(fcr+i) where α = 2 */
        
        /* Multiply by (x - root) */
        int temp[RS_MAX_PARITY_SYMBOLS + 1];
        memcpy(temp, rs->generator_poly, (rs->gen_poly_degree + 1) * sizeof(int));
        
        /* Shift polynomial (multiply by x) */
        for (int j = rs->gen_poly_degree; j >= 0; j--) {
            rs->generator_poly[j + 1] = temp[j];
        }
        rs->generator_poly[0] = 0;
        rs->gen_poly_degree++;
        
        /* Subtract root * polynomial (equivalent to adding in GF(2^m)) */
        for (int j = 0; j < rs->gen_poly_degree; j++) {
            rs->generator_poly[j] = gf_add(rs->gf, rs->generator_poly[j],
                                          gf_mul(rs->gf, root, temp[j]));
        }
    }
    
    FSO_LOG_INFO(RS_MODULE, "Generator polynomial generated successfully, degree: %d", rs->gen_poly_degree);
    
    return FSO_SUCCESS;
}

FSOErrorCode rs_encode(RSCodec* rs, const uint8_t* data, size_t data_len,
                       uint8_t* encoded, size_t* encoded_len)
{
    FSO_CHECK_NULL(rs);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(encoded);
    FSO_CHECK_PARAM(data_len == (size_t)rs->k);
    FSO_CHECK_NULL(encoded_len);
    FSO_CHECK_PARAM(*encoded_len >= (size_t)rs->n);
    
    /* Copy information symbols to the beginning of encoded array (systematic encoding) */
    for (int i = 0; i < rs->k; i++) {
        encoded[i] = data[i];
    }
    
    /* Initialize parity symbols to zero */
    for (int i = rs->k; i < rs->n; i++) {
        encoded[i] = 0;
    }
    
    /* Systematic encoding: compute parity symbols using polynomial division
     * We compute the remainder of data(x) * x^(n-k) divided by g(x)
     * This is equivalent to computing the parity check symbols */
    
    /* Create temporary array for polynomial division */
    int temp[RS_MAX_CODE_LENGTH];
    
    /* Copy data symbols and pad with zeros (multiply by x^(n-k)) */
    for (int i = 0; i < rs->k; i++) {
        temp[i] = data[i];
    }
    for (int i = rs->k; i < rs->n; i++) {
        temp[i] = 0;
    }
    
    /* Perform polynomial long division to get remainder (parity symbols) */
    for (int i = 0; i < rs->k; i++) {
        int feedback = temp[i];
        
        if (feedback != 0) {
            /* Subtract feedback * generator_poly from temp */
            for (int j = 0; j <= rs->gen_poly_degree; j++) {
                temp[i + j] = gf_add(rs->gf, temp[i + j], 
                                    gf_mul(rs->gf, feedback, rs->generator_poly[j]));
            }
        }
    }
    
    /* Copy the remainder (parity symbols) to the encoded array */
    for (int i = 0; i < rs->num_roots; i++) {
        encoded[rs->k + i] = temp[rs->k + i];
    }
    
    /* Set the actual encoded length */
    *encoded_len = rs->n;
    
    FSO_LOG_DEBUG(RS_MODULE, "Encoded %zu symbols to %d symbols", data_len, rs->n);
    
    return FSO_SUCCESS;
}

FSOErrorCode rs_calculate_syndrome(RSCodec* rs, const uint8_t* received, size_t received_len)
{
    FSO_CHECK_NULL(rs);
    FSO_CHECK_NULL(received);
    FSO_CHECK_PARAM(received_len == (size_t)rs->n);
    
    /* Calculate syndrome S_i = r(α^(fcr+i)) for i = 0, 1, ..., num_roots-1 */
    for (int i = 0; i < rs->num_roots; i++) {
        int alpha_power = gf_pow(rs->gf, 2, rs->fcr + i);
        rs->syndrome[i] = 0;
        
        /* Evaluate received polynomial at α^(fcr+i) */
        for (int j = 0; j < rs->n; j++) {
            rs->syndrome[i] = gf_add(rs->gf, rs->syndrome[i],
                                    gf_mul(rs->gf, received[j],
                                          gf_pow(rs->gf, alpha_power, j)));
        }
    }
    
    return FSO_SUCCESS;
}

int rs_has_errors(const RSCodec* rs)
{
    if (!rs || !rs->syndrome) return 0;
    
    /* Check if any syndrome is non-zero */
    for (int i = 0; i < rs->num_roots; i++) {
        if (rs->syndrome[i] != 0) {
            return 1;
        }
    }
    
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

int rs_is_primitive_poly_valid(int symbol_size, int primitive_poly)
{
    if (symbol_size < 3 || symbol_size > RS_MAX_SYMBOL_SIZE) return 0;
    if (primitive_poly <= 0) return 0;
    
    /* Check if polynomial has correct degree */
    int expected_degree = symbol_size;
    int degree = 0;
    int temp = primitive_poly;
    while (temp > 0) {
        degree++;
        temp >>= 1;
    }
    degree--; /* Adjust for bit position */
    
    return (degree == expected_degree);
}

int rs_get_default_primitive_poly(int symbol_size)
{
    for (int i = 0; primitive_polys[i].symbol_size != 0; i++) {
        if (primitive_polys[i].symbol_size == symbol_size) {
            return primitive_polys[i].primitive_poly;
        }
    }
    return 0; /* Not found */
}

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

static FSOErrorCode gf_build_tables(GaloisField* gf)
{
    /* Build exponential and logarithm tables */
    gf->exp_table[0] = 1;
    gf->log_table[0] = 0; /* Undefined, but set to 0 */
    gf->log_table[1] = 0;
    
    int x = 1;
    for (int i = 1; i < gf->field_size - 1; i++) {
        x = gf_mul_no_table(x, 2, gf->primitive_poly, gf->field_size);
        gf->exp_table[i] = x;
        gf->log_table[x] = i;
    }
    
    /* Extend exponential table for easier modular arithmetic */
    for (int i = gf->field_size - 1; i < 2 * gf->field_size - 2; i++) {
        gf->exp_table[i] = gf->exp_table[i - (gf->field_size - 1)];
    }
    
    /* Build inverse table */
    gf->inv_table[0] = 0; /* 0 has no inverse */
    for (int i = 1; i < gf->field_size; i++) {
        gf->inv_table[i] = gf->exp_table[gf->field_size - 1 - gf->log_table[i]];
    }
    
    return FSO_SUCCESS;
}

static int gf_mul_no_table(int a, int b, int primitive_poly, int field_size)
{
    int result = 0;
    
    while (b > 0) {
        if (b & 1) {
            result ^= a;
        }
        a <<= 1;
        if (a >= field_size) {
            a ^= primitive_poly;
        }
        b >>= 1;
    }
    
    return result;
}