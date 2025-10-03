/**
 * @file sim_scenarios.c
 * @brief Test scenario management and batch simulation
 * 
 * Provides preset configurations for common test scenarios and batch
 * simulation capabilities for running multiple scenarios.
 */

#include "simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Scenario Definitions
 * ============================================================================ */

/**
 * @brief Scenario descriptor
 */
typedef struct {
    const char* name;
    const char* description;
    void (*configure)(SimConfig* config);
} ScenarioDescriptor;

/* ============================================================================
 * Scenario Configuration Functions
 * ============================================================================ */

static void configure_clear_scenario(SimConfig* config) {
    sim_config_init_defaults(config);
    
    config->environment.weather = WEATHER_CLEAR;
    config->environment.turbulence_strength = 1e-15;
    config->environment.visibility = 20000.0;
    config->environment.temperature = 20.0;
    config->environment.humidity = 0.5;
    
    config->link.link_distance = 1000.0;
    config->link.transmit_power = 0.001;  // 1 mW
    
    config->control.num_packets = 100;
    config->control.packet_size = 1024;
}

static void configure_foggy_scenario(SimConfig* config) {
    sim_config_init_defaults(config);
    
    config->environment.weather = WEATHER_FOG;
    config->environment.turbulence_strength = 5e-15;
    config->environment.visibility = 200.0;  // 200m visibility
    config->environment.temperature = 15.0;
    config->environment.humidity = 0.95;
    
    config->link.link_distance = 500.0;  // Shorter link due to fog
    config->link.transmit_power = 0.005;  // 5 mW (higher power)
    
    config->control.num_packets = 100;
    config->control.packet_size = 1024;
    
    // Use stronger FEC for foggy conditions
    config->system.code_rate = 0.75;
}

static void configure_rainy_scenario(SimConfig* config) {
    sim_config_init_defaults(config);
    
    config->environment.weather = WEATHER_RAIN;
    config->environment.turbulence_strength = 3e-15;
    config->environment.rainfall_rate = 25.0;  // 25 mm/hr (moderate rain)
    config->environment.visibility = 1000.0;
    config->environment.temperature = 18.0;
    config->environment.humidity = 0.95;
    
    config->link.link_distance = 800.0;
    config->link.transmit_power = 0.003;  // 3 mW
    
    config->control.num_packets = 100;
    config->control.packet_size = 1024;
    
    config->system.code_rate = 0.75;
}

static void configure_high_turbulence_scenario(SimConfig* config) {
    sim_config_init_defaults(config);
    
    config->environment.weather = WEATHER_HIGH_TURBULENCE;
    config->environment.turbulence_strength = 1e-13;  // Very high turbulence
    config->environment.visibility = 10000.0;
    config->environment.temperature = 35.0;  // Hot day
    config->environment.humidity = 0.3;
    config->environment.correlation_time = 0.0005;  // Fast fading
    
    config->link.link_distance = 1500.0;
    config->link.transmit_power = 0.002;  // 2 mW
    
    config->control.num_packets = 100;
    config->control.packet_size = 1024;
    
    // Enable beam tracking for high turbulence
    config->system.enable_tracking = 1;
    config->system.tracking_update_rate = 100.0;
}

static void configure_long_range_scenario(SimConfig* config) {
    sim_config_init_defaults(config);
    
    config->environment.weather = WEATHER_CLEAR;
    config->environment.turbulence_strength = 2e-15;
    config->environment.visibility = 20000.0;
    
    config->link.link_distance = 5000.0;  // 5 km
    config->link.transmit_power = 0.010;  // 10 mW
    config->link.beam_divergence = 0.0005;  // Tighter beam
    
    config->control.num_packets = 100;
    config->control.packet_size = 1024;
    
    config->system.code_rate = 0.7;  // Stronger FEC
}

static void configure_short_range_scenario(SimConfig* config) {
    sim_config_init_defaults(config);
    
    config->environment.weather = WEATHER_CLEAR;
    config->environment.turbulence_strength = 5e-16;
    
    config->link.link_distance = 100.0;  // 100 m
    config->link.transmit_power = 0.0001;  // 0.1 mW
    
    config->control.num_packets = 100;
    config->control.packet_size = 1024;
    
    config->system.code_rate = 0.9;  // Light FEC
}

