/**
 * @file simulator.h
 * @brief Hardware-in-Loop (HIL) simulator for FSO communication systems
 * 
 * This module provides end-to-end link simulation with configurable parameters
 * and comprehensive metrics collection. Simulates complete transmitter and
 * receiver chains including modulation, FEC, channel effects, and demodulation.
 * 
 * Requirements addressed:
 * - 6.1: Complete transmitter and receiver chain modeling
 * - 6.2: Configurable link parameters and environmental conditions
 * - 6.3: Channel propagation with atmospheric effects
 * - 6.4: Performance metrics (BER, SNR, throughput, packet loss)
 * - 6.5: Visualization and time-series data collection
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "../src/fso.h"
#include "../src/modulation/modulation.h"
#include "../src/fec/fec.h"
#include "../src/turbulence/channel.h"
#include "../src/beam_tracking/beam_tracking.h"

/* ============================================================================
 * Simulator Configuration Structures
 * ============================================================================ */

/**
 * @brief Link parameters configuration
 */
typedef struct {
    double link_distance;        /**< Link distance in meters (100 to 10000) */
    double transmit_power;       /**< Transmit power in watts */
    double receiver_sensitivity; /**< Receiver sensitivity in watts */
    double wavelength;           /**< Optical wavelength in meters (e.g., 1550nm) */
    double beam_divergence;      /**< Beam divergence angle in radians */
    double receiver_aperture;    /**< Receiver aperture diameter in meters */
} LinkConfig;

/**
 * @brief Environmental parameters configuration
 */
typedef struct {
    WeatherCondition weather;    /**< Weather condition */
    double turbulence_strength;  /**< Turbulence strength (C_n²) */
    double temperature;          /**< Temperature in Celsius */
    double humidity;             /**< Relative humidity (0-1) */
    double visibility;           /**< Visibility in meters (for fog) */
    double rainfall_rate;        /**< Rainfall rate in mm/hr */
    double snowfall_rate;        /**< Snowfall rate in mm/hr */
    double correlation_time;     /**< Temporal correlation time in seconds */
} EnvironmentConfig;

/**
 * @brief System configuration parameters
 */
typedef struct {
    ModulationType modulation;   /**< Modulation scheme */
    int ppm_order;               /**< PPM order (if using PPM) */
    FECType fec_type;            /**< FEC codec type */
    double code_rate;            /**< FEC code rate */
    int use_interleaver;         /**< Enable interleaving (0 or 1) */
    int interleaver_depth;       /**< Interleaver depth */
    int enable_tracking;         /**< Enable beam tracking (0 or 1) */
    double tracking_update_rate; /**< Beam tracking update rate in Hz */
} SystemConfig;

/**
 * @brief Simulation control parameters
 */
typedef struct {
    double simulation_time;      /**< Total simulation time in seconds */
    double sample_rate;          /**< Sample rate in Hz */
    int packet_size;             /**< Packet size in bytes */
    int num_packets;             /**< Number of packets to simulate */
    double noise_floor;          /**< Noise floor in watts */
    unsigned int random_seed;    /**< Random seed (0 for time-based) */
    int verbose;                 /**< Verbose output (0 or 1) */
} SimulationControl;

/**
 * @brief Complete simulator configuration
 * 
 * Combines all configuration parameters for a simulation run.
 * 
 * @note Requirement 6.1: Link, environment, and system parameters
 * @note Requirement 6.2: Configurable simulation scenarios
 */
typedef struct {
    LinkConfig link;             /**< Link parameters */
    EnvironmentConfig environment; /**< Environmental parameters */
    SystemConfig system;         /**< System configuration */
    SimulationControl control;   /**< Simulation control */
} SimConfig;

/* ============================================================================
 * Simulation Results Structures
 * ============================================================================ */

/**
 * @brief Packet-level statistics
 */
