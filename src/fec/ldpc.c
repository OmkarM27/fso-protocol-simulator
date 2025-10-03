/**
 * @file ldpc.c
 * @brief Low-Density Parity-Check (LDPC) error correction code implementation
 * 
 * This file implements LDPC encoding using sparse parity-check matrices
 * and systematic encoding with support for configurable code rates.
 */

#include "ldpc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LDPC_MODULE "LDPC"

/* Standard LDPC code parameters for common rates */
static const struct {
    double code_rate;
    int n;      /* Code length */
    int k;      /* Information length */
} standard_ldpc_params[] = {
    {LDPC_RATE_1_2, 1024, 512},   /* Rate 1/2 */
    {LDPC_RATE_1_2, 2048, 1024},  /* Rate 1/2 */
    {LDPC_RATE_2_3, 1536, 1024},  /* Rate 2/3 */
    {LDPC_RATE_2_3, 3072, 2048},  /* Rate 2/3 */
    {LDPC_RATE_3_4, 2048, 1536},  /* Rate 3/4 */
    {LDPC_RATE_3_4, 4096, 3072},  /* Rate 3/4 */
    {LDPC_RATE_5_6, 3072, 2560},  /* Rate 5/6 */
    {LDPC_RATE_5_6, 6144, 5120},  /* Rate 5/6 */
    {0.0, 0, 0}                   /* Terminator */
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static FSOErrorCode ldpc_create_regular_matrix(LDPCCodec* ldpc, int dv, int dc);
static FSOErrorCode ldpc_gaussian_elimination(SparseMatrix* H, SparseMatrix* G, int k);
static int gcd(int a, int b);

/* ============================================================================
 * LDPC Functions
 * ============================================================================ */

FSOErrorCode ldpc_init(LDPCCodec* ldpc, const LDPCConfig* config, int n, int k)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(config);
    FSO_CHECK_PARAM(n > k && k > 0);
    FSO_CHECK_PARAM(n <= LDPC_MAX_CODE_LENGTH);
    
    memset(ldpc, 0, sizeof(LDPCCodec));
    
    /* Initialize parameters */
    ldpc->n = n;
    ldpc->k = k;
    ldpc->m = n - k;
    ldpc->code_rate = (double)k / (double)n;
    ldpc->max_iterations = config->max_iterations > 0 ? config->max_iterations : LDPC_MAX_ITERATIONS;
    ldpc->convergence_threshold = config->convergence_threshold > 0 ? 
                                  config->convergence_threshold : LDPC_DEFAULT_CONVERGENCE_THRESHOLD;
    
    /* Allocate parity-check matrix */
    ldpc->H = (SparseMatrix*)malloc(sizeof(SparseMatrix));
    if (!ldpc->H) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate parity-check matrix");
        return FSO_ERROR_MEMORY;
    }
    
    /* Allocate generator matrix */
    ldpc->G = (SparseMatrix*)malloc(sizeof(SparseMatrix));
    if (!ldpc->G) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate generator matrix");
        free(ldpc->H);
        ldpc->H = NULL;
        return FSO_ERROR_MEMORY;
    }
    
    /* Allocate workspace arrays */
    ldpc->variable_to_check = (double*)calloc(n * ldpc->m, sizeof(double));
    ldpc->check_to_variable = (double*)calloc(n * ldpc->m, sizeof(double));
    ldpc->posterior_llr = (double*)calloc(n, sizeof(double));
    ldpc->channel_llr = (double*)calloc(n, sizeof(double));
    ldpc->decoded_bits = (int*)calloc(n, sizeof(int));
    ldpc->syndrome = (int*)calloc(ldpc->m, sizeof(int));
    
    if (!ldpc->variable_to_check || !ldpc->check_to_variable || 
        !ldpc->posterior_llr || !ldpc->channel_llr || 
        !ldpc->decoded_bits || !ldpc->syndrome) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate LDPC workspace");
        ldpc_free(ldpc);
        return FSO_ERROR_MEMORY;
    }
    
    /* Allocate connectivity arrays */
    ldpc->var_degree = (int*)calloc(n, sizeof(int));
    ldpc->check_degree = (int*)calloc(ldpc->m, sizeof(int));
    ldpc->var_to_check_edges = (int**)calloc(n, sizeof(int*));
    ldpc->check_to_var_edges = (int**)calloc(ldpc->m, sizeof(int*));
    
    if (!ldpc->var_degree || !ldpc->check_degree || 
        !ldpc->var_to_check_edges || !ldpc->check_to_var_edges) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate connectivity arrays");
        ldpc_free(ldpc);
        return FSO_ERROR_MEMORY;
    }
    
    FSO_LOG_INFO(LDPC_MODULE, "LDPC codec initialized: LDPC(%d,%d) rate=%.3f",
                n, k, ldpc->code_rate);
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_free(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    
    /* Free sparse matrices */
    if (ldpc->H) {
        sparse_matrix_free(ldpc->H);
        free(ldpc->H);
        ldpc->H = NULL;
    }
    
    if (ldpc->G) {
        sparse_matrix_free(ldpc->G);
        free(ldpc->G);
        ldpc->G = NULL;
    }
    
    /* Free workspace arrays */
    if (ldpc->variable_to_check) {
        free(ldpc->variable_to_check);
        ldpc->variable_to_check = NULL;
    }
    
    if (ldpc->check_to_variable) {
        free(ldpc->check_to_variable);
        ldpc->check_to_variable = NULL;
    }
    
    if (ldpc->posterior_llr) {
        free(ldpc->posterior_llr);
        ldpc->posterior_llr = NULL;
    }
    
    if (ldpc->channel_llr) {
        free(ldpc->channel_llr);
        ldpc->channel_llr = NULL;
    }
    
    if (ldpc->decoded_bits) {
        free(ldpc->decoded_bits);
        ldpc->decoded_bits = NULL;
    }
    
    if (ldpc->syndrome) {
        free(ldpc->syndrome);
        ldpc->syndrome = NULL;
    }
    
    /* Free connectivity arrays */
    if (ldpc->var_to_check_edges) {
        for (int i = 0; i < ldpc->n; i++) {
            if (ldpc->var_to_check_edges[i]) {
                free(ldpc->var_to_check_edges[i]);
            }
        }
        free(ldpc->var_to_check_edges);
        ldpc->var_to_check_edges = NULL;
    }
    
    if (ldpc->check_to_var_edges) {
        for (int i = 0; i < ldpc->m; i++) {
            if (ldpc->check_to_var_edges[i]) {
                free(ldpc->check_to_var_edges[i]);
            }
        }
        free(ldpc->check_to_var_edges);
        ldpc->check_to_var_edges = NULL;
    }
    
    if (ldpc->var_degree) {
        free(ldpc->var_degree);
        ldpc->var_degree = NULL;
    }
    
    if (ldpc->check_degree) {
        free(ldpc->check_degree);
        ldpc->check_degree = NULL;
    }
    
    memset(ldpc, 0, sizeof(LDPCCodec));
    FSO_LOG_DEBUG(LDPC_MODULE, "LDPC codec freed");
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_generate_standard_matrix(LDPCCodec* ldpc, double code_rate)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_PARAM(code_rate > 0.0 && code_rate < 1.0);
    
    /* Determine variable and check node degrees based on code rate */
    int dv, dc; /* Variable node degree, check node degree */
    
    if (fabs(code_rate - LDPC_RATE_1_2) < 1e-6) {
        dv = 3; dc = 6;  /* Regular (3,6) LDPC for rate 1/2 */
    } else if (fabs(code_rate - LDPC_RATE_2_3) < 1e-6) {
        dv = 4; dc = 8;  /* Regular (4,8) LDPC for rate 2/3 */
    } else if (fabs(code_rate - LDPC_RATE_3_4) < 1e-6) {
        dv = 5; dc = 10; /* Regular (5,10) LDPC for rate 3/4 */
    } else if (fabs(code_rate - LDPC_RATE_5_6) < 1e-6) {
        dv = 6; dc = 12; /* Regular (6,12) LDPC for rate 5/6 */
    } else {
        /* Default to (3,6) for other rates */
        dv = 3; dc = 6;
        FSO_LOG_WARNING(LDPC_MODULE, "Using default (3,6) LDPC for rate %.3f", code_rate);
    }
    
    /* Create regular LDPC matrix */
    FSOErrorCode result = ldpc_create_regular_matrix(ldpc, dv, dc);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to create regular LDPC matrix");
        return result;
    }
    
    FSO_LOG_INFO(LDPC_MODULE, "Generated (%d,%d) regular LDPC matrix for rate %.3f", 
                dv, dc, code_rate);
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_generate_generator_matrix(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(ldpc->H);
    
    /* Initialize generator matrix */
    FSOErrorCode result = sparse_matrix_init(ldpc->G, ldpc->k, ldpc->n, ldpc->k * ldpc->k);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to initialize generator matrix");
        return result;
    }
    
    /* Perform Gaussian elimination to get systematic form */
    result = ldpc_gaussian_elimination(ldpc->H, ldpc->G, ldpc->k);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to perform Gaussian elimination");
        return result;
    }
    
    /* Convert to CSR format for efficient multiplication */
    result = sparse_matrix_to_csr(ldpc->G);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to convert generator matrix to CSR");
        return result;
    }
    
    FSO_LOG_INFO(LDPC_MODULE, "Generated systematic generator matrix G(%d,%d)", 
                ldpc->k, ldpc->n);
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_encode(LDPCCodec* ldpc, const uint8_t* data, size_t data_len,
                         uint8_t* encoded, size_t* encoded_len)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(encoded);
    FSO_CHECK_NULL(encoded_len);
    FSO_CHECK_PARAM(data_len == (size_t)ldpc->k);
    FSO_CHECK_PARAM(*encoded_len >= (size_t)ldpc->n);
    
    /* Check if generator matrix is available */
    if (!ldpc->G || !ldpc->G->row_ptr) {
        FSO_LOG_ERROR(LDPC_MODULE, "Generator matrix not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    /* Initialize all bits to zero */
    memset(encoded, 0, ldpc->n * sizeof(uint8_t));
    
    /* Copy information bits to the beginning (systematic encoding) */
    for (int i = 0; i < ldpc->k; i++) {
        encoded[i] = data[i] & 1; /* Ensure binary */
    }
    
    /* Compute parity bits using generator matrix multiplication
     * For systematic form G = [I | P], parity bits are computed as P * u
     * where u is the information vector and P is the parity part of G */
    
    /* Use direct binary arithmetic for efficiency */
    for (int i = 0; i < ldpc->k; i++) {
        if (data[i] & 1) { /* If information bit is 1 */
            /* Add corresponding row of P to parity bits (XOR operation) */
            int row_start = ldpc->G->row_ptr[i];
            int row_end = ldpc->G->row_ptr[i + 1];
            
            for (int j = row_start; j < row_end; j++) {
                int col = ldpc->G->col_indices[j];
                if (col >= ldpc->k && ldpc->G->values[j] == 1) {
                    /* XOR with parity bit */
                    encoded[col] ^= 1;
                }
            }
        }
    }
    
    /* Set actual encoded length */
    *encoded_len = ldpc->n;
    
    /* Verify encoding by checking syndrome (optional debug check) */
    #ifdef DEBUG_LDPC_ENCODING
    int syndrome_check = 1;
    for (int i = 0; i < ldpc->H->rows && syndrome_check; i++) {
        int syndrome_bit = 0;
        int row_start = ldpc->H->row_ptr[i];
        int row_end = ldpc->H->row_ptr[i + 1];
        
        for (int j = row_start; j < row_end; j++) {
            int col = ldpc->H->col_indices[j];
            if (col < ldpc->n && ldpc->H->values[j] == 1) {
                syndrome_bit ^= encoded[col];
            }
        }
        
        if (syndrome_bit != 0) {
            FSO_LOG_WARNING(LDPC_MODULE, "Syndrome check failed at row %d", i);
            syndrome_check = 0;
        }
    }
    
    if (syndrome_check) {
        FSO_LOG_DEBUG(LDPC_MODULE, "Encoding syndrome check passed");
    }
    #endif
    
    FSO_LOG_DEBUG(LDPC_MODULE, "Encoded %zu information bits to %d total bits (rate %.3f)",
                 data_len, ldpc->n, ldpc->code_rate);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Sparse Matrix Functions
 * ============================================================================ */

FSOErrorCode sparse_matrix_init(SparseMatrix* matrix, int rows, int cols, int nnz)
{
    FSO_CHECK_NULL(matrix);
    FSO_CHECK_PARAM(rows > 0 && cols > 0 && nnz >= 0);
    
    memset(matrix, 0, sizeof(SparseMatrix));
    
    matrix->rows = rows;
    matrix->cols = cols;
    matrix->nnz = nnz;
    
    if (nnz > 0) {
        matrix->elements = (SparseElement*)calloc(nnz, sizeof(SparseElement));
        matrix->row_ptr = (int*)calloc(rows + 1, sizeof(int));
        matrix->col_indices = (int*)calloc(nnz, sizeof(int));
        matrix->values = (int*)calloc(nnz, sizeof(int));
        
        if (!matrix->elements || !matrix->row_ptr || !matrix->col_indices || !matrix->values) {
            sparse_matrix_free(matrix);
            return FSO_ERROR_MEMORY;
        }
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode sparse_matrix_free(SparseMatrix* matrix)
{
    FSO_CHECK_NULL(matrix);
    
    if (matrix->elements) {
        free(matrix->elements);
        matrix->elements = NULL;
    }
    
    if (matrix->row_ptr) {
        free(matrix->row_ptr);
        matrix->row_ptr = NULL;
    }
    
    if (matrix->col_indices) {
        free(matrix->col_indices);
        matrix->col_indices = NULL;
    }
    
    if (matrix->values) {
        free(matrix->values);
        matrix->values = NULL;
    }
    
    memset(matrix, 0, sizeof(SparseMatrix));
    
    return FSO_SUCCESS;
}

FSOErrorCode sparse_matrix_set(SparseMatrix* matrix, int row, int col, int value)
{
    FSO_CHECK_NULL(matrix);
    FSO_CHECK_PARAM(row >= 0 && row < matrix->rows);
    FSO_CHECK_PARAM(col >= 0 && col < matrix->cols);
    
    /* Find insertion point or existing element */
    int insert_idx = 0;
    for (int i = 0; i < matrix->nnz; i++) {
        if (matrix->elements[i].row == row && matrix->elements[i].col == col) {
            /* Update existing element */
            matrix->elements[i].value = value;
            return FSO_SUCCESS;
        }
        if (matrix->elements[i].row < row || 
            (matrix->elements[i].row == row && matrix->elements[i].col < col)) {
            insert_idx = i + 1;
        }
    }
    
    /* Insert new element (assuming we have space) */
    if (insert_idx < matrix->nnz) {
        /* Shift elements to make room */
        memmove(&matrix->elements[insert_idx + 1], &matrix->elements[insert_idx],
                (matrix->nnz - insert_idx) * sizeof(SparseElement));
    }
    
    matrix->elements[insert_idx].row = row;
    matrix->elements[insert_idx].col = col;
    matrix->elements[insert_idx].value = value;
    
    return FSO_SUCCESS;
}

int sparse_matrix_get(const SparseMatrix* matrix, int row, int col)
{
    if (!matrix || row < 0 || row >= matrix->rows || col < 0 || col >= matrix->cols) {
        return 0;
    }
    
    /* Search for element */
    for (int i = 0; i < matrix->nnz; i++) {
        if (matrix->elements[i].row == row && matrix->elements[i].col == col) {
            return matrix->elements[i].value;
        }
    }
    
    return 0; /* Not found */
}

FSOErrorCode sparse_matrix_to_csr(SparseMatrix* matrix)
{
    FSO_CHECK_NULL(matrix);
    
    if (matrix->nnz == 0) {
        return FSO_SUCCESS;
    }
    
    /* Sort elements by row, then by column */
    for (int i = 0; i < matrix->nnz - 1; i++) {
        for (int j = i + 1; j < matrix->nnz; j++) {
            if (matrix->elements[i].row > matrix->elements[j].row ||
                (matrix->elements[i].row == matrix->elements[j].row &&
                 matrix->elements[i].col > matrix->elements[j].col)) {
                /* Swap elements */
                SparseElement temp = matrix->elements[i];
                matrix->elements[i] = matrix->elements[j];
                matrix->elements[j] = temp;
            }
        }
    }
    
    /* Build CSR arrays */
    int current_row = 0;
    matrix->row_ptr[0] = 0;
    
    for (int i = 0; i < matrix->nnz; i++) {
        /* Fill row pointers for empty rows */
        while (current_row < matrix->elements[i].row) {
            current_row++;
            matrix->row_ptr[current_row] = i;
        }
        
        matrix->col_indices[i] = matrix->elements[i].col;
        matrix->values[i] = matrix->elements[i].value;
    }
    
    /* Fill remaining row pointers */
    while (current_row < matrix->rows) {
        current_row++;
        matrix->row_ptr[current_row] = matrix->nnz;
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode sparse_matrix_vector_multiply(const SparseMatrix* matrix, 
                                           const double* x, double* y)
{
    FSO_CHECK_NULL(matrix);
    FSO_CHECK_NULL(x);
    FSO_CHECK_NULL(y);
    
    /* Initialize output vector */
    for (int i = 0; i < matrix->rows; i++) {
        y[i] = 0.0;
    }
    
    /* Perform multiplication using CSR format */
    if (matrix->row_ptr && matrix->col_indices && matrix->values) {
        /* Use CSR format for efficient multiplication */
        for (int i = 0; i < matrix->rows; i++) {
            int row_start = matrix->row_ptr[i];
            int row_end = matrix->row_ptr[i + 1];
            
            for (int j = row_start; j < row_end; j++) {
                int col = matrix->col_indices[j];
                if (col < matrix->cols) {
                    y[i] += (double)matrix->values[j] * x[col];
                }
            }
        }
    } else {
        /* Fallback to element-wise multiplication */
        for (int i = 0; i < matrix->nnz; i++) {
            int row = matrix->elements[i].row;
            int col = matrix->elements[i].col;
            if (row < matrix->rows && col < matrix->cols) {
                y[row] += (double)matrix->elements[i].value * x[col];
            }
        }
    }
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

FSOErrorCode ldpc_get_standard_params(double code_rate, int* n, int* k)
{
    FSO_CHECK_NULL(n);
    FSO_CHECK_NULL(k);
    
    /* Find matching standard parameters */
    for (int i = 0; standard_ldpc_params[i].code_rate > 0; i++) {
        if (fabs(standard_ldpc_params[i].code_rate - code_rate) < 1e-6) {
            *n = standard_ldpc_params[i].n;
            *k = standard_ldpc_params[i].k;
            return FSO_SUCCESS;
        }
    }
    
    /* If no exact match, use default parameters */
    *n = 1024;
    *k = (int)(code_rate * (*n));
    
    FSO_LOG_WARNING(LDPC_MODULE, "Using default parameters n=%d, k=%d for rate %.3f", 
                   *n, *k, code_rate);
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_validate_config(const LDPCConfig* config, int n, int k)
{
    FSO_CHECK_NULL(config);
    FSO_CHECK_PARAM(n > k && k > 0);
    FSO_CHECK_PARAM(n <= LDPC_MAX_CODE_LENGTH);
    FSO_CHECK_PARAM(config->max_iterations > 0 && config->max_iterations <= LDPC_MAX_ITERATIONS);
    FSO_CHECK_PARAM(config->convergence_threshold > 0.0);
    
    double code_rate = (double)k / (double)n;
    FSO_CHECK_PARAM(code_rate > 0.0 && code_rate < 1.0);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Static Helper Functions
 * ============================================================================ */

static FSOErrorCode ldpc_create_regular_matrix(LDPCCodec* ldpc, int dv, int dc)
{
    /* Create a regular LDPC matrix with variable degree dv and check degree dc
     * Uses a structured construction with cyclic shifts to avoid short cycles */
    
    int m = ldpc->m;
    int n = ldpc->n;
    
    /* Calculate number of edges */
    int num_edges = n * dv;
    if (num_edges != m * dc) {
        FSO_LOG_ERROR(LDPC_MODULE, "Invalid degree combination: n*dv != m*dc (%d != %d)", 
                     num_edges, m * dc);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    /* Initialize parity-check matrix */
    FSOErrorCode result = sparse_matrix_init(ldpc->H, m, n, num_edges);
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    /* Improved regular construction using cyclic shifts to avoid 4-cycles */
    int edge_count = 0;
    int shift_increment = FSO_MAX(1, m / dv);
    
    for (int v = 0; v < n; v++) {
        for (int d = 0; d < dv; d++) {
            /* Use cyclic shift pattern to distribute connections */
            int base_check = (v * dv + d) % m;
            int check_node = (base_check + d * shift_increment) % m;
            
            /* Avoid duplicate connections */
            int duplicate = 0;
            for (int prev = 0; prev < edge_count; prev++) {
                if (ldpc->H->elements[prev].row == check_node && 
                    ldpc->H->elements[prev].col == v) {
                    duplicate = 1;
                    break;
                }
            }
            
            if (!duplicate) {
                /* Set element in sparse matrix */
                ldpc->H->elements[edge_count].row = check_node;
                ldpc->H->elements[edge_count].col = v;
                ldpc->H->elements[edge_count].value = 1;
                edge_count++;
            } else {
                /* Find alternative check node */
                for (int alt = 1; alt < m && edge_count < num_edges; alt++) {
                    int alt_check = (check_node + alt) % m;
                    int alt_duplicate = 0;
                    
                    for (int prev = 0; prev < edge_count; prev++) {
                        if (ldpc->H->elements[prev].row == alt_check && 
                            ldpc->H->elements[prev].col == v) {
                            alt_duplicate = 1;
                            break;
                        }
                    }
                    
                    if (!alt_duplicate) {
                        ldpc->H->elements[edge_count].row = alt_check;
                        ldpc->H->elements[edge_count].col = v;
                        ldpc->H->elements[edge_count].value = 1;
                        edge_count++;
                        break;
                    }
                }
            }
        }
    }
    
    ldpc->H->nnz = edge_count;
    
    /* Convert to CSR format */
    result = sparse_matrix_to_csr(ldpc->H);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to convert H matrix to CSR");
        return result;
    }
    
    /* Initialize connectivity arrays for message passing */
    result = ldpc_init_message_graph(ldpc);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to initialize message passing graph");
        return result;
    }
    
    FSO_LOG_INFO(LDPC_MODULE, "Created regular LDPC matrix: %d edges, avg var degree %.2f, avg check degree %.2f",
                edge_count, (double)edge_count / n, (double)edge_count / m);
    
    return FSO_SUCCESS;
}

static FSOErrorCode ldpc_gaussian_elimination(SparseMatrix* H, SparseMatrix* G, int k)
{
    /* Generate systematic generator matrix from parity-check matrix
     * For systematic LDPC codes, we need G = [I | P] where H = [P^T | I]
     * This is a simplified approach assuming the H matrix can be put in systematic form */
    
    int n = H->cols;
    int m = H->rows;
    
    /* Allocate temporary matrix for H manipulation */
    int** H_temp = (int**)calloc(m, sizeof(int*));
    if (!H_temp) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate temporary H matrix");
        return FSO_ERROR_MEMORY;
    }
    
    for (int i = 0; i < m; i++) {
        H_temp[i] = (int*)calloc(n, sizeof(int));
        if (!H_temp[i]) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                free(H_temp[j]);
            }
            free(H_temp);
            return FSO_ERROR_MEMORY;
        }
    }
    
    /* Copy H matrix to temporary dense format for manipulation */
    for (int i = 0; i < H->nnz; i++) {
        int row = H->elements[i].row;
        int col = H->elements[i].col;
        if (row < m && col < n) {
            H_temp[row][col] = H->elements[i].value;
        }
    }
    
    /* Perform Gaussian elimination over GF(2) to get systematic form [P^T | I] */
    int pivot_row = 0;
    for (int col = k; col < n && pivot_row < m; col++) {
        /* Find pivot */
        int pivot_found = 0;
        for (int row = pivot_row; row < m; row++) {
            if (H_temp[row][col] == 1) {
                /* Swap rows if needed */
                if (row != pivot_row) {
                    for (int c = 0; c < n; c++) {
                        int temp = H_temp[pivot_row][c];
                        H_temp[pivot_row][c] = H_temp[row][c];
                        H_temp[row][c] = temp;
                    }
                }
                pivot_found = 1;
                break;
            }
        }
        
        if (!pivot_found) {
            continue; /* Skip this column */
        }
        
        /* Eliminate other 1s in this column */
        for (int row = 0; row < m; row++) {
            if (row != pivot_row && H_temp[row][col] == 1) {
                /* XOR rows (GF(2) addition) */
                for (int c = 0; c < n; c++) {
                    H_temp[row][c] ^= H_temp[pivot_row][c];
                }
            }
        }
        
        pivot_row++;
    }
    
    /* Build generator matrix G = [I | P] where P is extracted from systematic H */
    int nnz_count = 0;
    
    /* Information part: identity matrix I */
    for (int i = 0; i < k; i++) {
        if (nnz_count < G->nnz) {
            G->elements[nnz_count].row = i;
            G->elements[nnz_count].col = i;
            G->elements[nnz_count].value = 1;
            nnz_count++;
        }
    }
    
    /* Parity part: P matrix (transpose of P^T from H) */
    for (int i = 0; i < k && i < m; i++) {
        for (int j = k; j < n; j++) {
            if (H_temp[i][j] == 1 && nnz_count < G->nnz) {
                G->elements[nnz_count].row = i;
                G->elements[nnz_count].col = j;
                G->elements[nnz_count].value = 1;
                nnz_count++;
            }
        }
    }
    
    G->nnz = nnz_count;
    
    /* Cleanup temporary matrix */
    for (int i = 0; i < m; i++) {
        free(H_temp[i]);
    }
    free(H_temp);
    
    FSO_LOG_DEBUG(LDPC_MODULE, "Generated systematic generator matrix with %d non-zero elements", nnz_count);
    
    return FSO_SUCCESS;
}

int ldpc_estimate_min_distance(const LDPCCodec* ldpc)
{
    if (!ldpc || !ldpc->H) {
        return 0;
    }
    
    /* For regular LDPC codes, minimum distance is typically related to girth
     * This is a rough estimate based on variable node degree */
    int min_var_degree = ldpc->n;
    for (int i = 0; i < ldpc->n; i++) {
        if (ldpc->var_degree[i] > 0 && ldpc->var_degree[i] < min_var_degree) {
            min_var_degree = ldpc->var_degree[i];
        }
    }
    
    /* Conservative estimate: minimum distance is at least min_var_degree + 1 */
    return min_var_degree + 1;
}

FSOErrorCode ldpc_soft_to_hard(const double* llr, uint8_t* bits, size_t length)
{
    FSO_CHECK_NULL(llr);
    FSO_CHECK_NULL(bits);
    
    for (size_t i = 0; i < length; i++) {
        /* LLR > 0 means bit is more likely to be 0, LLR < 0 means bit is more likely to be 1 */
        bits[i] = (llr[i] < 0.0) ? 1 : 0;
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_hard_to_soft(const uint8_t* bits, double* llr, size_t length, double snr_db)
{
    FSO_CHECK_NULL(bits);
    FSO_CHECK_NULL(llr);
    FSO_CHECK_PARAM(snr_db > 0.0);
    
    /* Convert SNR from dB to linear scale */
    double snr_linear = fso_db_to_linear(snr_db);
    
    /* For BPSK modulation over AWGN channel:
     * LLR = 2 * received_symbol * Es/N0
     * For hard decisions, we approximate based on bit value and SNR */
    double llr_magnitude = 2.0 * snr_linear;
    
    for (size_t i = 0; i < length; i++) {
        /* Positive LLR for bit 0, negative LLR for bit 1 */
        llr[i] = (bits[i] == 0) ? llr_magnitude : -llr_magnitude;
    }
    
    return FSO_SUCCESS;
}

static int gcd(int a, int b)
{
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

FSOErrorCode ldpc_init_message_graph(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(ldpc->H);
    
    /* Initialize degree arrays */
    memset(ldpc->var_degree, 0, ldpc->n * sizeof(int));
    memset(ldpc->check_degree, 0, ldpc->m * sizeof(int));
    
    /* Count degrees from parity-check matrix */
    for (int i = 0; i < ldpc->H->nnz; i++) {
        int row = ldpc->H->elements[i].row;
        int col = ldpc->H->elements[i].col;
        
        if (row < ldpc->m && col < ldpc->n) {
            ldpc->var_degree[col]++;
            ldpc->check_degree[row]++;
        }
    }
    
    /* Allocate edge lists */
    for (int i = 0; i < ldpc->n; i++) {
        if (ldpc->var_degree[i] > 0) {
            ldpc->var_to_check_edges[i] = (int*)calloc(ldpc->var_degree[i], sizeof(int));
            if (!ldpc->var_to_check_edges[i]) {
                FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate var-to-check edges for node %d", i);
                return FSO_ERROR_MEMORY;
            }
        }
    }
    
    for (int i = 0; i < ldpc->m; i++) {
        if (ldpc->check_degree[i] > 0) {
            ldpc->check_to_var_edges[i] = (int*)calloc(ldpc->check_degree[i], sizeof(int));
            if (!ldpc->check_to_var_edges[i]) {
                FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate check-to-var edges for node %d", i);
                return FSO_ERROR_MEMORY;
            }
        }
    }
    
    /* Fill edge lists */
    int* var_edge_count = (int*)calloc(ldpc->n, sizeof(int));
    int* check_edge_count = (int*)calloc(ldpc->m, sizeof(int));
    
    if (!var_edge_count || !check_edge_count) {
        FSO_LOG_ERROR(LDPC_MODULE, "Failed to allocate edge count arrays");
        if (var_edge_count) free(var_edge_count);
        if (check_edge_count) free(check_edge_count);
        return FSO_ERROR_MEMORY;
    }
    
    for (int i = 0; i < ldpc->H->nnz; i++) {
        int row = ldpc->H->elements[i].row;
        int col = ldpc->H->elements[i].col;
        
        if (row < ldpc->m && col < ldpc->n) {
            /* Add edge to variable node's list */
            if (ldpc->var_to_check_edges[col] && var_edge_count[col] < ldpc->var_degree[col]) {
                ldpc->var_to_check_edges[col][var_edge_count[col]] = row;
                var_edge_count[col]++;
            }
            
            /* Add edge to check node's list */
            if (ldpc->check_to_var_edges[row] && check_edge_count[row] < ldpc->check_degree[row]) {
                ldpc->check_to_var_edges[row][check_edge_count[row]] = col;
                check_edge_count[row]++;
            }
        }
    }
    
    free(var_edge_count);
    free(check_edge_count);
    
    FSO_LOG_DEBUG(LDPC_MODULE, "Initialized message passing graph with %d variable nodes and %d check nodes",
                 ldpc->n, ldpc->m);
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_decode(LDPCCodec* ldpc, const uint8_t* received, size_t received_len,
                         uint8_t* decoded, size_t decoded_len, int* errors_corrected)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(received);
    FSO_CHECK_NULL(decoded);
    FSO_CHECK_PARAM(received_len == (size_t)ldpc->n);
    FSO_CHECK_PARAM(decoded_len >= (size_t)ldpc->k);
    
    /* Check if message passing graph is initialized */
    if (!ldpc->var_to_check_edges || !ldpc->check_to_var_edges) {
        FSO_LOG_ERROR(LDPC_MODULE, "Message passing graph not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    /* Initialize channel LLRs from received hard bits
     * For hard decision decoding, we use a large LLR magnitude */
    const double hard_llr_magnitude = 10.0;
    for (int i = 0; i < ldpc->n; i++) {
        /* Positive LLR for bit 0, negative LLR for bit 1 */
        ldpc->channel_llr[i] = (received[i] == 0) ? hard_llr_magnitude : -hard_llr_magnitude;
    }
    
    /* Initialize variable-to-check messages with channel LLRs */
    for (int v = 0; v < ldpc->n; v++) {
        for (int e = 0; e < ldpc->var_degree[v]; e++) {
            int edge_idx = v * ldpc->m + e;
            if (edge_idx < ldpc->n * ldpc->m) {
                ldpc->variable_to_check[edge_idx] = ldpc->channel_llr[v];
            }
        }
    }
    
    /* Initialize check-to-variable messages to zero */
    memset(ldpc->check_to_variable, 0, ldpc->n * ldpc->m * sizeof(double));
    
    /* Belief propagation iterations */
    int iteration;
    int converged = 0;
    
    for (iteration = 0; iteration < ldpc->max_iterations; iteration++) {
        /* Update check-to-variable messages */
        FSOErrorCode result = ldpc_update_check_messages(ldpc);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR(LDPC_MODULE, "Failed to update check messages at iteration %d", iteration);
            return result;
        }
        
        /* Update variable-to-check messages */
        result = ldpc_update_variable_messages(ldpc);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR(LDPC_MODULE, "Failed to update variable messages at iteration %d", iteration);
            return result;
        }
        
        /* Update posterior LLRs and make hard decisions */
        result = ldpc_update_posteriors(ldpc);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR(LDPC_MODULE, "Failed to update posteriors at iteration %d", iteration);
            return result;
        }
        
        /* Check for convergence (syndrome check) */
        result = ldpc_calculate_syndrome(ldpc);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR(LDPC_MODULE, "Failed to calculate syndrome at iteration %d", iteration);
            return result;
        }
        
        converged = ldpc_check_convergence(ldpc);
        if (converged) {
            FSO_LOG_DEBUG(LDPC_MODULE, "LDPC decoder converged at iteration %d", iteration + 1);
            break;
        }
    }
    
    /* Extract decoded information bits (systematic part) */
    for (int i = 0; i < ldpc->k; i++) {
        decoded[i] = ldpc->decoded_bits[i];
    }
    
    /* Calculate number of errors corrected */
    if (errors_corrected) {
        *errors_corrected = 0;
        for (int i = 0; i < ldpc->k; i++) {
            if (decoded[i] != received[i]) {
                (*errors_corrected)++;
            }
        }
    }
    
    if (!converged) {
        FSO_LOG_WARNING(LDPC_MODULE, "LDPC decoder did not converge after %d iterations", 
                       ldpc->max_iterations);
    }
    
    FSO_LOG_DEBUG(LDPC_MODULE, "LDPC decode completed: %d iterations, converged=%d, errors_corrected=%d",
                 iteration + 1, converged, errors_corrected ? *errors_corrected : 0);
    
    return FSO_SUCCESS;
}
/* 
============================================================================
 * Belief Propagation Functions
 * ============================================================================ */

FSOErrorCode ldpc_update_check_messages(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(ldpc->check_to_var_edges);
    FSO_CHECK_NULL(ldpc->variable_to_check);
    FSO_CHECK_NULL(ldpc->check_to_variable);
    
    /* Update messages from check nodes to variable nodes
     * Using the sum-product algorithm (belief propagation)
     * 
     * For each check node c and connected variable node v:
     * m_c->v = 2 * atanh( prod_{v' in N(c)\v} tanh(m_v'->c / 2) )
     * 
     * To avoid numerical issues, we use the log-domain representation:
     * m_c->v = sign * phi( sum_{v' in N(c)\v} phi(|m_v'->c|) )
     * where phi(x) = -log(tanh(x/2)) and sign is the product of signs
     */
    
    for (int c = 0; c < ldpc->m; c++) {
        int degree = ldpc->check_degree[c];
        if (degree == 0) continue;
        
        int* connected_vars = ldpc->check_to_var_edges[c];
        
        /* For each variable node connected to this check node */
        for (int e = 0; e < degree; e++) {
            int v = connected_vars[e];
            if (v >= ldpc->n) continue;
            
            /* Compute product of tanh values from all other variable nodes */
            double product_sign = 1.0;
            double sum_phi = 0.0;
            
            for (int e2 = 0; e2 < degree; e2++) {
                if (e2 == e) continue; /* Exclude current variable node */
                
                int v2 = connected_vars[e2];
                if (v2 >= ldpc->n) continue;
                
                /* Get variable-to-check message */
                int msg_idx = v2 * ldpc->m + c;
                if (msg_idx >= ldpc->n * ldpc->m) continue;
                
                double msg = ldpc->variable_to_check[msg_idx];
                
                /* Track sign */
                if (msg < 0.0) {
                    product_sign *= -1.0;
                }
                
                /* Compute phi function: phi(x) = -log(tanh(|x|/2))
                 * For numerical stability, use approximation for small and large values */
                double abs_msg = fabs(msg);
                double phi_val;
                
                if (abs_msg < 1e-10) {
                    /* For very small values, phi(x) ≈ -log(x/2) */
                    phi_val = 10.0; /* Large value to avoid log(0) */
                } else if (abs_msg > 10.0) {
                    /* For large values, phi(x) ≈ exp(-|x|) */
                    phi_val = exp(-abs_msg);
                } else {
                    /* General case: phi(x) = -log(tanh(x/2)) */
                    double tanh_val = tanh(abs_msg / 2.0);
                    if (tanh_val > 1e-10) {
                        phi_val = -log(tanh_val);
                    } else {
                        phi_val = 10.0;
                    }
                }
                
                sum_phi += phi_val;
            }
            
            /* Compute check-to-variable message using inverse phi function
             * phi^(-1)(x) = phi(x) since phi is self-inverse */
            double msg_magnitude;
            if (sum_phi < 1e-10) {
                msg_magnitude = 10.0; /* Large LLR */
            } else if (sum_phi > 10.0) {
                msg_magnitude = exp(-sum_phi);
            } else {
                double tanh_val = exp(-sum_phi);
                if (tanh_val < 1.0 - 1e-10) {
                    msg_magnitude = 2.0 * atanh(tanh_val);
                } else {
                    msg_magnitude = 10.0;
                }
            }
            
            /* Apply sign and store message */
            int msg_idx = c * ldpc->n + v;
            if (msg_idx < ldpc->n * ldpc->m) {
                ldpc->check_to_variable[msg_idx] = product_sign * msg_magnitude;
            }
        }
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_update_variable_messages(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(ldpc->var_to_check_edges);
    FSO_CHECK_NULL(ldpc->variable_to_check);
    FSO_CHECK_NULL(ldpc->check_to_variable);
    FSO_CHECK_NULL(ldpc->channel_llr);
    
    /* Update messages from variable nodes to check nodes
     * 
     * For each variable node v and connected check node c:
     * m_v->c = m_channel_v + sum_{c' in N(v)\c} m_c'->v
     * 
     * This is simply the sum of the channel LLR and all incoming
     * check-to-variable messages except from the target check node.
     */
    
    for (int v = 0; v < ldpc->n; v++) {
        int degree = ldpc->var_degree[v];
        if (degree == 0) continue;
        
        int* connected_checks = ldpc->var_to_check_edges[v];
        
        /* For each check node connected to this variable node */
        for (int e = 0; e < degree; e++) {
            int c = connected_checks[e];
            if (c >= ldpc->m) continue;
            
            /* Sum channel LLR and all check-to-variable messages except from c */
            double msg_sum = ldpc->channel_llr[v];
            
            for (int e2 = 0; e2 < degree; e2++) {
                if (e2 == e) continue; /* Exclude current check node */
                
                int c2 = connected_checks[e2];
                if (c2 >= ldpc->m) continue;
                
                /* Get check-to-variable message */
                int msg_idx = c2 * ldpc->n + v;
                if (msg_idx < ldpc->n * ldpc->m) {
                    msg_sum += ldpc->check_to_variable[msg_idx];
                }
            }
            
            /* Store variable-to-check message */
            int msg_idx = v * ldpc->m + e;
            if (msg_idx < ldpc->n * ldpc->m) {
                ldpc->variable_to_check[msg_idx] = msg_sum;
            }
        }
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_update_posteriors(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(ldpc->var_to_check_edges);
    FSO_CHECK_NULL(ldpc->check_to_variable);
    FSO_CHECK_NULL(ldpc->channel_llr);
    FSO_CHECK_NULL(ldpc->posterior_llr);
    FSO_CHECK_NULL(ldpc->decoded_bits);
    
    /* Update posterior LLRs for each variable node
     * 
     * For each variable node v:
     * L_v = m_channel_v + sum_{c in N(v)} m_c->v
     * 
     * The posterior LLR is the sum of the channel LLR and all
     * incoming check-to-variable messages.
     */
    
    for (int v = 0; v < ldpc->n; v++) {
        /* Start with channel LLR */
        double posterior = ldpc->channel_llr[v];
        
        /* Add all check-to-variable messages */
        int degree = ldpc->var_degree[v];
        if (degree > 0 && ldpc->var_to_check_edges[v]) {
            int* connected_checks = ldpc->var_to_check_edges[v];
            
            for (int e = 0; e < degree; e++) {
                int c = connected_checks[e];
                if (c >= ldpc->m) continue;
                
                /* Get check-to-variable message */
                int msg_idx = c * ldpc->n + v;
                if (msg_idx < ldpc->n * ldpc->m) {
                    posterior += ldpc->check_to_variable[msg_idx];
                }
            }
        }
        
        /* Store posterior LLR */
        ldpc->posterior_llr[v] = posterior;
        
        /* Make hard decision: LLR < 0 means bit is more likely to be 1 */
        ldpc->decoded_bits[v] = (posterior < 0.0) ? 1 : 0;
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode ldpc_calculate_syndrome(LDPCCodec* ldpc)
{
    FSO_CHECK_NULL(ldpc);
    FSO_CHECK_NULL(ldpc->H);
    FSO_CHECK_NULL(ldpc->decoded_bits);
    FSO_CHECK_NULL(ldpc->syndrome);
    
    /* Calculate syndrome s = H * c (mod 2)
     * where c is the current decoded codeword estimate
     * 
     * If syndrome is all zeros, the codeword is valid (converged).
     */
    
    /* Initialize syndrome to zero */
    memset(ldpc->syndrome, 0, ldpc->m * sizeof(int));
    
    /* Compute syndrome using sparse matrix multiplication */
    if (ldpc->H->row_ptr && ldpc->H->col_indices) {
        /* Use CSR format for efficient computation */
        for (int row = 0; row < ldpc->m; row++) {
            int syndrome_bit = 0;
            int row_start = ldpc->H->row_ptr[row];
            int row_end = ldpc->H->row_ptr[row + 1];
            
            for (int j = row_start; j < row_end; j++) {
                int col = ldpc->H->col_indices[j];
                if (col < ldpc->n && ldpc->H->values[j] == 1) {
                    /* XOR with decoded bit (GF(2) addition) */
                    syndrome_bit ^= ldpc->decoded_bits[col];
                }
            }
            
            ldpc->syndrome[row] = syndrome_bit;
        }
    } else {
        /* Fallback to element-wise computation */
        for (int i = 0; i < ldpc->H->nnz; i++) {
            int row = ldpc->H->elements[i].row;
            int col = ldpc->H->elements[i].col;
            
            if (row < ldpc->m && col < ldpc->n && ldpc->H->elements[i].value == 1) {
                ldpc->syndrome[row] ^= ldpc->decoded_bits[col];
            }
        }
    }
    
    return FSO_SUCCESS;
}

int ldpc_check_convergence(const LDPCCodec* ldpc)
{
    if (!ldpc || !ldpc->syndrome) {
        return 0;
    }
    
    /* Check if syndrome is all zeros
     * If syndrome is zero, the decoded codeword satisfies all parity checks */
    for (int i = 0; i < ldpc->m; i++) {
        if (ldpc->syndrome[i] != 0) {
            return 0; /* Not converged */
        }
    }
    
    return 1; /* Converged - all parity checks satisfied */
}
