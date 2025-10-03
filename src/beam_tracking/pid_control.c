/**
 * @file pid_control.c
 * @brief Implementation of PID feedback control for beam tracking
 */

#include "beam_tracking.h"
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * PID Configuration
 * ============================================================================ */

int beam_track_configure_pid(BeamTracker* tracker,
                             double kp, double ki, double kd,
                             double update_rate, double integral_limit) {
    FSO_CHECK_NULL(tracker);
    
    if (update_rate <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid PID update rate: %.3f Hz", update_rate);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // If PID controller doesn't exist, create it
    if (!tracker->pid) {
        tracker->pid = pid_create(kp, ki, kd, update_rate, integral_limit);
        if (!tracker->pid) {
            return FSO_ERROR_MEMORY;
        }
    } else {
        // Update existing PID parameters
        tracker->pid->kp = kp;
        tracker->pid->ki = ki;
        tracker->pid->kd = kd;
        tracker->pid->update_rate = update_rate;
        tracker->pid->dt = 1.0 / update_rate;
        tracker->pid->integral_limit = integral_limit;
        
        // Reset state when parameters change
        pid_reset(tracker->pid);
    }
    
    FSO_LOG_INFO("BeamTracking", "Configured PID: Kp=%.3f, Ki=%.3f, Kd=%.3f, rate=%.1f Hz",
                kp, ki, kd, update_rate);
    
    return FSO_SUCCESS;
}

int beam_track_reset_pid(BeamTracker* tracker) {
    FSO_CHECK_NULL(tracker);
    
    if (!tracker->pid) {
        FSO_LOG_WARNING("BeamTracking", "PID controller not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    pid_reset(tracker->pid);
    
    FSO_LOG_DEBUG("BeamTracking", "Reset PID controller state");
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * PID Feedback Control Update
 * ============================================================================ */

int beam_track_pid_update(BeamTracker* tracker,
                          double target_az,
                          double target_el,
                          double measured_strength) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(tracker->pid);
    FSO_CHECK_NULL(tracker->strength_map);
    
    if (measured_strength < 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid signal strength: %.3f", measured_strength);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Update current signal strength
    tracker->signal_strength = measured_strength;
    
    // Update signal strength map at current position
    int result = signal_map_set(tracker->strength_map,
                               tracker->azimuth,
                               tracker->elevation,
                               measured_strength);
    if (result != FSO_SUCCESS) {
        FSO_LOG_WARNING("BeamTracking", "Failed to update signal map");
    }
    
    // Calculate position error
    double error_az = target_az - tracker->azimuth;
    double error_el = target_el - tracker->elevation;
    
    // Compute PID control output
    double control_az, control_el;
    result = pid_update(tracker->pid, error_az, error_el, &control_az, &control_el);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("BeamTracking", "PID update failed");
        return result;
    }
    
    // Apply control output to beam position
    tracker->azimuth += control_az;
    tracker->elevation += control_el;
    
    // Calculate position change magnitude for convergence check
    double position_change = sqrt(control_az * control_az + control_el * control_el);
    
    // Update convergence tracking
    if (position_change < tracker->convergence_epsilon) {
        tracker->convergence_count++;
    } else {
        tracker->convergence_count = 0;
    }
    
    tracker->update_count++;
    
    FSO_LOG_DEBUG("BeamTracking", "PID update: pos=(%.6f, %.6f), target=(%.6f, %.6f), "
                 "error=(%.6f, %.6f), control=(%.6f, %.6f), strength=%.3f",
                 tracker->azimuth, tracker->elevation,
                 target_az, target_el,
                 error_az, error_el,
                 control_az, control_el,
                 tracker->signal_strength);
    
    return FSO_SUCCESS;
}
