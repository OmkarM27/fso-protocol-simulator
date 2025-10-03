/**
 * @file modulation.c
 * @brief Implementation of optical modulation schemes
 */

#include "modulation/modulation.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MODULE_NAME "MODULATION"

/* ============================================================================
 * Initialization and Cleanup Functions
 * ============================================================================ */

int modulator_init(Modulator* mod, ModulationType type, double symbol_rate) {
    FSO_CHECK_NULL(mod);
    FSO_CHECK_PARAM(symbol_rate > 0.0);
    
    memset(mod, 0, sizeof(Modulator));
    
    mod->type = type;
    mod->symbol_rate = symbol_rate;
    
    switch (type) {
        case MOD_OOK:
            mod->bits_per_symbol = 1;
            FSO_LOG_INFO(MODULE_NAME, "Initialized OOK modulator at %.2f symbols/s", 
                        symbol_rate);
            break;
            
        case MOD_PPM:
            // Default to 4-PPM (2 bits per symbol)
            mod->bits_per_symbol = 2;
            mod->config.ppm.order = 4;
            mod->config.ppm.slots_per_symbol = 4;
            FSO_LOG_INFO(MODULE_NAME, "Initialized 4-PPM modulator at %.2f symbols/s", 
                        symbol_rate);
            break;
            
        case MOD_DPSK:
            mod->bits_per_symbol = 1;
            mod->config.dpsk.last_phase = 0.0;
            mod->config.dpsk.initialized = 0;
            FSO_LOG_INFO(MODULE_NAME, "Initialized DPSK modulator at %.2f symbols/s", 
                        symbol_rate);
            break;
            
        default:
            FSO_LOG_ERROR(MODULE_NAME, "Unsupported modulation type: %d", type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    mod->initialized = 1;
    return FSO_SUCCESS;
}

int modulator_init_ppm(Modulator* mod, double symbol_rate, int ppm_order) {
    FSO_CHECK_NULL(mod);
    FSO_CHECK_PARAM(symbol_rate > 0.0);
    FSO_CHECK_PARAM(ppm_order == 2 || ppm_order == 4 || 
                    ppm_order == 8 || ppm_order == 16);
    
    memset(mod, 0, sizeof(Modulator));
    
    mod->type = MOD_PPM;
    mod->symbol_rate = symbol_rate;
    mod->config.ppm.order = ppm_order;
    mod->config.ppm.slots_per_symbol = ppm_order;
    
    // Calculate bits per symbol: log2(order)
    if (ppm_order == 2) {
        mod->bits_per_symbol = 1;
    } else if (ppm_order == 4) {
        mod->bits_per_symbol = 2;
    } else if (ppm_order == 8) {
        mod->bits_per_symbol = 3;
    } else if (ppm_order == 16) {
        mod->bits_per_symbol = 4;
    }
    
    mod->initialized = 1;
    
    FSO_LOG_INFO(MODULE_NAME, "Initialized %d-PPM modulator at %.2f symbols/s (%d bits/symbol)",
                ppm_order, symbol_rate, mod->bits_per_symbol);
    
    return FSO_SUCCESS;
}

void modulator_free(Modulator* mod) {
    if (mod != NULL) {
        FSO_LOG_DEBUG(MODULE_NAME, "Freeing modulator");
        memset(mod, 0, sizeof(Modulator));
    }
}

/* ============================================================================
 * Generic Modulation Functions
 * ============================================================================ */

int modulate(Modulator* mod, const uint8_t* data, size_t data_len,
             double* symbols, size_t* symbol_len) {
    FSO_CHECK_NULL(mod);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(symbol_len);
    FSO_CHECK_PARAM(mod->initialized);
    FSO_CHECK_PARAM(data_len > 0);
    
    int result;
    
    switch (mod->type) {
        case MOD_OOK:
            result = ook_modulate(data, data_len, symbols, symbol_len);
            break;
            
        case MOD_PPM:
            result = ppm_modulate(data, data_len, symbols, symbol_len, 
                                 mod->config.ppm.order);
            break;
            
        case MOD_DPSK:
            // DPSK uses complex symbols, so this is a special case
            FSO_LOG_ERROR(MODULE_NAME, 
                         "Use dpsk_modulate() for DPSK (requires complex symbols)");
            return FSO_ERROR_UNSUPPORTED;
            
        default:
            FSO_LOG_ERROR(MODULE_NAME, "Unsupported modulation type: %d", mod->type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    return result;
}

int demodulate(Modulator* mod, const double* symbols, size_t symbol_len,
               uint8_t* data, size_t* data_len, double snr) {
    FSO_CHECK_NULL(mod);
    FSO_CHECK_NULL(symbols);
    FSO_CHECK_NULL(data);
    FSO_CHECK_NULL(data_len);
    FSO_CHECK_PARAM(mod->initialized);
    FSO_CHECK_PARAM(symbol_len > 0);
    
    int result;
    
    switch (mod->type) {
        case MOD_OOK:
            result = ook_demodulate(symbols, symbol_len, data, data_len, snr);
            break;
            
        case MOD_PPM:
            result = ppm_demodulate(symbols, symbol_len, data, data_len,
                                   mod->config.ppm.order);
            break;
            
        case MOD_DPSK:
            // DPSK uses complex symbols, so this is a special case
            FSO_LOG_ERROR(MODULE_NAME, 
                         "Use dpsk_demodulate() for DPSK (requires complex symbols)");
            return FSO_ERROR_UNSUPPORTED;
            
        default:
            FSO_LOG_ERROR(MODULE_NAME, "Unsupported modulation type: %d", mod->type);
            return FSO_ERROR_UNSUPPORTED;
    }
    
    return result;
}