/* ============================================================================
 * Scenario Registry
 * ============================================================================ */

static const ScenarioDescriptor g_scenarios[] = {
    {
        .name = "clear",
        .description = "Clear weather, moderate distance (1 km)",
        .configure = configure_clear_scenario
    },
    {
        .name = "foggy",
        .description = "Dense fog, short distance (500 m)",
        .configure = configure_foggy_scenario
    },
    {
        .name = "rainy",
        .description = "Moderate rain, medium distance (800 m)",
        .configure = configure_rainy_scenario
    },
    {
        .name = "high_turbulence",
        .description = "High atmospheric turbulence with beam tracking (1.5 km)",
        .configure = configure_high_turbulence_scenario
    },
    {
        .name = "long_range",
        .description = "Long range clear link (5 km)",
        .configure = configure_long_range_scenario
    },
    {
        .name = "short_range",
        .description = "Short range indoor/building link (100 m)",
        .configure = configure_short_range_scenario
    }
};

static const int g_num_scenarios = sizeof(g_scenarios) / sizeof(g_scenarios[0]);

/* ============================================================================
 * Scenario Management Functions
 * ============================================================================ */

/**
 * @brief List all available scenarios
 * 
 * Prints a list of all predefined scenarios with descriptions.
 */
void sim_list_scenarios(void) {
    printf("\nAvailable Simulation Scenarios:\n");
    printf("================================\n\n");
    
    for (int i = 0; i < g_num_scenarios; i++) {
        printf("  %-20s - %s\n", g_scenarios[i].name, g_scenarios[i].description);
    }
    
    printf("\n");
}

/**
 * @brief Load scenario configuration by name
 * 
 * @param config Output configuration structure
 * @param scenario_name Name of scenario to load
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Requirement 6.2: Preset configurations for test scenarios
 */
