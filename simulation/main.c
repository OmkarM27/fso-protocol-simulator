/**
 * @file main.c
 * @brief Main entry point for FSO simulator
 * 
 * Command-line interface for running FSO link simulations.
 */

#include "simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Usage Information
 * ============================================================================ */

static void print_usage(const char* program_name) {
    printf("FSO Communication Link Simulator\n");
    printf("================================\n\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -s, --scenario <name>    Run predefined scenario\n");
    printf("  -l, --list               List available scenarios\n");
    printf("  -b, --batch              Run all scenarios in batch mode\n");
    printf("  -o, --output <base>      Output base filename (default: results)\n");
    printf("  -v, --verbose            Enable verbose output\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s --scenario clear\n", program_name);
    printf("  %s --batch --output batch_results\n", program_name);
    printf("  %s --list\n\n", program_name);
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char* argv[]) {
    // Set default log level
    fso_set_log_level(LOG_INFO);
    
    // Default options
    const char* scenario_name = NULL;
    const char* output_base = "results";
    int list_scenarios = 0;
    int batch_mode = 0;
    int verbose = 0;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_scenarios = 1;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--batch") == 0) {
            batch_mode = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
            fso_set_log_level(LOG_DEBUG);
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scenario") == 0) && i + 1 < argc) {
            scenario_name = argv[++i];
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            output_base = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Handle list scenarios
    if (list_scenarios) {
        sim_list_scenarios();
        return 0;
    }
    
    // Handle batch mode
    if (batch_mode) {
        printf("Running batch simulation...\n\n");
        
        // Allocate batch results (assuming max 10 scenarios)
        BatchResult* batch_results = (BatchResult*)calloc(10, sizeof(BatchResult));
        if (batch_results == NULL) {
            fprintf(stderr, "Failed to allocate batch results\n");
            return 1;
        }
        
        int num_successful = sim_run_all_scenarios(batch_results);
        
        // Print summary
        sim_print_batch_summary(batch_results, 6);  // We have 6 scenarios
        
        // Cleanup
        sim_free_batch_results(batch_results, 6);
        free(batch_results);
        
        printf("Batch simulation complete: %d scenarios successful\n", num_successful);
        return 0;
    }
    
    // Single scenario mode
    if (scenario_name == NULL) {
        scenario_name = "clear";  // Default scenario
        printf("No scenario specified, using default: %s\n\n", scenario_name);
    }
    
    // Load scenario configuration
    SimConfig config;
    int result = sim_load_scenario(&config, scenario_name);
    if (result != FSO_SUCCESS) {
        fprintf(stderr, "Failed to load scenario: %s\n", scenario_name);
        fprintf(stderr, "Use --list to see available scenarios\n");
        return 1;
    }
    
    config.control.verbose = verbose;
    
    // Print configuration
    sim_config_print(&config);
    
    // Run simulation
    SimResults results;
    printf("Running simulation...\n\n");
    
    if (config.system.enable_tracking) {
        result = sim_run_with_tracking(&config, &results);
    } else {
        result = sim_run(&config, &results);
    }
    
    if (result != FSO_SUCCESS) {
        fprintf(stderr, "Simulation failed with error code: %d\n", result);
        return 1;
    }
    
    // Print results
    sim_results_print(&results);
    
    // Generate visualizations
    printf("Generating visualizations...\n");
    result = sim_generate_all_visualizations(&config, &results, output_base);
    if (result != FSO_SUCCESS) {
        fprintf(stderr, "Warning: Failed to generate some visualizations\n");
    }
    
    // Cleanup
    sim_results_free(&results);
    
    printf("\nSimulation complete!\n");
    return 0;
}
