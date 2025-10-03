/**
 * @file fec.c
 * @brief Forward Error Correction (FEC) module implementation
 * 
 * This file implements the base FEC interface that supports multiple
 * FEC codec types including Reed-Solomon and LDPC codes.
 */

#include "fec.h"
#include "reed_solomon.h"
#include "ldpc.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define FEC_MODULE "FEC"

/* ============================================================================
 * FEC Functions
 * ============================================================================ */

FSOErrorCode fec_init(FECCodec* codec, FECType type, int data_length, 
                      int code_length, void* config)
{
    FSO_CHECK_NULL(codec);
    FSO_CHECK_PARAM(code_length > data_length && data_length > 0);
    FSO_CHECK_NULL(config);
    
    memset(codec, 0, sizeof(FECCodec));
    
    codec->type = type;
    codec->data_length = data_length;
    codec->code_length = code_length;
    codec->code_rate = fec_calculate_code_rate(data_length, code_length);
    
    FSOErrorCode result = FSO_SUCCESS;
    
    switch (type) {
        case FEC_REED_SOLOMON: {
            RSConfig* rs_config = (RSConfig*)config;
            RSCodec* rs_codec = (RSCodec*)malloc(sizeof(RSCodec));
            if (!rs_codec) {
                FSO_LOG_ERROR(FEC_MODULE, "Failed to allocate Reed-Solomon codec");
                return FSO_ERROR_MEMORY;
            }
            
            result = rs_init(rs_codec, rs_config, code_length, data_length);
            if (result != FSO_SUCCESS) {
                free(rs_codec);
                return result;
            }
            
            codec->codec_state = rs_codec;
            break;
        }
        
        case FEC_LDPC: {
            LDPCConfig* ldpc_config = (LDPCConfig*)config;
            LDPCCodec* ldpc_codec = (LDPCCodec*)malloc(sizeof(LDPCCodec));
            if (!ldpc_codec) {
                FSO_LOG_ERROR(FEC_MODULE, "Failed to allocate LDPC codec");
                return FSO_ERROR_MEMORY;
            }
            
            result = ldpc_init(ldpc_codec, ldpc_config, code_length, data_length);
            if (result != FSO_SUCCESS) {
                free(ldpc_codec);
                return result;
            }
            
            /* Generate standard LDPC matrix for the code rate */
            result = ldpc_generate_standard_matrix(ldpc_codec, codec->code_rate);
            if (result != FSO_SUCCESS) {
                ldpc_free(ldpc_codec);
                free(ldpc_codec);
                return result;
            }
            
            /* Generate generator matrix for encoding */
            result = ldpc_generate_generator_matrix(ldpc_codec);
            if (result != FSO_SUCCESS) {
                ldpc_free(ldpc_codec);
                free(ldpc_codec);
                return result;
            }
            
            codec->codec_state = ldpc_codec;
            break;
        }
        
        default:
            FSO_LOG_ERROR(FEC_MODULE, "Unsupported FEC type: %d", type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    codec->is_initialized = 1;
    
    FSO_LOG_INFO(FEC_MODULE, "FEC codec initialized: type=%s, rate=%.3f",
                fec_type_string(type), codec->code_rate);
    
    return FSO_SUCCESS;
}

FSOErrorCode fec_free(FECCodec* codec)
{
    FSO_CHECK_NULL(codec);
    
    if (!codec->is_initialized) {
        return FSO_SUCCESS;
    }
    
    FSOErrorCode result = FSO_SUCCESS;
    
    switch (codec->type) {
        case FEC_REED_SOLOMON:
            if (codec->codec_state) {
                result = rs_free((RSCodec*)codec->codec_state);
                free(codec->codec_state);
            }
            break;
            
        case FEC_LDPC:
            if (codec->codec_state) {
                result = ldpc_free((LDPCCodec*)codec->codec_state);
                free(codec->codec_state);
            }
            break;
            
        default:
            FSO_LOG_WARNING(FEC_MODULE, "Unknown FEC type during free: %d", codec->type);
            break;
    }
    
    memset(codec, 0, sizeof(FECCodec));
    FSO_LOG_DEBUG(FEC_MODULE, "FEC codec freed");
    
    return result;
}

FSOErrorCode fec_encode(FECCodec* codec, const uint8_t* data, size_t data_len,
                        uint8_t* encoded, size_t* encoded_len)
{
    FSO_CHECK_NULL(codec);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(encoded);
    FSO_CHECK_NULL(encoded_len);
    FSO_CHECK_PARAM(codec->is_initialized);
    FSO_CHECK_PARAM(data_len == (size_t)codec->data_length);
    FSO_CHECK_PARAM(*encoded_len >= (size_t)codec->code_length);
    
    FSOErrorCode result = FSO_SUCCESS;
    
    switch (codec->type) {
        case FEC_REED_SOLOMON:
            result = rs_encode((RSCodec*)codec->codec_state, data, data_len,
                              encoded, encoded_len);
            break;
            
        case FEC_LDPC:
            result = ldpc_encode((LDPCCodec*)codec->codec_state, data, data_len,
                                encoded, encoded_len);
            break;
            
        default:
            FSO_LOG_ERROR(FEC_MODULE, "Unsupported FEC type for encoding: %d", codec->type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    if (result == FSO_SUCCESS) {
        FSO_LOG_DEBUG(FEC_MODULE, "Encoded %zu bytes to %zu bytes using %s",
                     data_len, *encoded_len, fec_type_string(codec->type));
    }
    
    return result;
}

FSOErrorCode fec_decode(FECCodec* codec, const uint8_t* received, size_t received_len,
                        uint8_t* decoded, size_t* decoded_len, FECStats* stats)
{
    FSO_CHECK_NULL(codec);
    FSO_CHECK_NULL(received);
    FSO_CHECK_NULL(decoded);
    FSO_CHECK_NULL(decoded_len);
    FSO_CHECK_PARAM(codec->is_initialized);
    FSO_CHECK_PARAM(received_len == (size_t)codec->code_length);
    FSO_CHECK_PARAM(*decoded_len >= (size_t)codec->data_length);
    
    /* Initialize stats if provided */
    if (stats) {
        memset(stats, 0, sizeof(FECStats));
    }
    
    FSOErrorCode result = FSO_SUCCESS;
    int errors_corrected = 0;
    
    switch (codec->type) {
        case FEC_REED_SOLOMON:
            result = rs_decode((RSCodec*)codec->codec_state, received, received_len,
                              decoded, *decoded_len, &errors_corrected);
            if (stats) {
                stats->errors_corrected = errors_corrected;
                stats->errors_detected = errors_corrected;
                stats->uncorrectable = (result != FSO_SUCCESS);
            }
            break;
            
        case FEC_LDPC:
            result = ldpc_decode((LDPCCodec*)codec->codec_state, received, received_len,
                                decoded, *decoded_len, &errors_corrected);
            if (stats) {
                stats->errors_corrected = errors_corrected;
                stats->errors_detected = errors_corrected;
                stats->uncorrectable = (result != FSO_SUCCESS);
                /* LDPC-specific stats would be filled here */
            }
            break;
            
        default:
            FSO_LOG_ERROR(FEC_MODULE, "Unsupported FEC type for decoding: %d", codec->type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    if (result == FSO_SUCCESS) {
        *decoded_len = codec->data_length;
        FSO_LOG_DEBUG(FEC_MODULE, "Decoded %zu bytes to %zu bytes using %s, corrected %d errors",
                     received_len, *decoded_len, fec_type_string(codec->type), errors_corrected);
    }
    
    return result;
}

FSOErrorCode fec_validate_config(FECType type, int data_length, int code_length, 
                                 void* config)
{
    FSO_CHECK_PARAM(code_length > data_length && data_length > 0);
    FSO_CHECK_NULL(config);
    
    double code_rate = fec_calculate_code_rate(data_length, code_length);
    FSO_CHECK_PARAM(code_rate > 0.0 && code_rate < 1.0);
    
    switch (type) {
        case FEC_REED_SOLOMON: {
            RSConfig* rs_config = (RSConfig*)config;
            FSO_CHECK_PARAM(rs_config->symbol_size >= 3 && rs_config->symbol_size <= 16);
            FSO_CHECK_PARAM(rs_config->num_roots > 0 && rs_config->num_roots <= 255);
            FSO_CHECK_PARAM(rs_config->primitive_poly > 0);
            break;
        }
        
        case FEC_LDPC: {
            LDPCConfig* ldpc_config = (LDPCConfig*)config;
            return ldpc_validate_config(ldpc_config, code_length, data_length);
        }
        
        default:
            FSO_LOG_ERROR(FEC_MODULE, "Unsupported FEC type for validation: %d", type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    return FSO_SUCCESS;
}

FSOErrorCode fec_get_info(const FECCodec* codec, FECType* type, int* data_length,
                          int* code_length, double* code_rate)
{
    FSO_CHECK_NULL(codec);
    FSO_CHECK_PARAM(codec->is_initialized);
    
    if (type) *type = codec->type;
    if (data_length) *data_length = codec->data_length;
    if (code_length) *code_length = codec->code_length;
    if (code_rate) *code_rate = codec->code_rate;
    
    return FSO_SUCCESS;
}

double fec_calculate_code_rate(int data_length, int code_length)
{
    if (code_length <= 0) return 0.0;
    return (double)data_length / (double)code_length;
}

int fec_is_initialized(const FECCodec* codec)
{
    return (codec != NULL) && codec->is_initialized;
}

const char* fec_type_string(FECType type)
{
    switch (type) {
        case FEC_REED_SOLOMON: return "Reed-Solomon";
        case FEC_LDPC: return "LDPC";
        default: return "Unknown";
    }
}

int fec_calculate_min_code_length(FECType type, int data_length, 
                                  int error_correction_capability)
{
    switch (type) {
        case FEC_REED_SOLOMON:
            /* Reed-Solomon: n = k + 2t where t is error correction capability */
            return data_length + 2 * error_correction_capability;
            
        case FEC_LDPC:
            /* LDPC: depends on code rate, use conservative estimate */
            return (int)(data_length / 0.5); /* Assume rate 1/2 */
            
        default:
            return data_length * 2; /* Conservative default */
    }
}

/* ============================================================================
 * Interleaving Functions
 * ============================================================================ */

FSOErrorCode interleaver_init(InterleaverConfig* config, int block_size, int depth)
{
    FSO_CHECK_NULL(config);
    FSO_CHECK_PARAM(block_size > 0);
    FSO_CHECK_PARAM(depth > 0);
    
    memset(config, 0, sizeof(InterleaverConfig));
    
    config->block_size = block_size;
    config->depth = depth;
    
    /* Allocate permutation table (not used for block interleaver, but reserved for future) */
    config->permutation_table = NULL;
    
    FSO_LOG_INFO(FEC_MODULE, "Interleaver initialized: block_size=%d, depth=%d",
                block_size, depth);
    
    return FSO_SUCCESS;
}

FSOErrorCode interleaver_free(InterleaverConfig* config)
{
    FSO_CHECK_NULL(config);
    
    if (config->permutation_table) {
        free(config->permutation_table);
        config->permutation_table = NULL;
    }
    
    memset(config, 0, sizeof(InterleaverConfig));
    FSO_LOG_DEBUG(FEC_MODULE, "Interleaver freed");
    
    return FSO_SUCCESS;
}

FSOErrorCode interleave(const InterleaverConfig* config, const uint8_t* input,
                        size_t input_len, uint8_t* output, size_t output_len)
{
    FSO_CHECK_NULL(config);
    FSO_CHECK_NULL(input);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(config->block_size > 0);
    FSO_CHECK_PARAM(config->depth > 0);
    FSO_CHECK_PARAM(output_len >= input_len);
    
    /* Block interleaver: write data row-wise, read column-wise
     * 
     * Example with block_size=4, depth=3:
     * Input:  [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
     * 
     * Write row-wise into matrix (depth rows x block_size columns):
     *   0  1  2  3
     *   4  5  6  7
     *   8  9 10 11
     * 
     * Read column-wise:
     * Output: [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]
     * 
     * This distributes burst errors across multiple codewords.
     */
    
    int total_size = config->block_size * config->depth;
    size_t full_blocks = input_len / total_size;
    size_t remaining = input_len % total_size;
    
    size_t out_idx = 0;
    size_t in_idx = 0;
    
    /* Process full blocks */
    for (size_t block = 0; block < full_blocks; block++) {
        /* Read column-wise from the conceptual matrix */
        for (int col = 0; col < config->block_size; col++) {
            for (int row = 0; row < config->depth; row++) {
                /* Calculate input position: block_start + row * block_size + col */
                size_t src_idx = in_idx + row * config->block_size + col;
                output[out_idx++] = input[src_idx];
            }
        }
        in_idx += total_size;
    }
    
    /* Handle remaining bytes (partial block) - copy directly without interleaving */
    if (remaining > 0) {
        for (size_t i = 0; i < remaining; i++) {
            output[out_idx++] = input[in_idx + i];
        }
    }
    
    FSO_LOG_DEBUG(FEC_MODULE, "Interleaved %zu bytes", input_len);
    
    return FSO_SUCCESS;
}

FSOErrorCode deinterleave(const InterleaverConfig* config, const uint8_t* input,
                          size_t input_len, uint8_t* output, size_t output_len)
{
    FSO_CHECK_NULL(config);
    FSO_CHECK_NULL(input);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(config->block_size > 0);
    FSO_CHECK_PARAM(config->depth > 0);
    FSO_CHECK_PARAM(output_len >= input_len);
    
    /* Block deinterleaver: reverse of interleaver
     * Read column-wise (as written by interleaver), write row-wise
     * 
     * Example with block_size=4, depth=3:
     * Input:  [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]
     * 
     * Read column-wise into matrix:
     *   0  1  2  3
     *   4  5  6  7
     *   8  9 10 11
     * 
     * Write row-wise:
     * Output: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
     */
    
    int total_size = config->block_size * config->depth;
    size_t full_blocks = input_len / total_size;
    size_t remaining = input_len % total_size;
    
    size_t in_idx = 0;
    size_t out_idx = 0;
    
    /* Process full blocks */
    for (size_t block = 0; block < full_blocks; block++) {
        /* Write row-wise to the output (reverse of column-wise reading) */
        for (int col = 0; col < config->block_size; col++) {
            for (int row = 0; row < config->depth; row++) {
                /* Calculate output position: block_start + row * block_size + col */
                size_t dst_idx = out_idx + row * config->block_size + col;
                output[dst_idx] = input[in_idx++];
            }
        }
        out_idx += total_size;
    }
    
    /* Handle remaining bytes (partial block) - copy directly without deinterleaving */
    if (remaining > 0) {
        for (size_t i = 0; i < remaining; i++) {
            output[out_idx + i] = input[in_idx++];
        }
    }
    
    FSO_LOG_DEBUG(FEC_MODULE, "Deinterleaved %zu bytes", input_len);
    
    return FSO_SUCCESS;
}