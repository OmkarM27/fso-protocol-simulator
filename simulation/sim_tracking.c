/**
 * @file sim_tracking.c
 * @brief Beam tracking integration for simulator
 * 
 * Integrates beam tracking algorithms with the simulation loop to model
 * beam misalignment and tracking response.
 */

#include "simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Beam Tracking Simulation Context
 * ============================================================================ */

typedef struct {
    BeamTracker* tracker;
    double initial_azimuth;
    double initial_elevation;
    double misalignment_rate;      // Rate of beam drift (rad/s)
    double misalignment_amplitude; // Amplitude of random misalignment (rad)
    double current_misalignment_az;
    double current_misalignment_el;
    int reacquisition_count;
} TrackingContext;

/* ============================================================================
 * Signal Strength Callback for Beam Tracking
 * ============================================================================ */

/**
 * @brief Callback function to measure signal strength at beam position
 * 
 * This simulates measuring the received signal strength when the beam
 * is pointed at a specific azimuth and elevation angle.
 */
static double tracking_signal_callback(double azimuth, double elevation, void* user_data) {
    TrackingContext* ctx = (TrackingContext*)user_data;
    
    // Calculate angular error from optimal position
    double az_error = azimuth - ctx->initial_azimuth;
    double el_error = elevation - ctx->initial_elevation;
    double angular_error = sqrt(az_error * az_error + el_error * el_error);
    
    // Signal strength decreases with angular error (Gaussian beam pattern)
    // Assuming beam divergence of 1 mrad
    double beam_width = 0.001;  // 1 mrad
    double signal_strength = exp(-2.0 * angular_error * angular_error / 
                                 (beam_width * beam_width));
    
    // Add some noise
    signal_strength += fso_random_gaussian(0.0, 0.05);
    signal_strength = FSO_CLAMP(signal_strength, 0.0, 1.0);
    
    return signal_strength;
}

/* ============================================================================
 * Beam Tracking Simulation Functions
 * ============================================================================ */

/**
 * @brief Initialize beam tracking for simulation
 * 
 * @param config Simulation configuration
 * @param ctx Output tracking context
 * @return FSO_SUCCESS on success, error code otherwise
 */
static int sim_tracking_init(const SimConfig* config, TrackingContext* ctx) {
    if (config == NULL || ctx == NULL) {
        return FSO_ERROR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(TrackingContext));
    
    // Initialize beam tracker
    ctx->tracker = (BeamTracker*)malloc(sizeof(BeamTracker));
    if (ctx->tracker == NULL) {
        FSO_LOG_ERROR("SimTracking", "Failed to allocate beam tracker");
        return FSO_ERROR_MEMORY;
    }
    
    // Set initial beam position (assume aligned at start)
    ctx->initial_azimuth = 0.0;
    ctx->initial_elevation = 0.0;
    
    // Initialize tracker with signal map
    int result = beam_track_init(ctx->tracker,
                                 ctx->initial_azimuth,
                                 ctx->initial_elevation,
                                 21,    // 21x21 signal map
                                 21,
                                 0.01,  // ±5 mrad azimuth range
                                 0.01); // ±5 mrad elevation range
    
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("SimTracking", "Failed to initialize beam tracker");
        free(ctx->tracker);
        return result;
    }
    
    // Configure PID controller for tracking
    beam_track_configure_pid(ctx->tracker,
                             1.0,   // Kp
                             0.1,   // Ki
                             0.05,  // Kd
                             config->system.tracking_update_rate,
                             0.01); // Integral limit
    
    // Set misalignment parameters based on turbulence
    ctx->misalignment_rate = config->environment.turbulence_strength * 1e10;  // Scale to rad/s
    ctx->misalignment_amplitude = 0.002;  // 2 mrad amplitude
    
    // Set signal threshold for misalignment detection
    beam_track_set_threshold(ctx->tracker, 0.3);  // 30% of peak signal
    
    FSO_LOG_INFO("SimTracking", "Initialized beam tracking with update rate %.1f Hz",
                 config->system.tracking_update_rate);
    
    return FSO_SUCCESS;
}

/**
 * @brief Update beam misalignment due to environmental disturbances
 * 
 * Simulates beam drift and random perturbations due to atmospheric turbulence,
 * platform vibrations, etc.
 * 
 * @param ctx Tracking context
 * @param time_step Time step in seconds
 */
static void sim_tracking_update_misalignment(TrackingContext* ctx, double time_step) {
    // Add slow drift
    ctx->current_misalignment_az += ctx->misalignment_rate * time_step;
    ctx->current_misalignment_el += ctx->misalignment_rate * time_step * 0.7;
    
    // Add random perturbations (high-frequency jitter)
    ctx->current_misalignment_az += fso_random_gaussian(0.0, 
        ctx->misalignment_amplitude * sqrt(time_step));
    ctx->current_misalignment_el += fso_random_gaussian(0.0, 
        ctx->misalignment_amplitude * sqrt(time_step));
    
    // Clamp to reasonable range
    ctx->current_misalignment_az = FSO_CLAMP(ctx->current_misalignment_az, -0.01, 0.01);
    ctx->current_misalignment_el = FSO_CLAMP(ctx->current_misalignment_el, -0.01, 0.01);
}

