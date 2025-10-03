/**
 * @file beam_tracking.c
 * @brief Implementation of beam tracking initialization and basic operations
 */

#include "beam_tracking.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Signal Map Operations
 * ============================================================================ */

SignalMap* signal_map_create(size_t azimuth_samples,
                             size_t elevation_samples,
                             double azimuth_range,
                             double elevation_range,
                             double center_az,
                             double center_el) {
    // Validate parameters
    if (azimuth_samples == 0 || elevation_samples == 0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid map dimensions: az=%zu, el=%zu",
                     azimuth_samples, elevation_samples);
        return NULL;
    }
    
    if (azimuth_range <= 0.0 || elevation_range <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid map range: az_range=%.3f, el_range=%.3f",
                     azimuth_range, elevation_range);
        return NULL;
    }
    
    // Allocate map structure
    SignalMap* map = (SignalMap*)malloc(sizeof(SignalMap));
    if (!map) {
        FSO_LOG_ERROR("BeamTracking", "Failed to allocate SignalMap");
        return NULL;
    }
    
    // Allocate data array
    size_t total_samples = azimuth_samples * elevation_samples;
    map->data = (double*)calloc(total_samples, sizeof(double));
    if (!map->data) {
        FSO_LOG_ERROR("BeamTracking", "Failed to allocate map data (%zu samples)", total_samples);
        free(map);
        return NULL;
    }
    
    // Initialize map parameters
    map->azimuth_samples = azimuth_samples;
    map->elevation_samples = elevation_samples;
    map->azimuth_min = center_az - azimuth_range / 2.0;
    map->azimuth_max = center_az + azimuth_range / 2.0;
    map->elevation_min = center_el - elevation_range / 2.0;
    map->elevation_max = center_el + elevation_range / 2.0;
    map->azimuth_resolution = azimuth_range / (azimuth_samples - 1);
    map->elevation_resolution = elevation_range / (elevation_samples - 1);
    
    FSO_LOG_DEBUG("BeamTracking", "Created signal map: %zux%zu samples, "
                 "az=[%.3f, %.3f], el=[%.3f, %.3f]",
                 azimuth_samples, elevation_samples,
                 map->azimuth_min, map->azimuth_max,
                 map->elevation_min, map->elevation_max);
    
    return map;
}

void signal_map_free(SignalMap* map) {
    if (map) {
        if (map->data) {
            free(map->data);
        }
        free(map);
    }
}

