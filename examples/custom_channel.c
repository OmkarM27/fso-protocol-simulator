/**
 * @file custom_channel.c
 * @brief Custom channel model example
 * 
 * This example demonstrates how to create and use custom atmospheric channel
 * models with different weather conditions and parameters.
 * 
 * Compile:
 *   gcc -I../src custom_channel.c -L../build -lfso -lm -lfftw3 -fopenmp -o custom_channel
 * 
 * Run:
 *   ./custom_channel
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fso.h"
#include "turbulence/channel.h"

#define NUM_SAMPLES 1000
#define TRANSMIT_POWER 0.1  // 100 mW

void print_channel_info(const ChannelModel* channel) {
    char info_buffer[512];
    channel_get_info(channel, info_buffer, sizeof(info_buffer));
    printf("%s\n", info_buffer);
}

void simulate_channel_scenario(const char* scenario_name, 
                               ChannelModel* channel,
                               double transmit_power,
                               int num_samples) {
    printf("--- %s ---\n\n", scenario_name);
    
    print_channel_info(channel);
    
    // Simulate transmission
    double received_powers[NUM_SAMPLES];
    double min_power = 1e10;
    double max_power = 0.0;
    double sum_power = 0.0;
    
    printf("\nSimulating %d samples...\n", num_samples);
    
    for (int i = 0; i < num_samples; i++) {
        double time_step = 1e-3;  // 1 ms between samples
        double noise_power = 1e-9;  // 1 nW
        
        double rx_power = channel_apply_effects(channel, transmit_power,
                                                noise_power, time_step);
        received_powers[i] = rx_power;
        
        if (rx_power < min_power) min_power = rx_power;
        if (rx_power > max_power) max_power = rx_power;
        sum_power += rx_power;
    }
    
    double avg_power = sum_power / num_samples;
    
    // Calculate statistics
    double variance = 0.0;
    for (int i = 0; i < num_samples; i++) {
        double diff = received_powers[i] - avg_power;
        variance += diff * diff;
    }
    variance /= num_samples;
    double stddev = sqrt(variance);
    
    // Calculate fade depth (dB)
    double fade_depth_db = fso_watts_to_dbm(max_power) - fso_watts_to_dbm(min_power);
    
    // Calculate average SNR
    double avg_snr = avg_power / 1e-9;  // Noise power = 1 nW
    double avg_snr_db = fso_linear_to_db(avg_snr);
    
    printf("\nResults:\n");
    printf("  Transmit power: %.1f mW (%.1f dBm)\n", 
           transmit_power * 1000.0, fso_watts_to_dbm(transmit_power));
    printf("  Average received power: %.3e W (%.1f dBm)\n",
           avg_power, fso_watts_to_dbm(avg_power));
    printf("  Min received power: %.3e W (%.1f dBm)\n",
           min_power, fso_watts_to_dbm(min_power));
    printf("  Max received power: %.3e W (%.1f dBm)\n",
           max_power, fso_watts_to_dbm(max_power));
    printf("  Standard deviation: %.3e W\n", stddev);
    printf("  Fade depth: %.1f dB\n", fade_depth_db);
    printf("  Average SNR: %.1f dB\n", avg_snr_db);
    
    // Calculate path loss
    double path_loss_db = fso_watts_to_dbm(transmit_power) - fso_watts_to_dbm(avg_power);
    printf("  Total path loss: %.1f dB\n", path_loss_db);
    
    printf("\n");
}

int main(void) {
    printf("=== Custom Channel Model Example ===\n\n");
    
    // Set log level
    fso_set_log_level(LOG_INFO);
    
    // Initialize random number generator
    fso_random_init(0);
    
    // ========================================================================
    // Scenario 1: Clear Weather, Short Distance
    // ========================================================================
    ChannelModel channel1;
    if (channel_init(&channel1, 500.0, 1.55e-6, WEATHER_CLEAR) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize channel 1\n");
        return 1;
    }
    
    simulate_channel_scenario("Scenario 1: Clear Weather, 500m",
                             &channel1, TRANSMIT_POWER, NUM_SAMPLES);
    
    channel_free(&channel1);
    
    // ========================================================================
    // Scenario 2: Foggy Conditions
    // ========================================================================
    ChannelModel channel2;
    if (channel_init(&channel2, 1000.0, 1.55e-6, WEATHER_FOG) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize channel 2\n");
        return 1;
    }
    
    // Set fog parameters (visibility = 200m)
    channel_set_weather_params(&channel2, 200.0, 0.0, 0.0);
    channel_update_calculations(&channel2);
    
    simulate_channel_scenario("Scenario 2: Fog (200m visibility), 1km",
                             &channel2, TRANSMIT_POWER, NUM_SAMPLES);
    
    channel_free(&channel2);
    
    // ========================================================================
    // Scenario 3: Rainy Conditions
    // ========================================================================
    ChannelModel channel3;
    if (channel_init(&channel3, 1000.0, 1.55e-6, WEATHER_RAIN) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize channel 3\n");
        return 1;
    }
    
    // Set rain parameters (25 mm/hr - heavy rain)
    channel_set_weather_params(&channel3, 1000.0, 25.0, 0.0);
    channel_update_calculations(&channel3);
    
    simulate_channel_scenario("Scenario 3: Heavy Rain (25 mm/hr), 1km",
                             &channel3, TRANSMIT_POWER, NUM_SAMPLES);
    
    channel_free(&channel3);
    
    // ========================================================================
    // Scenario 4: High Turbulence
    // ========================================================================
    ChannelModel channel4;
    if (channel_init_extended(&channel4, 2000.0, 1.55e-6, 
                             WEATHER_HIGH_TURBULENCE,
                             1e-13,  // Strong turbulence
                             5e-3) != FSO_SUCCESS) {  // 5ms correlation time
        fprintf(stderr, "Failed to initialize channel 4\n");
        return 1;
    }
    
    simulate_channel_scenario("Scenario 4: High Turbulence, 2km",
                             &channel4, TRANSMIT_POWER, NUM_SAMPLES);
    
    channel_free(&channel4);
    
    // ========================================================================
    // Scenario 5: Long Distance, Clear
    // ========================================================================
    ChannelModel channel5;
    if (channel_init(&channel5, 5000.0, 1.55e-6, WEATHER_CLEAR) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize channel 5\n");
        return 1;
    }
    
    simulate_channel_scenario("Scenario 5: Long Distance (5km), Clear",
                             &channel5, TRANSMIT_POWER, NUM_SAMPLES);
    
    channel_free(&channel5);
    
    // ========================================================================
    // Comparison Table
    // ========================================================================
    printf("=== Comparison Table ===\n\n");
    
    printf("Scenario                    | Distance | Weather      | Avg SNR | Fade Depth\n");
    printf("----------------------------|----------|--------------|---------|------------\n");
    printf("1. Clear, Short             |   500 m  | Clear        |  High   |    Low\n");
    printf("2. Fog                      |  1000 m  | Fog (200m)   |  Low    |  Medium\n");
    printf("3. Heavy Rain               |  1000 m  | Rain (25mm/h)|  Low    |  Medium\n");
    printf("4. High Turbulence          |  2000 m  | Turbulent    | Medium  |   High\n");
    printf("5. Long Distance            |  5000 m  | Clear        |  Low    |  Medium\n");
    
    // ========================================================================
    // Analysis and Recommendations
    // ========================================================================
    printf("\n=== Analysis and Recommendations ===\n\n");
    
    printf("Weather Impact:\n");
    printf("  - Fog: Most severe attenuation (visibility-dependent)\n");
    printf("  - Rain: Moderate attenuation (rate-dependent)\n");
    printf("  - Snow: Similar to rain but typically less severe\n");
    printf("  - Clear: Minimal attenuation, mainly path loss\n\n");
    
    printf("Turbulence Impact:\n");
    printf("  - Causes signal fading (scintillation)\n");
    printf("  - Stronger at longer distances\n");
    printf("  - Time-varying (requires adaptive systems)\n");
    printf("  - Can be mitigated with aperture averaging\n\n");
    
    printf("Distance Impact:\n");
    printf("  - Path loss increases with distance (20*log10(d))\n");
    printf("  - Turbulence effects increase with distance\n");
    printf("  - Beam divergence causes geometric loss\n");
    printf("  - Practical limit: ~5-10 km for terrestrial links\n\n");
    
    printf("System Design Recommendations:\n\n");
    
    printf("For Short Distance (<1 km):\n");
    printf("  - Simple modulation (OOK) sufficient\n");
    printf("  - Minimal FEC required\n");
    printf("  - Basic beam tracking\n");
    printf("  - High availability even in bad weather\n\n");
    
    printf("For Medium Distance (1-3 km):\n");
    printf("  - Use PPM for power efficiency\n");
    printf("  - Reed-Solomon FEC recommended\n");
    printf("  - Active beam tracking required\n");
    printf("  - Weather-dependent availability\n\n");
    
    printf("For Long Distance (>3 km):\n");
    printf("  - High-order PPM or DPSK\n");
    printf("  - LDPC FEC for best performance\n");
    printf("  - Sophisticated beam tracking (PID control)\n");
    printf("  - Backup link recommended for critical applications\n");
    printf("  - Consider adaptive modulation and coding\n\n");
    
    printf("Turbulence Mitigation:\n");
    printf("  - Use larger receiver aperture (aperture averaging)\n");
    printf("  - Implement adaptive optics if possible\n");
    printf("  - Use diversity techniques (spatial, temporal)\n");
    printf("  - Strong FEC to handle fading-induced errors\n\n");
    
    printf("Weather Mitigation:\n");
    printf("  - Increase transmit power (within eye safety limits)\n");
    printf("  - Use longer wavelengths (less fog attenuation)\n");
    printf("  - Implement hybrid RF/FSO systems\n");
    printf("  - Site selection: avoid fog-prone areas\n\n");
    
    printf("=== Example Complete ===\n");
    
    return 0;
}