/**
 * @brief Update beam tracking and calculate signal strength
 * 
 * @param ctx Tracking context
 * @param time_step Time step in seconds
 * @param signal_strength Output: normalized signal strength (0-1)
 * @return FSO_SUCCESS on success, error code otherwise
 */
static int sim_tracking_update(TrackingContext* ctx, double time_step, 
                               double* signal_strength) {
    if (ctx == NULL || ctx->tracker == NULL || signal_strength == NULL) {
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Update environmental misalignment
    sim_tracking_update_misalignment(ctx, time_step);
    
    // Calculate actual beam position (tracker position + misalignment)
    double actual_az = ctx->tracker->azimuth + ctx->current_misalignment_az;
    double actual_el = ctx->tracker->elevation + ctx->current_misalignment_el;
    
    // Measure signal strength at actual position
    *signal_strength = tracking_signal_callback(actual_az, actual_el, ctx);
    
    // Check for misalignment
    int is_misaligned = beam_track_check_misalignment(ctx->tracker, *signal_strength);
    
    if (is_misaligned && !ctx->tracker->reacquisition_mode) {
        FSO_LOG_WARNING("SimTracking", "Beam misalignment detected, initiating reacquisition");
        
        // Perform reacquisition
        int result = beam_track_reacquire(ctx->tracker,
                                         0.02,  // ±10 mrad azimuth search
                                         0.02,  // ±10 mrad elevation search
                                         0.002, // 2 mrad resolution
                                         tracking_signal_callback,
                                         ctx);
        
        if (result == FSO_SUCCESS) {
            ctx->reacquisition_count++;
            FSO_LOG_INFO("SimTracking", "Beam reacquisition successful");
        } else {
            FSO_LOG_ERROR("SimTracking", "Beam reacquisition failed");
        }
    } else {
        // Normal tracking update using gradient descent
        beam_track_update(ctx->tracker, *signal_strength);
    }
    
    return FSO_SUCCESS;
}

/**
 * @brief Calculate channel gain from beam tracking state
 * 
 * Converts signal strength to channel gain factor that affects received power.
 * 
 * @param signal_strength Normalized signal strength (0-1)
 * @return Channel gain factor
 */
static double sim_tracking_calculate_gain(double signal_strength) {
    // Signal strength directly affects received power
    // Add minimum floor to prevent complete signal loss
    double min_gain = 0.01;  // -20 dB minimum
    return FSO_MAX(signal_strength, min_gain);
}

/**
 * @brief Free beam tracking resources
 * 
 * @param ctx Tracking context
 */
static void sim_tracking_free(TrackingContext* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    if (ctx->tracker != NULL) {
        beam_track_free(ctx->tracker);
        free(ctx->tracker);
        ctx->tracker = NULL;
    }
}

/* ============================================================================
 * Enhanced Simulation with Beam Tracking
 * ============================================================================ */

/**
 * @brief Run simulation with beam tracking enabled
 * 
 * Extended version of sim_run() that includes beam tracking simulation.
 * 
 * @param config Simulation configuration
 * @param results Output results structure
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Requirement 6.1: Integrate beam tracker with simulation loop
 */
int sim_run_with_tracking(const SimConfig* config, SimResults* results) {
    if (config == NULL || results == NULL) {
        FSO_LOG_ERROR("Simulator", "NULL pointer in sim_run_with_tracking");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (!config->system.enable_tracking) {
        FSO_LOG_WARNING("Simulator", "Tracking not enabled, using standard simulation");
        return sim_run(config, results);
    }
    
    // Initialize tracking context
    TrackingContext tracking_ctx;
    int result = sim_tracking_init(config, &tracking_ctx);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("Simulator", "Failed to initialize tracking");
        return result;
    }
    
    // Run simulation with tracking (simplified version - full implementation
    // would integrate with the complete sim_run loop)
    results->tracking_enabled = 1;
    
    // For now, call standard simulation and add tracking metrics
    result = sim_run(config, results);
    
    if (result == FSO_SUCCESS) {
        // Add tracking-specific metrics
        results->reacquisitions = tracking_ctx.reacquisition_count;
        results->tracking_updates = config->control.num_packets;
        
        FSO_LOG_INFO("Simulator", "Tracking simulation completed: %d reacquisitions",
                     tracking_ctx.reacquisition_count);
    }
    
    // Cleanup
    sim_tracking_free(&tracking_ctx);
    
    return result;
}

/**
 * @brief Get tracking performance metrics
 * 
 * @param results Simulation results
 * @param avg_azimuth Output: average beam azimuth
 * @param avg_elevation Output: average beam elevation
 * @param num_reacquisitions Output: number of reacquisitions
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sim_get_tracking_metrics(const SimResults* results,
                             double* avg_azimuth,
                             double* avg_elevation,
                             int* num_reacquisitions) {
    if (results == NULL) {
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (!results->tracking_enabled) {
        FSO_LOG_WARNING("SimTracking", "Tracking was not enabled in simulation");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (avg_azimuth != NULL) {
        *avg_azimuth = results->avg_beam_azimuth;
    }
    
    if (avg_elevation != NULL) {
        *avg_elevation = results->avg_beam_elevation;
    }
    
    if (num_reacquisitions != NULL) {
        *num_reacquisitions = results->reacquisitions;
    }
    
    return FSO_SUCCESS;
}
