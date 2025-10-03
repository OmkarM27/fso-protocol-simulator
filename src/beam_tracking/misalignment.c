/**
 * @file misalignment.c
 * @brief Implementation of misalignment detection and recovery
 */

#include "beam_tracking.h"
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Threshold Configuration
 * ============================================================================ */

int beam_track_set_threshold(BeamTracker* tracker, double threshold) {
    FSO_CHECK_NULL(tracker);
    
    if (threshold < 0.0 || threshold > 1.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid threshold: %.3f (must be 0.0-1.0)", threshold);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    tracker->signal_threshold = threshold;
    
    FSO_LOG_INFO("BeamTracking", "Set misalignment threshold: %.3f", threshold);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Misalignment Detection
 * ============================================================================ */

int beam_track_check_misalignment(BeamTracker* tracker, double measured_strength) {
    FSO_CHECK_NULL(tracker);
    
    if (measured_strength < 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid signal strength: %.3f", measured_strength);
        return 0;
    }
    
    // Update current signal strength
    tracker->signal_strength = measured_strength;
    
    // Check if signal is below threshold
    if (measured_strength < tracker->signal_threshold) {
        if (!tracker->misaligned) {
            // Transition to misaligned state
            tracker->misaligned = 1;
            FSO_LOG_WARNING("BeamTracking", "Misalignment detected: strength=%.3f < threshold=%.3f",
                          measured_strength, tracker->signal_threshold);
        }
        return 1;  // Misaligned
    } else {
        if (tracker->misaligned) {
            // Transition back to aligned state
            tracker->misaligned = 0;
            FSO_LOG_INFO("BeamTracking", "Alignment restored: strength=%.3f >= threshold=%.3f",
                        measured_strength, tracker->signal_threshold);
        }
        return 0;  // Aligned
    }
}

/* ============================================================================
 * Status Query
 * ============================================================================ */

int beam_track_get_status(const BeamTracker* tracker,
                          int* is_aligned,
                          int* is_converged,
                          int* is_reacquiring) {
    FSO_CHECK_NULL(tracker);
    
    if (is_aligned) {
        *is_aligned = !tracker->misaligned;
    }
    
    if (is_converged) {
        *is_converged = beam_track_is_converged(tracker);
    }
    
    if (is_reacquiring) {
        *is_reacquiring = tracker->reacquisition_mode;
    }
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Reacquisition
 * ============================================================================ */

int beam_track_reacquire(BeamTracker* tracker,
                         double az_search_range,
                         double el_search_range,
                         double resolution,
                         BeamScanCallback callback,
                         void* user_data) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(callback);
    
    if (az_search_range <= 0.0 || el_search_range <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid search range: az=%.3f, el=%.3f",
                     az_search_range, el_search_range);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (resolution <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid resolution: %.6f", resolution);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    FSO_LOG_INFO("BeamTracking", "Starting reacquisition: search_range=(%.3f, %.3f), res=%.6f",
                az_search_range, el_search_range, resolution);
    
    // Enter reacquisition mode
    tracker->reacquisition_mode = 1;
    
    // Reset PID controller to avoid accumulated errors
    if (tracker->pid) {
        pid_reset(tracker->pid);
    }
    
    // Reset convergence tracking
    tracker->convergence_count = 0;
    
    // Perform full scan to find signal
    int result = beam_track_scan(tracker, az_search_range, el_search_range,
                                 resolution, callback, user_data);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("BeamTracking", "Reacquisition scan failed");
        tracker->reacquisition_mode = 0;
        return result;
    }
    
    // Check if we found a signal above threshold
    if (tracker->signal_strength < tracker->signal_threshold) {
        FSO_LOG_WARNING("BeamTracking", "Reacquisition failed: peak strength %.3f < threshold %.3f",
                       tracker->signal_strength, tracker->signal_threshold);
        tracker->reacquisition_mode = 0;
        return FSO_ERROR_CONVERGENCE;
    }
    
    // Signal found, exit reacquisition mode
    tracker->reacquisition_mode = 0;
    tracker->misaligned = 0;
    
    FSO_LOG_INFO("BeamTracking", "Reacquisition successful: az=%.3f, el=%.3f, strength=%.3f",
                tracker->azimuth, tracker->elevation, tracker->signal_strength);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Initial Calibration
 * ============================================================================ */

int beam_track_calibrate(BeamTracker* tracker,
                         double az_range,
                         double el_range,
                         double coarse_resolution,
                         double fine_resolution,
                         BeamScanCallback callback,
                         void* user_data) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(callback);
    
    if (az_range <= 0.0 || el_range <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid calibration range: az=%.3f, el=%.3f",
                     az_range, el_range);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (coarse_resolution <= 0.0 || fine_resolution <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid resolution: coarse=%.6f, fine=%.6f",
                     coarse_resolution, fine_resolution);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (fine_resolution >= coarse_resolution) {
        FSO_LOG_WARNING("BeamTracking", "Fine resolution (%.6f) should be smaller than coarse (%.6f)",
                       fine_resolution, coarse_resolution);
    }
    
    FSO_LOG_INFO("BeamTracking", "Starting calibration: range=(%.3f, %.3f), "
                "coarse_res=%.6f, fine_res=%.6f",
                az_range, el_range, coarse_resolution, fine_resolution);
    
    // Phase 1: Coarse scan over full range
    FSO_LOG_INFO("BeamTracking", "Phase 1: Coarse scan");
    int result = beam_track_scan(tracker, az_range, el_range,
                                 coarse_resolution, callback, user_data);
    if (result != FSO_SUCCESS) {
        FSO_LOG_ERROR("BeamTracking", "Coarse scan failed");
        return result;
    }
    
    // Store coarse peak position
    double coarse_az = tracker->azimuth;
    double coarse_el = tracker->elevation;
    double coarse_strength = tracker->signal_strength;
    
    FSO_LOG_INFO("BeamTracking", "Coarse peak: az=%.3f, el=%.3f, strength=%.3f",
                coarse_az, coarse_el, coarse_strength);
    
    // Phase 2: Fine scan around coarse peak
    // Use smaller range for fine scan (e.g., 2x coarse resolution on each side)
    double fine_az_range = coarse_resolution * 4.0;
    double fine_el_range = coarse_resolution * 4.0;
    
    FSO_LOG_INFO("BeamTracking", "Phase 2: Fine scan around peak");
    result = beam_track_scan(tracker, fine_az_range, fine_el_range,
                            fine_resolution, callback, user_data);
    if (result != FSO_SUCCESS) {
        FSO_LOG_WARNING("BeamTracking", "Fine scan failed, using coarse result");
        // Restore coarse peak position
        tracker->azimuth = coarse_az;
        tracker->elevation = coarse_el;
        tracker->signal_strength = coarse_strength;
    }
    
    // Check if calibration found acceptable signal
    if (tracker->signal_strength < tracker->signal_threshold) {
        FSO_LOG_WARNING("BeamTracking", "Calibration signal weak: %.3f < threshold %.3f",
                       tracker->signal_strength, tracker->signal_threshold);
        return FSO_ERROR_CONVERGENCE;
    }
    
    // Reset tracking state after calibration
    tracker->convergence_count = 0;
    tracker->misaligned = 0;
    tracker->reacquisition_mode = 0;
    
    if (tracker->pid) {
        pid_reset(tracker->pid);
    }
    
    FSO_LOG_INFO("BeamTracking", "Calibration complete: az=%.3f, el=%.3f, strength=%.3f",
                tracker->azimuth, tracker->elevation, tracker->signal_strength);
    
    return FSO_SUCCESS;
}
