/**
 * @file beam_tracking_demo.c
 * @brief Beam tracking demonstration
 * 
 * This example demonstrates beam tracking algorithms including gradient descent
 * and PID control for maintaining optical alignment.
 * 
 * Compile:
 *   gcc -I../src beam_tracking_demo.c -L../build -lfso -lm -lfftw3 -fopenmp -o beam_tracking_demo
 * 
 * Run:
 *   ./beam_tracking_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fso.h"
#include "beam_tracking/beam_tracking.h"

// Simulated signal strength function
// Creates a Gaussian peak at target position with some noise
double simulate_signal_strength(double azimuth, double elevation, void* user_data) {
    // Target position (passed as user_data)
    double* target = (double*)user_data;
    double target_az = target[0];
    double target_el = target[1];
    
    // Gaussian beam profile
    double sigma = 0.01;  // Beam width (radians)
    double az_diff = azimuth - target_az;
    double el_diff = elevation - target_el;
    double distance_sq = az_diff * az_diff + el_diff * el_diff;
    
    // Signal strength (Gaussian)
    double signal = exp(-distance_sq / (2.0 * sigma * sigma));
    
    // Add some noise
    double noise = fso_random_gaussian(0.0, 0.05);
    signal += noise;
    
    // Clamp to [0, 1]
    if (signal < 0.0) signal = 0.0;
    if (signal > 1.0) signal = 1.0;
    
    return signal;
}

void print_tracker_status(const BeamTracker* tracker) {
    printf("  Position: az=%.4f rad (%.2f°), el=%.4f rad (%.2f°)\n",
           tracker->azimuth, tracker->azimuth * 180.0 / FSO_PI,
           tracker->elevation, tracker->elevation * 180.0 / FSO_PI);
    printf("  Signal strength: %.4f\n", tracker->signal_strength);
    printf("  Step size: %.6f\n", tracker->step_size);
    printf("  Convergence count: %d\n", tracker->convergence_count);
}

int main(void) {
    printf("=== Beam Tracking Demonstration ===\n\n");
    
    // Set log level
    fso_set_log_level(LOG_INFO);
    
    // Initialize random number generator
    fso_random_init(0);
    
    // ========================================================================
    // Scenario 1: Gradient Descent Tracking
    // ========================================================================
    printf("--- Scenario 1: Gradient Descent Tracking ---\n\n");
    
    // Target beam position
    double target_position[2] = {0.05, 0.03};  // 2.86°, 1.72°
    printf("Target position: az=%.4f rad (%.2f°), el=%.4f rad (%.2f°)\n\n",
           target_position[0], target_position[0] * 180.0 / FSO_PI,
           target_position[1], target_position[1] * 180.0 / FSO_PI);
    
    // Initialize beam tracker with initial misalignment
    BeamTracker tracker;
    double initial_az = 0.0;
    double initial_el = 0.0;
    
    printf("Initializing beam tracker...\n");
    if (beam_track_init(&tracker, initial_az, initial_el, 
                        21, 21,  // 21x21 signal map
                        0.1, 0.1) != FSO_SUCCESS) {  // ±0.1 rad range
        fprintf(stderr, "Failed to initialize beam tracker\n");
        return 1;
    }
    
    printf("Initial state:\n");
    print_tracker_status(&tracker);
    printf("\n");
    
    // Perform gradient descent tracking
    printf("Performing gradient descent tracking...\n");
    int max_iterations = 100;
    
    for (int iter = 0; iter < max_iterations; iter++) {
        // Measure signal strength at current position
        double signal = simulate_signal_strength(tracker.azimuth, 
                                                 tracker.elevation,
                                                 target_position);
        
        // Update tracker
        if (beam_track_update(&tracker, signal) != FSO_SUCCESS) {
            fprintf(stderr, "Tracking update failed\n");
            break;
        }
        
        // Print progress every 10 iterations
        if ((iter + 1) % 10 == 0) {
            printf("Iteration %d:\n", iter + 1);
            print_tracker_status(&tracker);
            printf("\n");
        }
        
        // Check convergence
        if (beam_track_is_converged(&tracker)) {
            printf("Converged after %d iterations!\n", iter + 1);
            break;
        }
    }
    
    printf("Final state:\n");
    print_tracker_status(&tracker);
    
    // Calculate tracking error
    double az_error = fabs(tracker.azimuth - target_position[0]);
    double el_error = fabs(tracker.elevation - target_position[1]);
    double total_error = sqrt(az_error * az_error + el_error * el_error);
    
    printf("\nTracking error:\n");
    printf("  Azimuth error: %.6f rad (%.3f°)\n", 
           az_error, az_error * 180.0 / FSO_PI);
    printf("  Elevation error: %.6f rad (%.3f°)\n", 
           el_error, el_error * 180.0 / FSO_PI);
    printf("  Total error: %.6f rad (%.3f°)\n", 
           total_error, total_error * 180.0 / FSO_PI);
    
    beam_track_free(&tracker);
    
    // ========================================================================
    // Scenario 2: Beam Scanning and Reacquisition
    // ========================================================================
    printf("\n\n--- Scenario 2: Beam Scanning and Reacquisition ---\n\n");
    
    // New target position
    target_position[0] = -0.03;
    target_position[1] = 0.04;
    printf("Target position: az=%.4f rad (%.2f°), el=%.4f rad (%.2f°)\n\n",
           target_position[0], target_position[0] * 180.0 / FSO_PI,
           target_position[1], target_position[1] * 180.0 / FSO_PI);
    
    // Initialize tracker at wrong position (simulating signal loss)
    if (beam_track_init(&tracker, 0.0, 0.0, 21, 21, 0.1, 0.1) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize beam tracker\n");
        return 1;
    }
    
    // Set signal threshold for misalignment detection
    beam_track_set_threshold(&tracker, 0.3);
    
    printf("Performing beam scan to find signal...\n");
    
    // Perform full scan
    if (beam_track_scan(&tracker, 0.1, 0.1, 0.01,
                        simulate_signal_strength,
                        target_position) != FSO_SUCCESS) {
        fprintf(stderr, "Beam scan failed\n");
        beam_track_free(&tracker);
        return 1;
    }
    
    printf("Scan complete. Finding peak signal...\n");
    
    // Find peak in signal map
    double peak_az, peak_el, peak_strength;
    if (beam_track_find_peak(&tracker, &peak_az, &peak_el, 
                             &peak_strength) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to find peak\n");
        beam_track_free(&tracker);
        return 1;
    }
    
    printf("Peak found:\n");
    printf("  Position: az=%.4f rad (%.2f°), el=%.4f rad (%.2f°)\n",
           peak_az, peak_az * 180.0 / FSO_PI,
           peak_el, peak_el * 180.0 / FSO_PI);
    printf("  Signal strength: %.4f\n", peak_strength);
    
    // Move to peak position
    tracker.azimuth = peak_az;
    tracker.elevation = peak_el;
    tracker.signal_strength = peak_strength;
    
    // Calculate reacquisition error
    az_error = fabs(peak_az - target_position[0]);
    el_error = fabs(peak_el - target_position[1]);
    total_error = sqrt(az_error * az_error + el_error * el_error);
    
    printf("\nReacquisition error:\n");
    printf("  Azimuth error: %.6f rad (%.3f°)\n", 
           az_error, az_error * 180.0 / FSO_PI);
    printf("  Elevation error: %.6f rad (%.3f°)\n", 
           el_error, el_error * 180.0 / FSO_PI);
    printf("  Total error: %.6f rad (%.3f°)\n", 
           total_error, total_error * 180.0 / FSO_PI);
    
    beam_track_free(&tracker);
    
    // ========================================================================
    // Scenario 3: PID Feedback Control
    // ========================================================================
    printf("\n\n--- Scenario 3: PID Feedback Control ---\n\n");
    
    // Target position
    target_position[0] = 0.02;
    target_position[1] = -0.02;
    printf("Target position: az=%.4f rad (%.2f°), el=%.4f rad (%.2f°)\n\n",
           target_position[0], target_position[0] * 180.0 / FSO_PI,
           target_position[1], target_position[1] * 180.0 / FSO_PI);
    
    // Initialize tracker
    if (beam_track_init(&tracker, 0.0, 0.0, 21, 21, 0.1, 0.1) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to initialize beam tracker\n");
        return 1;
    }
    
    // Configure PID controller
    printf("Configuring PID controller...\n");
    printf("  Kp = 1.0, Ki = 0.2, Kd = 0.05\n");
    printf("  Update rate = 100 Hz\n\n");
    
    if (beam_track_configure_pid(&tracker, 1.0, 0.2, 0.05, 
                                 100.0, 0.1) != FSO_SUCCESS) {
        fprintf(stderr, "Failed to configure PID\n");
        beam_track_free(&tracker);
        return 1;
    }
    
    printf("Performing PID tracking...\n");
    int num_updates = 50;
    
    for (int i = 0; i < num_updates; i++) {
        // Measure signal strength
        double signal = simulate_signal_strength(tracker.azimuth,
                                                 tracker.elevation,
                                                 target_position);
        
        // PID update
        if (beam_track_pid_update(&tracker, target_position[0],
                                  target_position[1], signal) != FSO_SUCCESS) {
            fprintf(stderr, "PID update failed\n");
            break;
        }
        
        // Print progress every 10 updates
        if ((i + 1) % 10 == 0) {
            printf("Update %d:\n", i + 1);
            print_tracker_status(&tracker);
            printf("\n");
        }
    }
    
    printf("Final state:\n");
    print_tracker_status(&tracker);
    
    // Calculate tracking error
    az_error = fabs(tracker.azimuth - target_position[0]);
    el_error = fabs(tracker.elevation - target_position[1]);
    total_error = sqrt(az_error * az_error + el_error * el_error);
    
    printf("\nTracking error:\n");
    printf("  Azimuth error: %.6f rad (%.3f°)\n", 
           az_error, az_error * 180.0 / FSO_PI);
    printf("  Elevation error: %.6f rad (%.3f°)\n", 
           el_error, el_error * 180.0 / FSO_PI);
    printf("  Total error: %.6f rad (%.3f°)\n", 
           total_error, total_error * 180.0 / FSO_PI);
    
    beam_track_free(&tracker);
    
    // ========================================================================
    // Summary
    // ========================================================================
    printf("\n\n=== Summary ===\n\n");
    printf("This demonstration showed three beam tracking scenarios:\n\n");
    
    printf("1. Gradient Descent:\n");
    printf("   - Iteratively moves toward peak signal\n");
    printf("   - Uses momentum for smooth convergence\n");
    printf("   - Adaptive step size for efficiency\n\n");
    
    printf("2. Beam Scanning:\n");
    printf("   - Scans angular space to build signal map\n");
    printf("   - Finds peak signal position\n");
    printf("   - Used for initial acquisition or reacquisition\n\n");
    
    printf("3. PID Control:\n");
    printf("   - Smooth tracking with feedback control\n");
    printf("   - Disturbance rejection\n");
    printf("   - Zero steady-state error\n\n");
    
    printf("In practice, these methods are often combined:\n");
    printf("- Use scanning for initial acquisition\n");
    printf("- Use gradient descent for coarse tracking\n");
    printf("- Use PID for fine tracking and disturbance rejection\n\n");
    
    printf("=== Demonstration Complete ===\n");
    
    return 0;
}
