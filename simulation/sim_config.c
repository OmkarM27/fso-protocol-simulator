/**
 * @file sim_config.c
 * @brief Simulator configuration management implementation
 */

#include "simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Default Configuration Values
 * ============================================================================ */

#define DEFAULT_LINK_DISTANCE 1000.0        // 1 km
#define DEFAULT_TRANSMIT_POWER 0.001        // 1 mW
#define DEFAULT_RECEIVER_SENSITIVITY 1e-9   // 1 nW
#define DEFAULT_WAVELENGTH 1.55e-6          // 1550 nm
#define DEFAULT_BEAM_DIVERGENCE 0.001       // 1 mrad
#define DEFAULT_RECEIVER_APERTURE 0.1       // 10 cm

#define DEFAULT_WEATHER WEATHER_CLEAR
#define DEFAULT_TURBULENCE_STRENGTH 1e-14
#define DEFAULT_TEMPERATURE 20.0
#define DEFAULT_HUMIDITY 0.5
#define DEFAULT_VISIBILITY 10000.0
#define DEFAULT_CORRELATION_TIME 0.001      // 1 ms

#define DEFAULT_MODULATION MOD_OOK
#define DEFAULT_PPM_ORDER 4
#define DEFAULT_FEC_TYPE FEC_REED_SOLOMON
#define DEFAULT_CODE_RATE 0.8
#define DEFAULT_INTERLEAVER_DEPTH 10

#define DEFAULT_SIMULATION_TIME 1.0         // 1 second
#define DEFAULT_SAMPLE_RATE 1e6             // 1 MHz
#define DEFAULT_PACKET_SIZE 1024            // 1 KB
#define DEFAULT_NUM_PACKETS 100
#define DEFAULT_NOISE_FLOOR 1e-12           // 1 pW

/* ============================================================================
 * Configuration Initialization
 * ============================================================================ */

