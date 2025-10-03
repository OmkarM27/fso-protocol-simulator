/**
 * @file simulator.c
 * @brief Main simulation loop implementation
 * 
 * Implements the complete Hardware-in-Loop simulator with end-to-end
 * link simulation including modulation, FEC, channel effects, and metrics collection.
 */

#include "simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Generate random data packet
 */
static void generate_random_packet(uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        data[i] = (uint8_t)fso_random_int(0, 255);
    }
}

/**
 * @brief Count bit errors between two data buffers
 */
static int count_bit_errors(const uint8_t* data1, const uint8_t* data2, size_t length) {
    int errors = 0;
    for (size_t i = 0; i < length; i++) {
        uint8_t diff = data1[i] ^ data2[i];
        // Count set bits in diff
        while (diff) {
            errors += diff & 1;
            diff >>= 1;
        }
    }
    return errors;
}

/**
 * @brief Add AWGN to signal
 */
static void add_awgn(double* signal, size_t length, double noise_power) {
    double noise_stddev = sqrt(noise_power);
    for (size_t i = 0; i < length; i++) {
        signal[i] += fso_random_gaussian(0.0, noise_stddev);
    }
}

/**
 * @brief Calculate required buffer sizes
 */
static int calculate_buffer_sizes(const SimConfig* config, 
                                  size_t* max_symbols,
                                  size_t* max_encoded) {
    // Calculate maximum symbols needed
    int bits_per_symbol = 1;  // Default for OOK
    
    if (config->system.modulation == MOD_PPM) {
        // PPM: log2(order) bits per symbol, but order slots per symbol
        int log2_order = 0;
        int order = config->system.ppm_order;
        while (order > 1) {
            log2_order++;
            order >>= 1;
        }
        bits_per_symbol = log2_order;
        *max_symbols = (config->control.packet_size * 8 / bits_per_symbol) * config->system.ppm_order;
    } else if (config->system.modulation == MOD_DPSK) {
        bits_per_symbol = 1;
        *max_symbols = config->control.packet_size * 8;
    } else {  // OOK
        bits_per_symbol = 1;
        *max_symbols = config->control.packet_size * 8;
    }
    
    // Add margin for FEC overhead
    *max_encoded = (size_t)((double)config->control.packet_size / config->system.code_rate) + 256;
    *max_symbols = (*max_symbols * 2) + 1024;  // Extra margin
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Main Simulation Function
 * ============================================================================ */

/**
 * @brief Run complete FSO link simulation
 * 
 * Implements the main simulation loop:
 * 1. Generate random data packets
 * 2. Apply FEC encoding
 * 3. Modulate data to optical symbols
 * 4. Apply channel effects (fading, attenuation, noise)
 * 5. Demodulate received signal
 * 6. Apply FEC decoding
 * 7. Compare with original data and collect metrics
 * 
 * @param config Simulation configuration
 * @param results Output results structure
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Requirement 6.1: Complete transmitter and receiver chain
 * @note Requirement 6.3: Channel propagation with atmospheric effects
 */
int sim_run(const SimConfig* config, SimResults* results) {
    if (config == NULL || results == NULL) {
        FSO_LOG_ERROR("Simulator", "NULL pointer in sim_run");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Validate configuration
    int result = sim_config_validate(config);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("Simulator", "Configuration validation failed");
        return result;
    }
    
    FSO_LOG_INFO("Simulator", "Starting simulation with %d packets", 
                 config->control.num_packets);
    
    // Initialize random number generator
    if (config->control.random_seed == 0) {
        fso_random_init((unsigned int)time(NULL));
    } else {
        fso_random_init(config->control.random_seed);
    }
    
    // Initialize results
    result = sim_results_init(results, config->control.num_packets * 10, 
                             config->control.num_packets);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("Simulator", "Failed to initialize results");
        return result;
    }
    
    results->start_time = (double)clock() / CLOCKS_PER_SEC;
    
    // Initialize modulator
    Modulator modulator;
    if (config->system.modulation == MOD_PPM) {
        result = modulator_init_ppm(&modulator, config->control.sample_rate, 
                                   config->system.ppm_order);
    } else {
        result = modulator_init(&modulator, config->system.modulation, 
                               config->control.sample_rate);
    }
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("Simulator", "Failed to initialize modulator");
        sim_results_free(results);
        return result;
    }
    
    // Initialize FEC codec
    FECCodec fec_codec;
    int data_len = config->control.packet_size;
    int code_len = (int)((double)data_len / config->system.code_rate);
    
    // For Reed-Solomon, use default configuration
    RSConfig rs_config = {
        .symbol_size = 8,
        .num_roots = code_len - data_len,
        .first_root = 1,
        .primitive_poly = 0x11d,  // x^8 + x^4 + x^3 + x^2 + 1
        .fcr = 1
    };
    
    result = fec_init(&fec_codec, config->system.fec_type, data_len, code_len, &rs_config);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("Simulator", "Failed to initialize FEC codec");
        modulator_free(&modulator);
        sim_results_free(results);
        return result;
    }
    
    // Initialize interleaver if enabled
    InterleaverConfig interleaver;
    if (config->system.use_interleaver) {
        result = interleaver_init(&interleaver, code_len, config->system.interleaver_depth);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("Simulator", "Failed to initialize interleaver");
            fec_free(&fec_codec);
            modulator_free(&modulator);
            sim_results_free(results);
            return result;
        }
    }
    
    // Initialize channel model
    ChannelModel channel;
    result = channel_init_extended(&channel, 
                                   config->link.link_distance,
                                   config->link.wavelength,
                                   config->environment.weather,
                                   config->environment.turbulence_strength,
                                   config->environment.correlation_time);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("Simulator", "Failed to initialize channel model");
        if (config->system.use_interleaver) interleaver_free(&interleaver);
        fec_free(&fec_codec);
        modulator_free(&modulator);
        sim_results_free(results);
        return result;
    }
    
    // Set additional channel parameters
    channel_set_atmospheric_params(&channel, config->environment.temperature,
                                   config->environment.humidity);
    channel_set_weather_params(&channel, config->environment.visibility,
                              config->environment.rainfall_rate,
                              config->environment.snowfall_rate);
    channel_set_beam_divergence(&channel, config->link.beam_divergence);
    channel_update_calculations(&channel);
    
    // Calculate buffer sizes
    size_t max_symbols, max_encoded;
    calculate_buffer_sizes(config, &max_symbols, &max_encoded);
    
    // Allocate buffers
    uint8_t* tx_data = (uint8_t*)malloc(config->control.packet_size);
    uint8_t* encoded_data = (uint8_t*)malloc(max_encoded);
    uint8_t* interleaved_data = (uint8_t*)malloc(max_encoded);
    double* tx_symbols = (double*)malloc(max_symbols * sizeof(double));
    double* rx_symbols = (double*)malloc(max_symbols * sizeof(double));
    uint8_t* deinterleaved_data = (uint8_t*)malloc(max_encoded);
    uint8_t* decoded_data = (uint8_t*)malloc(config->control.packet_size);
    
    if (!tx_data || !encoded_data || !interleaved_data || !tx_symbols || 
        !rx_symbols || !deinterleaved_data || !decoded_data) {
        FSO_LOG_ERROR("Simulator", "Failed to allocate buffers");
        free(tx_data); free(encoded_data); free(interleaved_data);
        free(tx_symbols); free(rx_symbols); free(deinterleaved_data); free(decoded_data);
        channel_free(&channel);
        if (config->system.use_interleaver) interleaver_free(&interleaver);
        fec_free(&fec_codec);
        modulator_free(&modulator);
        sim_results_free(results);
        return FSO_ERROR_MEMORY;
    }
    
    // Main simulation loop
    double time_per_packet = config->control.simulation_time / config->control.num_packets;
    
    for (int packet_id = 0; packet_id < config->control.num_packets; packet_id++) {
        double current_time = packet_id * time_per_packet;
        
        if (config->control.verbose && (packet_id % 10 == 0)) {
            printf("Processing packet %d/%d (%.1f%%)\n", 
                   packet_id + 1, config->control.num_packets,
                   100.0 * (packet_id + 1) / config->control.num_packets);
        }
        
        // Step 1: Generate random data packet
        generate_random_packet(tx_data, config->control.packet_size);
        
        // Step 2: Apply FEC encoding
        size_t encoded_len;
        FECStats fec_stats = {0};
        result = fec_encode(&fec_codec, tx_data, config->control.packet_size,
                           encoded_data, &encoded_len);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("Simulator", "FEC encoding failed for packet %d", packet_id);
            continue;
        }
        
        // Step 2b: Apply interleaving if enabled
        uint8_t* modulation_input = encoded_data;
        size_t modulation_input_len = encoded_len;
        
        if (config->system.use_interleaver) {
            result = interleave(&interleaver, encoded_data, encoded_len,
                               interleaved_data, max_encoded);
            if (result == FSO_SUCCESS) {
                modulation_input = interleaved_data;
            }
        }
        
        // Step 3: Modulate data to optical symbols
        size_t symbol_len;
        result = modulate(&modulator, modulation_input, modulation_input_len,
                         tx_symbols, &symbol_len);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("Simulator", "Modulation failed for packet %d", packet_id);
            continue;
        }
        
        // Step 4: Apply channel effects
        double signal_power = fso_signal_power_real(tx_symbols, symbol_len);
        double tx_power = config->link.transmit_power * signal_power;
        
        // Apply channel fading and attenuation
        double rx_power = channel_apply_effects(&channel, tx_power,
                                               config->control.noise_floor,
                                               time_per_packet);
        
        // Scale received symbols
        double channel_gain = sqrt(rx_power / tx_power);
        for (size_t i = 0; i < symbol_len; i++) {
            rx_symbols[i] = tx_symbols[i] * channel_gain;
        }
        
        // Add AWGN
        add_awgn(rx_symbols, symbol_len, config->control.noise_floor);
        
        // Calculate SNR
        double rx_signal_power = fso_signal_power_real(rx_symbols, symbol_len);
        double snr_linear = rx_power / config->control.noise_floor;
        double snr_db = fso_linear_to_db(snr_linear);
        
        // Step 5: Demodulate received signal
        size_t demod_len;
        result = demodulate(&modulator, rx_symbols, symbol_len,
                           deinterleaved_data, &demod_len, snr_db);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("Simulator", "Demodulation failed for packet %d", packet_id);
            continue;
        }
        
        // Step 5b: Apply deinterleaving if enabled
        uint8_t* fec_input = deinterleaved_data;
        size_t fec_input_len = demod_len;
        
        if (config->system.use_interleaver) {
            result = deinterleave(&interleaver, deinterleaved_data, demod_len,
                                 encoded_data, max_encoded);
            if (result == FSO_SUCCESS) {
                fec_input = encoded_data;
            }
        }
        
        // Step 6: Apply FEC decoding
        size_t decoded_len;
        result = fec_decode(&fec_codec, fec_input, fec_input_len,
                           decoded_data, &decoded_len, &fec_stats);
        
        // Step 7: Compare with original data and collect metrics
        int bit_errors = count_bit_errors(tx_data, decoded_data, 
                                         FSO_MIN(config->control.packet_size, decoded_len));
        int total_bits = config->control.packet_size * 8;
        double ber = (double)bit_errors / (double)total_bits;
        
        // Record packet statistics
        PacketStats packet_stats = {
            .packet_id = packet_id,
            .bits_transmitted = total_bits,
            .bits_received = decoded_len * 8,
            .bit_errors = bit_errors,
            .ber = ber,
            .snr_db = snr_db,
            .received_power = rx_power,
            .fec_corrected_errors = fec_stats.errors_corrected,
            .fec_uncorrectable = fec_stats.uncorrectable
        };
        sim_results_add_packet(results, &packet_stats);
        
        // Record time-series point
        TimeSeriesPoint ts_point = {
            .timestamp = current_time,
            .ber = ber,
            .snr_db = snr_db,
            .received_power = rx_power,
            .throughput = (double)total_bits / time_per_packet,
            .beam_azimuth = 0.0,
            .beam_elevation = 0.0,
            .signal_strength = channel_gain
        };
        sim_results_add_point(results, &ts_point);
    }
    
    // Calculate final metrics
    sim_results_calculate_metrics(results);
    
    results->end_time = (double)clock() / CLOCKS_PER_SEC;
    results->simulation_duration = results->end_time - results->start_time;
    
    // Cleanup
    free(tx_data);
    free(encoded_data);
    free(interleaved_data);
    free(tx_symbols);
    free(rx_symbols);
    free(deinterleaved_data);
    free(decoded_data);
    
    channel_free(&channel);
    if (config->system.use_interleaver) {
        interleaver_free(&interleaver);
    }
    fec_free(&fec_codec);
    modulator_free(&modulator);
    
    FSO_LOG_INFO("Simulator", "Simulation completed: %d packets, BER=%.3e, SNR=%.2f dB",
                 results->total_packets, results->avg_ber, results->avg_snr);
    
    return FSO_SUCCESS;
}