int signal_map_set(SignalMap* map, double azimuth, double elevation, double strength) {
    FSO_CHECK_NULL(map);
    FSO_CHECK_NULL(map->data);
    
    // Check if angle is within map bounds
    if (azimuth < map->azimuth_min || azimuth > map->azimuth_max ||
        elevation < map->elevation_min || elevation > map->elevation_max) {
        FSO_LOG_WARNING("BeamTracking", "Angle out of map bounds: az=%.3f, el=%.3f",
                       azimuth, elevation);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Find nearest grid point
    int az_idx = (int)round((azimuth - map->azimuth_min) / map->azimuth_resolution);
    int el_idx = (int)round((elevation - map->elevation_min) / map->elevation_resolution);
    
    // Clamp to valid indices
    az_idx = FSO_CLAMP(az_idx, 0, (int)map->azimuth_samples - 1);
    el_idx = FSO_CLAMP(el_idx, 0, (int)map->elevation_samples - 1);
    
    // Set value in flattened array
    size_t index = el_idx * map->azimuth_samples + az_idx;
    map->data[index] = strength;
    
    return FSO_SUCCESS;
}

int signal_map_get(const SignalMap* map, double azimuth, double elevation, double* strength) {
    FSO_CHECK_NULL(map);
    FSO_CHECK_NULL(map->data);
    FSO_CHECK_NULL(strength);
    
    // Check if angle is within map bounds
    if (azimuth < map->azimuth_min || azimuth > map->azimuth_max ||
        elevation < map->elevation_min || elevation > map->elevation_max) {
        *strength = 0.0;
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Calculate continuous indices
    double az_idx_f = (azimuth - map->azimuth_min) / map->azimuth_resolution;
    double el_idx_f = (elevation - map->elevation_min) / map->elevation_resolution;
    
    // Get integer and fractional parts for bilinear interpolation
    int az_idx0 = (int)floor(az_idx_f);
    int el_idx0 = (int)floor(el_idx_f);
    int az_idx1 = az_idx0 + 1;
    int el_idx1 = el_idx0 + 1;
    
    double az_frac = az_idx_f - az_idx0;
    double el_frac = el_idx_f - el_idx0;
    
    // Clamp indices to valid range
    az_idx0 = FSO_CLAMP(az_idx0, 0, (int)map->azimuth_samples - 1);
    az_idx1 = FSO_CLAMP(az_idx1, 0, (int)map->azimuth_samples - 1);
    el_idx0 = FSO_CLAMP(el_idx0, 0, (int)map->elevation_samples - 1);
    el_idx1 = FSO_CLAMP(el_idx1, 0, (int)map->elevation_samples - 1);
    
    // Get four corner values
    double v00 = map->data[el_idx0 * map->azimuth_samples + az_idx0];
    double v10 = map->data[el_idx0 * map->azimuth_samples + az_idx1];
    double v01 = map->data[el_idx1 * map->azimuth_samples + az_idx0];
    double v11 = map->data[el_idx1 * map->azimuth_samples + az_idx1];
    
    // Bilinear interpolation
    double v0 = v00 * (1.0 - az_frac) + v10 * az_frac;
    double v1 = v01 * (1.0 - az_frac) + v11 * az_frac;
    *strength = v0 * (1.0 - el_frac) + v1 * el_frac;
    
    return FSO_SUCCESS;
}

void signal_map_clear(SignalMap* map) {
    if (map && map->data) {
        size_t total_samples = map->azimuth_samples * map->elevation_samples;
        memset(map->data, 0, total_samples * sizeof(double));
    }
}

/* ============================================================================
 * PID Controller Operations
 * ============================================================================ */

PIDController* pid_create(double kp, double ki, double kd, 
                          double update_rate, double integral_limit) {
    // Validate parameters
    if (update_rate <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid update rate: %.3f Hz", update_rate);
        return NULL;
    }
    
    // Allocate controller
    PIDController* pid = (PIDController*)malloc(sizeof(PIDController));
    if (!pid) {
        FSO_LOG_ERROR("BeamTracking", "Failed to allocate PIDController");
        return NULL;
    }
    
    // Initialize parameters
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral_az = 0.0;
    pid->integral_el = 0.0;
    pid->prev_error_az = 0.0;
    pid->prev_error_el = 0.0;
    pid->integral_limit = integral_limit;
    pid->update_rate = update_rate;
    pid->dt = 1.0 / update_rate;
    
    FSO_LOG_DEBUG("BeamTracking", "Created PID controller: Kp=%.3f, Ki=%.3f, Kd=%.3f, "
                 "rate=%.1f Hz", kp, ki, kd, update_rate);
    
    return pid;
}

void pid_free(PIDController* pid) {
    if (pid) {
        free(pid);
    }
}

void pid_reset(PIDController* pid) {
    if (pid) {
        pid->integral_az = 0.0;
        pid->integral_el = 0.0;
        pid->prev_error_az = 0.0;
        pid->prev_error_el = 0.0;
    }
}

int pid_update(PIDController* pid, 
               double error_az, double error_el,
               double* output_az, double* output_el) {
    FSO_CHECK_NULL(pid);
    FSO_CHECK_NULL(output_az);
    FSO_CHECK_NULL(output_el);
    
    // Update integral terms with anti-windup
    pid->integral_az += error_az * pid->dt;
    pid->integral_el += error_el * pid->dt;
    
    // Apply anti-windup limits
    pid->integral_az = FSO_CLAMP(pid->integral_az, -pid->integral_limit, pid->integral_limit);
    pid->integral_el = FSO_CLAMP(pid->integral_el, -pid->integral_limit, pid->integral_limit);
    
    // Calculate derivative terms
    double derivative_az = (error_az - pid->prev_error_az) / pid->dt;
    double derivative_el = (error_el - pid->prev_error_el) / pid->dt;
    
    // Compute PID output
    *output_az = pid->kp * error_az + pid->ki * pid->integral_az + pid->kd * derivative_az;
    *output_el = pid->kp * error_el + pid->ki * pid->integral_el + pid->kd * derivative_el;
    
    // Store current error for next iteration
    pid->prev_error_az = error_az;
    pid->prev_error_el = error_el;
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Beam Tracker Initialization
 * ============================================================================ */

int beam_track_init(BeamTracker* tracker, 
                    double initial_az, 
                    double initial_el,
                    size_t map_az_samples,
                    size_t map_el_samples,
                    double map_az_range,
                    double map_el_range) {
    FSO_CHECK_NULL(tracker);
    
    // Validate parameters
    if (map_az_samples < 2 || map_el_samples < 2) {
        FSO_LOG_ERROR("BeamTracking", "Map dimensions too small: %zux%zu",
                     map_az_samples, map_el_samples);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (map_az_range <= 0.0 || map_el_range <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid map range: az=%.3f, el=%.3f",
                     map_az_range, map_el_range);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Initialize position
    tracker->azimuth = initial_az;
    tracker->elevation = initial_el;
    tracker->signal_strength = 0.0;
    
    // Initialize gradient descent parameters
    tracker->step_size = 0.01;  // Default step size (radians)
    tracker->momentum = 0.9;     // Default momentum coefficient
    tracker->velocity_az = 0.0;
    tracker->velocity_el = 0.0;
    
    // Initialize adaptive step size parameters
    tracker->step_size_min = 0.001;   // Minimum step size
    tracker->step_size_max = 0.1;     // Maximum step size
    tracker->step_adapt_factor = 1.1; // Adaptation factor
    
    // Initialize convergence tracking
    tracker->convergence_count = 0;
    tracker->convergence_threshold = 10;  // Iterations to declare convergence
    tracker->convergence_epsilon = 1e-4;  // Minimum change threshold
    
    // Create signal strength map
    tracker->strength_map = signal_map_create(map_az_samples, map_el_samples,
                                             map_az_range, map_el_range,
                                             initial_az, initial_el);
    if (!tracker->strength_map) {
        FSO_LOG_ERROR("BeamTracking", "Failed to create signal map");
        return FSO_ERROR_MEMORY;
    }
    
    // Create PID controller with default parameters
    // Default: Kp=1.0, Ki=0.1, Kd=0.05, rate=100Hz, limit=1.0
    tracker->pid = pid_create(1.0, 0.1, 0.05, 100.0, 1.0);
    if (!tracker->pid) {
        FSO_LOG_ERROR("BeamTracking", "Failed to create PID controller");
        signal_map_free(tracker->strength_map);
        return FSO_ERROR_MEMORY;
    }
    
    // Initialize misalignment detection
    tracker->signal_threshold = 0.1;  // Default threshold (10% of max)
    tracker->misaligned = 0;
    tracker->reacquisition_mode = 0;
    
    // Initialize statistics
    tracker->update_count = 0;
    tracker->scan_count = 0;
    
    FSO_LOG_INFO("BeamTracking", "Initialized beam tracker at az=%.3f, el=%.3f",
                initial_az, initial_el);
    
    return FSO_SUCCESS;
}

void beam_track_free(BeamTracker* tracker) {
    if (tracker) {
        if (tracker->strength_map) {
            signal_map_free(tracker->strength_map);
            tracker->strength_map = NULL;
        }
        if (tracker->pid) {
            pid_free(tracker->pid);
            tracker->pid = NULL;
        }
    }
}
