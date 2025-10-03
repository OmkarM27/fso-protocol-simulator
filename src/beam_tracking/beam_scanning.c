/**
 * @file beam_scanning.c
 * @brief Implementation of beam scanning and signal mapping
 */

#include "beam_tracking.h"
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Signal Map Update
 * ============================================================================ */

int beam_track_update_map(BeamTracker* tracker,
                          double azimuth,
                          double elevation,
                          double strength) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(tracker->strength_map);
    
    if (strength < 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid signal strength: %.3f", strength);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Update signal strength map
    int result = signal_map_set(tracker->strength_map, azimuth, elevation, strength);
    if (result != FSO_SUCCESS) {
        FSO_LOG_WARNING("BeamTracking", "Failed to update map at az=%.3f, el=%.3f",
                       azimuth, elevation);
        return result;
    }
    
    FSO_LOG_DEBUG("BeamTracking", "Updated map: az=%.3f, el=%.3f, strength=%.3f",
                 azimuth, elevation, strength);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Peak Finding
 * ============================================================================ */

int beam_track_find_peak(const BeamTracker* tracker,
                         double* peak_az,
                         double* peak_el,
                         double* peak_strength) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(tracker->strength_map);
    FSO_CHECK_NULL(peak_az);
    FSO_CHECK_NULL(peak_el);
    FSO_CHECK_NULL(peak_strength);
    
    const SignalMap* map = tracker->strength_map;
    
    // Initialize with first point
    *peak_strength = map->data[0];
    *peak_az = map->azimuth_min;
    *peak_el = map->elevation_min;
    
    // Search for maximum signal strength
    for (size_t el_idx = 0; el_idx < map->elevation_samples; el_idx++) {
        for (size_t az_idx = 0; az_idx < map->azimuth_samples; az_idx++) {
            size_t index = el_idx * map->azimuth_samples + az_idx;
            double strength = map->data[index];
            
            if (strength > *peak_strength) {
                *peak_strength = strength;
                *peak_az = map->azimuth_min + az_idx * map->azimuth_resolution;
                *peak_el = map->elevation_min + el_idx * map->elevation_resolution;
            }
        }
    }
    
    FSO_LOG_INFO("BeamTracking", "Found peak: az=%.3f, el=%.3f, strength=%.3f",
                *peak_az, *peak_el, *peak_strength);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Beam Scanning
 * ============================================================================ */

int beam_track_scan(BeamTracker* tracker,
                    double az_range,
                    double el_range,
                    double resolution,
                    BeamScanCallback callback,
                    void* user_data) {
    FSO_CHECK_NULL(tracker);
    FSO_CHECK_NULL(tracker->strength_map);
    FSO_CHECK_NULL(callback);
    
    if (az_range <= 0.0 || el_range <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid scan range: az=%.3f, el=%.3f",
                     az_range, el_range);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (resolution <= 0.0) {
        FSO_LOG_ERROR("BeamTracking", "Invalid scan resolution: %.6f", resolution);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Calculate scan bounds centered on current position
    double az_min = tracker->azimuth - az_range / 2.0;
    double az_max = tracker->azimuth + az_range / 2.0;
    double el_min = tracker->elevation - el_range / 2.0;
    double el_max = tracker->elevation + el_range / 2.0;
    
    // Calculate number of scan points
    int az_points = (int)ceil(az_range / resolution) + 1;
    int el_points = (int)ceil(el_range / resolution) + 1;
    
    FSO_LOG_INFO("BeamTracking", "Starting beam scan: %dx%d points, "
                "az=[%.3f, %.3f], el=[%.3f, %.3f], res=%.6f",
                az_points, el_points, az_min, az_max, el_min, el_max, resolution);
    
    // Clear existing map data
    signal_map_clear(tracker->strength_map);
    
    // Perform scan
    int points_scanned = 0;
    for (int el_idx = 0; el_idx < el_points; el_idx++) {
        double elevation = el_min + el_idx * resolution;
        
        for (int az_idx = 0; az_idx < az_points; az_idx++) {
            double azimuth = az_min + az_idx * resolution;
            
            // Measure signal strength at this position using callback
            double strength = callback(azimuth, elevation, user_data);
            
            // Update signal map
            int result = signal_map_set(tracker->strength_map, azimuth, elevation, strength);
            if (result == FSO_SUCCESS) {
                points_scanned++;
            }
            
            FSO_LOG_DEBUG("BeamTracking", "Scan point [%d,%d]: az=%.3f, el=%.3f, strength=%.3f",
                         az_idx, el_idx, azimuth, elevation, strength);
        }
    }
    
    tracker->scan_count++;
    
    FSO_LOG_INFO("BeamTracking", "Scan complete: %d points scanned (scan #%d)",
                points_scanned, tracker->scan_count);
    
    // Find peak in scanned data
    double peak_az, peak_el, peak_strength;
    int result = beam_track_find_peak(tracker, &peak_az, &peak_el, &peak_strength);
    if (result == FSO_SUCCESS) {
        // Update tracker position to peak
        tracker->azimuth = peak_az;
        tracker->elevation = peak_el;
        tracker->signal_strength = peak_strength;
        
        FSO_LOG_INFO("BeamTracking", "Updated position to peak: az=%.3f, el=%.3f, strength=%.3f",
                    peak_az, peak_el, peak_strength);
    }
    
    return FSO_SUCCESS;
}