int sim_config_init_defaults(SimConfig* config) {
    if (config == NULL) {
        FSO_LOG_ERROR("SimConfig", "NULL config pointer");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Link configuration
    config->link.link_distance = DEFAULT_LINK_DISTANCE;
    config->link.transmit_power = DEFAULT_TRANSMIT_POWER;
    config->link.receiver_sensitivity = DEFAULT_RECEIVER_SENSITIVITY;
    config->link.wavelength = DEFAULT_WAVELENGTH;
    config->link.beam_divergence = DEFAULT_BEAM_DIVERGENCE;
    config->link.receiver_aperture = DEFAULT_RECEIVER_APERTURE;
    
    // Environment configuration
    config->environment.weather = DEFAULT_WEATHER;
    config->environment.turbulence_strength = DEFAULT_TURBULENCE_STRENGTH;
    config->environment.temperature = DEFAULT_TEMPERATURE;
    config->environment.humidity = DEFAULT_HUMIDITY;
    config->environment.visibility = DEFAULT_VISIBILITY;
    config->environment.rainfall_rate = 0.0;
    config->environment.snowfall_rate = 0.0;
    config->environment.correlation_time = DEFAULT_CORRELATION_TIME;
    
    // System configuration
    config->system.modulation = DEFAULT_MODULATION;
    config->system.ppm_order = DEFAULT_PPM_ORDER;
    config->system.fec_type = DEFAULT_FEC_TYPE;
    config->system.code_rate = DEFAULT_CODE_RATE;
    config->system.use_interleaver = 1;
    config->system.interleaver_depth = DEFAULT_INTERLEAVER_DEPTH;
    config->system.enable_tracking = 0;
    config->system.tracking_update_rate = 100.0;
    
    // Simulation control
    config->control.simulation_time = DEFAULT_SIMULATION_TIME;
    config->control.sample_rate = DEFAULT_SAMPLE_RATE;
    config->control.packet_size = DEFAULT_PACKET_SIZE;
    config->control.num_packets = DEFAULT_NUM_PACKETS;
    config->control.noise_floor = DEFAULT_NOISE_FLOOR;
    config->control.random_seed = 0;  // Time-based
    config->control.verbose = 0;
    
    FSO_LOG_INFO("SimConfig", "Initialized with default values");
    return FSO_SUCCESS;
}

/* ============================================================================
 * Configuration Validation
 * ============================================================================ */

int sim_config_validate(const SimConfig* config) {
    if (config == NULL) {
        FSO_LOG_ERROR("SimConfig", "NULL config pointer");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Validate link parameters
    if (config->link.link_distance < 100.0 || config->link.link_distance > 10000.0) {
        FSO_LOG_ERROR("SimConfig", "Link distance must be between 100m and 10km, got %.1f m",
                     config->link.link_distance);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->link.transmit_power <= 0.0 || config->link.transmit_power > 1.0) {
        FSO_LOG_ERROR("SimConfig", "Transmit power must be between 0 and 1 W, got %.6f W",
                     config->link.transmit_power);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->link.receiver_sensitivity <= 0.0) {
        FSO_LOG_ERROR("SimConfig", "Receiver sensitivity must be positive, got %.3e W",
                     config->link.receiver_sensitivity);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->link.wavelength < 500e-9 || config->link.wavelength > 2000e-9) {
        FSO_LOG_ERROR("SimConfig", "Wavelength must be between 500nm and 2000nm, got %.1f nm",
                     config->link.wavelength * 1e9);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->link.beam_divergence <= 0.0 || config->link.beam_divergence > 0.1) {
        FSO_LOG_ERROR("SimConfig", "Beam divergence must be between 0 and 0.1 rad, got %.6f rad",
                     config->link.beam_divergence);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->link.receiver_aperture <= 0.0 || config->link.receiver_aperture > 1.0) {
        FSO_LOG_ERROR("SimConfig", "Receiver aperture must be between 0 and 1 m, got %.3f m",
                     config->link.receiver_aperture);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Validate environment parameters
    if (config->environment.weather < WEATHER_CLEAR || 
        config->environment.weather > WEATHER_HIGH_TURBULENCE) {
        FSO_LOG_ERROR("SimConfig", "Invalid weather condition: %d", config->environment.weather);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->environment.turbulence_strength < 1e-17 || 
        config->environment.turbulence_strength > 1e-12) {
        FSO_LOG_ERROR("SimConfig", "Turbulence strength (C_n²) must be between 1e-17 and 1e-12, got %.3e",
                     config->environment.turbulence_strength);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->environment.temperature < -50.0 || config->environment.temperature > 50.0) {
        FSO_LOG_ERROR("SimConfig", "Temperature must be between -50°C and 50°C, got %.1f°C",
                     config->environment.temperature);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->environment.humidity < 0.0 || config->environment.humidity > 1.0) {
        FSO_LOG_ERROR("SimConfig", "Humidity must be between 0 and 1, got %.2f",
                     config->environment.humidity);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->environment.visibility <= 0.0) {
        FSO_LOG_ERROR("SimConfig", "Visibility must be positive, got %.1f m",
                     config->environment.visibility);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->environment.correlation_time <= 0.0) {
        FSO_LOG_ERROR("SimConfig", "Correlation time must be positive, got %.6f s",
                     config->environment.correlation_time);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Validate system parameters
    if (config->system.modulation < MOD_OOK || config->system.modulation > MOD_DPSK) {
        FSO_LOG_ERROR("SimConfig", "Invalid modulation type: %d", config->system.modulation);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->system.modulation == MOD_PPM) {
        if (config->system.ppm_order != 2 && config->system.ppm_order != 4 &&
            config->system.ppm_order != 8 && config->system.ppm_order != 16) {
            FSO_LOG_ERROR("SimConfig", "PPM order must be 2, 4, 8, or 16, got %d",
                         config->system.ppm_order);
            return FSO_ERROR_INVALID_PARAM;
        }
    }
    
    if (config->system.fec_type < FEC_REED_SOLOMON || config->system.fec_type > FEC_LDPC) {
        FSO_LOG_ERROR("SimConfig", "Invalid FEC type: %d", config->system.fec_type);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->system.code_rate <= 0.0 || config->system.code_rate >= 1.0) {
        FSO_LOG_ERROR("SimConfig", "Code rate must be between 0 and 1, got %.2f",
                     config->system.code_rate);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->system.interleaver_depth < 1 || config->system.interleaver_depth > 100) {
        FSO_LOG_ERROR("SimConfig", "Interleaver depth must be between 1 and 100, got %d",
                     config->system.interleaver_depth);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->system.tracking_update_rate <= 0.0 || config->system.tracking_update_rate > 1000.0) {
        FSO_LOG_ERROR("SimConfig", "Tracking update rate must be between 0 and 1000 Hz, got %.1f Hz",
                     config->system.tracking_update_rate);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Validate simulation control parameters
    if (config->control.simulation_time <= 0.0) {
        FSO_LOG_ERROR("SimConfig", "Simulation time must be positive, got %.3f s",
                     config->control.simulation_time);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->control.sample_rate <= 0.0) {
        FSO_LOG_ERROR("SimConfig", "Sample rate must be positive, got %.3e Hz",
                     config->control.sample_rate);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->control.packet_size < 1 || config->control.packet_size > 1000000) {
        FSO_LOG_ERROR("SimConfig", "Packet size must be between 1 and 1000000 bytes, got %d",
                     config->control.packet_size);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->control.num_packets < 1 || config->control.num_packets > 1000000) {
        FSO_LOG_ERROR("SimConfig", "Number of packets must be between 1 and 1000000, got %d",
                     config->control.num_packets);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (config->control.noise_floor < 0.0) {
        FSO_LOG_ERROR("SimConfig", "Noise floor must be non-negative, got %.3e W",
                     config->control.noise_floor);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    FSO_LOG_INFO("SimConfig", "Configuration validated successfully");
    return FSO_SUCCESS;
}

/* ============================================================================
 * Preset Configurations
 * ============================================================================ */

int sim_config_create_preset(SimConfig* config, const char* scenario_name) {
    if (config == NULL || scenario_name == NULL) {
        FSO_LOG_ERROR("SimConfig", "NULL pointer in create_preset");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Start with defaults
    int result = sim_config_init_defaults(config);
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    // Apply scenario-specific settings
    if (strcmp(scenario_name, "clear") == 0) {
        config->environment.weather = WEATHER_CLEAR;
        config->environment.turbulence_strength = 1e-15;
        config->environment.visibility = 20000.0;
        config->link.link_distance = 1000.0;
        FSO_LOG_INFO("SimConfig", "Created 'clear' preset configuration");
        
    } else if (strcmp(scenario_name, "foggy") == 0) {
        config->environment.weather = WEATHER_FOG;
        config->environment.turbulence_strength = 5e-15;
        config->environment.visibility = 200.0;
        config->environment.humidity = 0.9;
        config->link.link_distance = 500.0;
        FSO_LOG_INFO("SimConfig", "Created 'foggy' preset configuration");
        
    } else if (strcmp(scenario_name, "rainy") == 0) {
        config->environment.weather = WEATHER_RAIN;
        config->environment.turbulence_strength = 3e-15;
        config->environment.rainfall_rate = 25.0;  // 25 mm/hr (moderate rain)
        config->environment.humidity = 0.95;
        config->link.link_distance = 800.0;
        FSO_LOG_INFO("SimConfig", "Created 'rainy' preset configuration");
        
    } else if (strcmp(scenario_name, "high_turbulence") == 0) {
        config->environment.weather = WEATHER_HIGH_TURBULENCE;
        config->environment.turbulence_strength = 1e-13;
        config->environment.temperature = 35.0;
        config->link.link_distance = 1500.0;
        config->system.enable_tracking = 1;  // Enable tracking for high turbulence
        FSO_LOG_INFO("SimConfig", "Created 'high_turbulence' preset configuration");
        
    } else {
        FSO_LOG_ERROR("SimConfig", "Unknown scenario: %s", scenario_name);
        FSO_LOG_ERROR("SimConfig", "Valid scenarios: clear, foggy, rainy, high_turbulence");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Configuration I/O
 * ============================================================================ */

void sim_config_print(const SimConfig* config) {
    if (config == NULL) {
        printf("NULL configuration\n");
        return;
    }
    
    printf("\n");
    printf("=== Simulator Configuration ===\n");
    printf("\n");
    
    printf("Link Parameters:\n");
    printf("  Distance:             %.1f m\n", config->link.link_distance);
    printf("  Transmit Power:       %.3e W (%.2f dBm)\n", 
           config->link.transmit_power,
           fso_watts_to_dbm(config->link.transmit_power));
    printf("  Receiver Sensitivity: %.3e W (%.2f dBm)\n",
           config->link.receiver_sensitivity,
           fso_watts_to_dbm(config->link.receiver_sensitivity));
    printf("  Wavelength:           %.1f nm\n", config->link.wavelength * 1e9);
    printf("  Beam Divergence:      %.3f mrad\n", config->link.beam_divergence * 1000.0);
    printf("  Receiver Aperture:    %.2f cm\n", config->link.receiver_aperture * 100.0);
    printf("\n");
    
    printf("Environment:\n");
    printf("  Weather:              %s\n", sim_weather_string(config->environment.weather));
    printf("  Turbulence (C_n²):    %.3e m^(-2/3)\n", config->environment.turbulence_strength);
    printf("  Temperature:          %.1f °C\n", config->environment.temperature);
    printf("  Humidity:             %.1f%%\n", config->environment.humidity * 100.0);
    printf("  Visibility:           %.1f m\n", config->environment.visibility);
    if (config->environment.rainfall_rate > 0.0) {
        printf("  Rainfall Rate:        %.1f mm/hr\n", config->environment.rainfall_rate);
    }
    if (config->environment.snowfall_rate > 0.0) {
        printf("  Snowfall Rate:        %.1f mm/hr\n", config->environment.snowfall_rate);
    }
    printf("  Correlation Time:     %.3f ms\n", config->environment.correlation_time * 1000.0);
    printf("\n");
    
    printf("System Configuration:\n");
    printf("  Modulation:           %s", sim_modulation_string(config->system.modulation));
    if (config->system.modulation == MOD_PPM) {
        printf(" (order %d)", config->system.ppm_order);
    }
    printf("\n");
    printf("  FEC:                  %s\n", sim_fec_string(config->system.fec_type));
    printf("  Code Rate:            %.2f\n", config->system.code_rate);
    printf("  Interleaver:          %s", config->system.use_interleaver ? "Enabled" : "Disabled");
    if (config->system.use_interleaver) {
        printf(" (depth %d)", config->system.interleaver_depth);
    }
    printf("\n");
    printf("  Beam Tracking:        %s", config->system.enable_tracking ? "Enabled" : "Disabled");
    if (config->system.enable_tracking) {
        printf(" (%.1f Hz)", config->system.tracking_update_rate);
    }
    printf("\n");
    printf("\n");
    
    printf("Simulation Control:\n");
    printf("  Simulation Time:      %.3f s\n", config->control.simulation_time);
    printf("  Sample Rate:          %.3e Hz\n", config->control.sample_rate);
    printf("  Packet Size:          %d bytes\n", config->control.packet_size);
    printf("  Number of Packets:    %d\n", config->control.num_packets);
    printf("  Noise Floor:          %.3e W (%.2f dBm)\n",
           config->control.noise_floor,
           fso_watts_to_dbm(config->control.noise_floor));
    printf("  Random Seed:          %u%s\n", 
           config->control.random_seed,
           config->control.random_seed == 0 ? " (time-based)" : "");
    printf("  Verbose:              %s\n", config->control.verbose ? "Yes" : "No");
    printf("\n");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* sim_modulation_string(ModulationType type) {
    switch (type) {
        case MOD_OOK:  return "OOK";
        case MOD_PPM:  return "PPM";
        case MOD_DPSK: return "DPSK";
        default:       return "Unknown";
    }
}

const char* sim_fec_string(FECType type) {
    switch (type) {
        case FEC_REED_SOLOMON: return "Reed-Solomon";
        case FEC_LDPC:         return "LDPC";
        default:               return "Unknown";
    }
}

const char* sim_weather_string(WeatherCondition weather) {
    switch (weather) {
        case WEATHER_CLEAR:           return "Clear";
        case WEATHER_FOG:             return "Fog";
        case WEATHER_RAIN:            return "Rain";
        case WEATHER_SNOW:            return "Snow";
        case WEATHER_HIGH_TURBULENCE: return "High Turbulence";
        default:                      return "Unknown";
    }
}