typedef struct {
    int packet_id;               /**< Packet identifier */
    int bits_transmitted;        /**< Number of bits transmitted */
    int bits_received;           /**< Number of bits received */
    int bit_errors;              /**< Number of bit errors */
    double ber;                  /**< Bit error rate for this packet */
    double snr_db;               /**< SNR in dB for this packet */
    double received_power;       /**< Received power in watts */
    int fec_corrected_errors;    /**< Errors corrected by FEC */
    int fec_uncorrectable;       /**< Flag: 1 if FEC failed */
} PacketStats;

/**
 * @brief Time-series data point
 */
typedef struct {
    double timestamp;            /**< Time in seconds */
    double ber;                  /**< Bit error rate */
    double snr_db;               /**< SNR in dB */
    double received_power;       /**< Received power in watts */
    double throughput;           /**< Throughput in bits/second */
    double beam_azimuth;         /**< Beam azimuth angle (radians) */
    double beam_elevation;       /**< Beam elevation angle (radians) */
    double signal_strength;      /**< Signal strength (normalized) */
} TimeSeriesPoint;

/**
 * @brief Simulation results and metrics
 * 
 * Contains aggregated statistics and time-series history for analysis
 * and visualization.
 * 
 * @note Requirement 6.4: Performance metrics collection
 * @note Requirement 6.5: Time-series history for visualization
 */
typedef struct {
    // Aggregated metrics
    double avg_ber;              /**< Average bit error rate */
    double avg_snr;              /**< Average SNR in dB */
    double avg_throughput;       /**< Average throughput in bits/second */
    double packet_loss_rate;     /**< Packet loss rate (0-1) */
    
    // Min/max values
    double min_snr;              /**< Minimum SNR in dB */
    double max_snr;              /**< Maximum SNR in dB */
    double min_ber;              /**< Minimum BER */
    double max_ber;              /**< Maximum BER */
    
    // Counters
    int total_packets;           /**< Total packets transmitted */
    int packets_received;        /**< Packets successfully received */
    int packets_lost;            /**< Packets lost (uncorrectable errors) */
    long long total_bits;        /**< Total bits transmitted */
    long long total_bit_errors;  /**< Total bit errors */
    long long fec_corrected_errors; /**< Total errors corrected by FEC */
    
    // Beam tracking metrics (if enabled)
    int tracking_enabled;        /**< Flag: 1 if tracking was enabled */
    double avg_beam_azimuth;     /**< Average beam azimuth */
    double avg_beam_elevation;   /**< Average beam elevation */
    int tracking_updates;        /**< Number of tracking updates */
    int reacquisitions;          /**< Number of beam reacquisitions */
    
    // Time-series history
    TimeSeriesPoint* history;    /**< Array of time-series data points */
    size_t history_length;       /**< Number of points in history */
    size_t history_capacity;     /**< Allocated capacity for history */
    
    // Packet-level statistics
    PacketStats* packet_stats;   /**< Array of per-packet statistics */
    size_t num_packet_stats;     /**< Number of packet statistics */
    
    // Timing information
    double simulation_duration;  /**< Actual simulation duration in seconds */
    double start_time;           /**< Simulation start timestamp */
    double end_time;             /**< Simulation end timestamp */
} SimResults;

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

/**
 * @brief Initialize simulator configuration with default values
 * 
 * Sets reasonable default values for all configuration parameters.
 * 
 * @param config Pointer to SimConfig structure to initialize
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_config_init_defaults(SimConfig* config);

/**
 * @brief Validate simulator configuration
 * 
 * Checks that all configuration parameters are within valid ranges
 * and consistent with each other.
 * 
 * @param config Pointer to SimConfig structure to validate
 * @return FSO_SUCCESS if valid, error code with details otherwise
 * 
 * @note Requirement 6.2: Configuration validation
 */
int sim_config_validate(const SimConfig* config);

