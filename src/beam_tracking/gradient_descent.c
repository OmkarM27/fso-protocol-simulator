/**
 * @file gradient_descent.c
 * @brief Implementation of gradient descent beam tracking algorithm
 */

#include "beam_tracking.h"
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Gradient Estimation
 * ============================================================================ */

int beam_track_estimate_gradient(BeamTracker* tracker,
                                 double delta_angle,
                                 double* grad_az,
                                 double* grad_el) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(tracker->strength_map);
    FSO_CHECK_NULL(grad_az);
    FSO_CHECK_NULL(grad_el);
    
    if (delta_angle <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid delta angle: %.6f", delta_angle);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Get current signal strength
    double s_center;
    int result = signal_map_get(tracker->strength_map, 
                                tracker->azimuth, 
                                tracker->elevation, 
                                &s_center);
    if (result != FSO_SUCCESS) {
        // If current position not in map, use stored signal strength
        s_center = tracker->signal_strength;
    }
    
    // Estimate gradient in azimuth using central difference
    double s_az_plus, s_az_minus;
    result = signal_map_get(tracker->strength_map,
                           tracker->azimuth + delta_angle,
                           tracker->elevation,
                           &s_az_plus);
    if (result != FSO_SUCCESS) {
        s_az_plus = s_center;  // Fallback if out of bounds
    }
    
    result = signal_map_get(tracker->strength_map,
                           tracker->azimuth - delta_angle,
                           tracker->elevation,
                           &s_az_minus);
    if (result != FSO_SUCCESS) {
        s_az_minus = s_center;  // Fallback if out of bounds
    }
    
    *grad_az = (s_az_plus - s_az_minus) / (2.0 * delta_angle);
    
    // Estimate gradient in elevation using central difference
    double s_el_plus, s_el_minus;
    result = signal_map_get(tracker->strength_map,
                           tracker->azimuth,
                           tracker->elevation + delta_angle,
                           &s_el_plus);
    if (result != FSO_SUCCESS) {
        s_el_plus = s_center;  // Fallback if out of bounds
    }
    
    result = signal_map_get(tracker->strength_map,
                           tracker->azimuth,
                           tracker->elevation - delta_angle,
                           &s_el_minus);
    if (result != FSO_SUCCESS) {
        s_el_minus = s_center;  // Fallback if out of bounds
    }
    
    *grad_el = (s_el_plus - s_el_minus) / (2.0 * delta_angle);
    
    FSO_LOG_DEBUG("BeamTracking", "Gradient: az=%.6f, el=%.6f", *grad_az, *grad_el);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Adaptive Step Size
 * ============================================================================ */

void beam_track_adapt_step_size(BeamTracker* tracker, double improvement) {
    if (!tracker) {
        return;
    }
    
    // If signal strength improved, increase step size
    if (improvement > 0.0) {
        tracker->step_size *= tracker->step_adapt_factor;
        tracker->convergence_count = 0;  // Reset convergence counter
    } 
    // If signal strength decreased, reduce step size
    else if (improvement < -tracker->convergence_epsilon) {
        tracker->step_size /= tracker->step_adapt_factor;
        tracker->convergence_count = 0;  // Reset convergence counter
    }
    // If no significant change, increment convergence counter
    else {
        tracker->convergence_count++;
    }
    
    // Clamp step size to valid range
    tracker->step_size = FSO_CLAMP(tracker->step_size, 
                                   tracker->step_size_min, 
                                   tracker->step_size_max);
    
    FSO_LOG_DEBUG("BeamTracking", "Adapted step size: %.6f (improvement: %.6f)",
                 tracker->step_size, improvement);
}

/* ============================================================================
 * Convergence Check
 * ============================================================================ */

int beam_track_is_converged(const BeamTracker* tracker) {
    if (!tracker) {
        return 0;
    }
    
    return (tracker->convergence_count >= tracker->convergence_threshold);
}

/* ============================================================================
 * Gradient Descent Update
 * ============================================================================ */

int beam_track_update(BeamTracker* tracker, double measured_strength) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(tracker->strength_map);
    
    if (measured_strength < 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid signal strength: %.3f", measured_strength);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Store previous signal strength for improvement calculation
    double prev_strength = tracker->signal_strength;
    
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
    
    // Calculate improvement
    double improvement = measured_strength - prev_strength;
    
    // Adapt step size based on improvement
    beam_track_adapt_step_size(tracker, improvement);
    
    // Check if converged
    if (beam_track_is_converged(tracker)) {
        FSO_LOG_INFO("BeamTracking", "Beam tracking converged at az=%.3f, el=%.3f, strength=%.3f",
                    tracker->azimuth, tracker->elevation, tracker->signal_strength);
        tracker->update_count++;
        return FSO_SUCCESS;
    }
    
    // Estimate gradient using finite differences
    double grad_az, grad_el;
    double delta_angle = tracker->step_size * 0.5;  // Use half step size for gradient estimation
    result = beam_track_estimate_gradient(tracker, delta_angle, &grad_az, &grad_el);
    if (result != FSO_SUCCESS) {
        FSO_LOG_WARNING("BeamTracking", "Failed to estimate gradient");
        tracker->update_count++;
        return result;
    }
    
    // Calculate gradient magnitude
    double grad_magnitude = sqrt(grad_az * grad_az + grad_el * grad_el);
    
    // If gradient is too small, we might be at a local maximum
    if (grad_magnitude < 1e-6) {
        FSO_LOG_DEBUG("BeamTracking", "Gradient magnitude very small: %.9f", grad_magnitude);
        tracker->convergence_count++;
        tracker->update_count++;
        return FSO_SUCCESS;
    }
    
    // Update velocity with momentum term
    // v_new = β*v_old + α*∇S(θ)
    tracker->velocity_az = tracker->momentum * tracker->velocity_az + 
                          tracker->step_size * grad_az;
    tracker->velocity_el = tracker->momentum * tracker->velocity_el + 
                          tracker->step_size * grad_el;
    
    // Update beam position using velocity
    // θ_new = θ_old + v_new
    double new_azimuth = tracker->azimuth + tracker->velocity_az;
    double new_elevation = tracker->elevation + tracker->velocity_el;
    
    // Calculate position change magnitude
    double position_change = sqrt(tracker->velocity_az * tracker->velocity_az +
                                 tracker->velocity_el * tracker->velocity_el);
    
    // Check if position change is significant
    if (position_change < tracker->convergence_epsilon) {
        tracker->convergence_count++;
    } else {
        tracker->convergence_count = 0;
    }
    
    // Update position
    tracker->azimuth = new_azimuth;
    tracker->elevation = new_elevation;
    
    tracker->update_count++;
    
    FSO_LOG_DEBUG("BeamTracking", "Updated position: az=%.6f, el=%.6f, "
                 "grad=(%.6f, %.6f), vel=(%.6f, %.6f), strength=%.3f",
                 tracker->azimuth, tracker->elevation,
                 grad_az, grad_el,
                 tracker->velocity_az, tracker->velocity_el,
                 tracker->signal_strength);
    
    return FSO_SUCCESS;
}