int sim_load_scenario(SimConfig* config, const char* scenario_name) {
    if (config == NULL || scenario_name == NULL) {
        FSO_LOG_ERROR("Scenarios", "NULL pointer in load_scenario");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Search for scenario
    for (int i = 0; i < g_num_scenarios; i++) {
        if (strcmp(g_scenarios[i].name, scenario_name) == 0) {
            FSO_LOG_INFO("Scenarios", "Loading scenario: %s", scenario_name);
            g_scenarios[i].configure(config);
            return FSO_SUCCESS;
        }
    }
    
    FSO_LOG_ERROR("Scenarios", "Unknown scenario: %s", scenario_name);
    return FSO_ERROR_INVALID_PARAM;
}

/**
 * @brief Get scenario description
 * 
 * @param scenario_name Name of scenario
 * @return Description string, or NULL if not found
 */
const char* sim_get_scenario_description(const char* scenario_name) {
    if (scenario_name == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < g_num_scenarios; i++) {
        if (strcmp(g_scenarios[i].name, scenario_name) == 0) {
            return g_scenarios[i].description;
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Batch Simulation
 * ============================================================================ */

/**
 * @brief Batch simulation results
 */
typedef struct {
    char scenario_name[64];
    SimResults results;
    int success;
} BatchResult;

/**
 * @brief Run batch simulation across multiple scenarios
 * 
 * Runs simulations for all specified scenarios and collects results.
 * 
 * @param scenario_names Array of scenario names to run
 * @param num_scenarios Number of scenarios
 * @param batch_results Output array of batch results (must be pre-allocated)
 * @return Number of successful simulations
 * 
 * @note Requirement 6.2: Batch simulation mode for multiple scenarios
 */
int sim_run_batch(const char** scenario_names, int num_scenarios, 
                  BatchResult* batch_results) {
    if (scenario_names == NULL || batch_results == NULL || num_scenarios <= 0) {
        FSO_LOG_ERROR("Scenarios", "Invalid parameters for batch simulation");
        return 0;
    }
    
    int successful = 0;
    
    FSO_LOG_INFO("Scenarios", "Starting batch simulation: %d scenarios", num_scenarios);
    
    for (int i = 0; i < num_scenarios; i++) {
        const char* scenario_name = scenario_names[i];
        BatchResult* batch_result = &batch_results[i];
        
        // Copy scenario name
        strncpy(batch_result->scenario_name, scenario_name, 
                sizeof(batch_result->scenario_name) - 1);
        batch_result->scenario_name[sizeof(batch_result->scenario_name) - 1] = '\0';
        
        printf("\n");
        printf("========================================\n");
        printf("Running scenario: %s\n", scenario_name);
        printf("========================================\n");
        
        // Load scenario configuration
        SimConfig config;
        int result = sim_load_scenario(&config, scenario_name);
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("Scenarios", "Failed to load scenario: %s", scenario_name);
            batch_result->success = 0;
            continue;
        }
        
        // Print configuration
        sim_config_print(&config);
        
        // Run simulation
        if (config.system.enable_tracking) {
            result = sim_run_with_tracking(&config, &batch_result->results);
        } else {
            result = sim_run(&config, &batch_result->results);
        }
        
        if (result == FSO_SUCCESS) {
            batch_result->success = 1;
            successful++;
            
            // Print results
            sim_results_print(&batch_result->results);
            
            // Export results to CSV
            char csv_filename[256];
            snprintf(csv_filename, sizeof(csv_filename), 
                    "results_%s.csv", scenario_name);
            sim_results_export_csv(&batch_result->results, csv_filename);
            
            snprintf(csv_filename, sizeof(csv_filename), 
                    "packets_%s.csv", scenario_name);
            sim_results_export_packets_csv(&batch_result->results, csv_filename);
            
        } else {
            FSO_LOG_ERROR("Scenarios", "Simulation failed for scenario: %s", scenario_name);
            batch_result->success = 0;
        }
    }
    
    printf("\n");
    printf("========================================\n");
    printf("Batch Simulation Complete\n");
    printf("========================================\n");
    printf("Successful: %d / %d scenarios\n", successful, num_scenarios);
    printf("\n");
    
    return successful;
}

/**
 * @brief Run all predefined scenarios
 * 
 * Convenience function to run all available scenarios.
 * 
 * @param batch_results Output array (must be pre-allocated with size >= num scenarios)
 * @return Number of successful simulations
 */
int sim_run_all_scenarios(BatchResult* batch_results) {
    if (batch_results == NULL) {
        FSO_LOG_ERROR("Scenarios", "NULL batch_results pointer");
        return 0;
    }
    
    // Build array of scenario names
    const char** scenario_names = (const char**)malloc(g_num_scenarios * sizeof(char*));
    if (scenario_names == NULL) {
        FSO_LOG_ERROR("Scenarios", "Failed to allocate scenario names array");
        return 0;
    }
    
    for (int i = 0; i < g_num_scenarios; i++) {
        scenario_names[i] = g_scenarios[i].name;
    }
    
    int result = sim_run_batch(scenario_names, g_num_scenarios, batch_results);
    
    free(scenario_names);
    
    return result;
}

/**
 * @brief Free batch results
 * 
 * @param batch_results Array of batch results
 * @param num_results Number of results
 */
void sim_free_batch_results(BatchResult* batch_results, int num_results) {
    if (batch_results == NULL) {
        return;
    }
    
    for (int i = 0; i < num_results; i++) {
        if (batch_results[i].success) {
            sim_results_free(&batch_results[i].results);
        }
    }
}

/**
 * @brief Print batch results summary
 * 
 * @param batch_results Array of batch results
 * @param num_results Number of results
 */
void sim_print_batch_summary(const BatchResult* batch_results, int num_results) {
    if (batch_results == NULL || num_results <= 0) {
        return;
    }
    
    printf("\n");
    printf("=== Batch Simulation Summary ===\n");
    printf("\n");
    printf("%-20s %10s %10s %10s %10s\n", 
           "Scenario", "BER", "SNR (dB)", "PLR", "Status");
    printf("%-20s %10s %10s %10s %10s\n",
           "--------------------", "----------", "----------", "----------", "----------");
    
    for (int i = 0; i < num_results; i++) {
        const BatchResult* result = &batch_results[i];
        
        if (result->success) {
            printf("%-20s %10.3e %10.2f %10.3f %10s\n",
                   result->scenario_name,
                   result->results.avg_ber,
                   result->results.avg_snr,
                   result->results.packet_loss_rate,
                   "SUCCESS");
        } else {
            printf("%-20s %10s %10s %10s %10s\n",
                   result->scenario_name,
                   "-", "-", "-", "FAILED");
        }
    }
    
    printf("\n");
}