/**
 * @brief Load configuration from file
 * 
 * Loads simulation configuration from a text file with key=value format.
 * 
 * @param config Pointer to SimConfig structure to populate
 * @param filename Path to configuration file
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_config_load(SimConfig* config, const char* filename);

/**
 * @brief Save configuration to file
 * 
 * Saves current configuration to a text file for later reuse.
 * 
 * @param config Pointer to SimConfig structure to save
 * @param filename Path to output file
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_config_save(const SimConfig* config, const char* filename);

/**
 * @brief Print configuration summary
 * 
 * Prints human-readable summary of configuration to stdout.
 * 
 * @param config Pointer to SimConfig structure to print
 */
void sim_config_print(const SimConfig* config);

/**
 * @brief Create preset configuration for specific scenario
 * 
 * Creates a predefined configuration for common test scenarios.
 * 
 * @param config Pointer to SimConfig structure to populate
 * @param scenario_name Scenario name: "clear", "foggy", "rainy", "high_turbulence"
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Requirement 6.2: Preset configurations for test scenarios
 */
int sim_config_create_preset(SimConfig* config, const char* scenario_name);

/* ============================================================================
 * Results Functions
 * ============================================================================ */

/**
 * @brief Initialize simulation results structure
 * 
 * Allocates memory for time-series history and packet statistics.
 * 
 * @param results Pointer to SimResults structure to initialize
 * @param history_capacity Initial capacity for time-series history
 * @param num_packets Expected number of packets
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_results_init(SimResults* results, size_t history_capacity, int num_packets);

/**
 * @brief Free simulation results resources
 * 
 * Releases all memory allocated for results structure.
 * 
 * @param results Pointer to SimResults structure to free
 */
void sim_results_free(SimResults* results);

/**
 * @brief Add time-series data point to results
 * 
 * Appends a new data point to the time-series history, reallocating
 * if necessary.
 * 
 * @param results Pointer to SimResults structure
 * @param point Time-series data point to add
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_results_add_point(SimResults* results, const TimeSeriesPoint* point);

/**
 * @brief Add packet statistics to results
 * 
 * Records statistics for a single packet.
 * 
 * @param results Pointer to SimResults structure
 * @param stats Packet statistics to add
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_results_add_packet(SimResults* results, const PacketStats* stats);

/**
 * @brief Calculate aggregated metrics from collected data
 * 
 * Computes average, min, max values from time-series and packet data.
 * Should be called after simulation completes.
 * 
 * @param results Pointer to SimResults structure
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Requirement 6.4: Calculate BER, SNR, throughput, packet loss rate
 */
int sim_results_calculate_metrics(SimResults* results);

/**
 * @brief Print results summary
 * 
 * Prints human-readable summary of simulation results to stdout.
 * 
 * @param results Pointer to SimResults structure to print
 */
void sim_results_print(const SimResults* results);

/**
 * @brief Export results to CSV file
 * 
 * Exports time-series data to CSV format for external analysis.
 * 
 * @param results Pointer to SimResults structure
 * @param filename Path to output CSV file
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Requirement 6.5: Write results to CSV files
 */
int sim_results_export_csv(const SimResults* results, const char* filename);

/**
 * @brief Export packet statistics to CSV file
 * 
 * Exports per-packet statistics to CSV format.
 * 
 * @param results Pointer to SimResults structure
 * @param filename Path to output CSV file
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_results_export_packets_csv(const SimResults* results, const char* filename);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get string representation of modulation type
 * 
 * @param type Modulation type
 * @return String representation
 */
const char* sim_modulation_string(ModulationType type);

/**
 * @brief Get string representation of FEC type
 * 
 * @param type FEC type
 * @return String representation
 */
const char* sim_fec_string(FECType type);

/**
 * @brief Get string representation of weather condition
 * 
 * @param weather Weather condition
 * @return String representation
 */
const char* sim_weather_string(WeatherCondition weather);

#endif /* SIMULATOR_H */
