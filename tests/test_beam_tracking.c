/**
 * @file test_beam_tracking.c
 * @brief Test suite for beam tracking algorithms
 */

#include "../src/beam_tracking/beam_tracking.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

/* Test configuration */
#define TEST_TOLERANCE 1e-6
#define TEST_MAP_SIZE 21
#define TEST_MAP_RANGE (0.2)  // ±0.1 radians

/* Global test state */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  [PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  [FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_NEAR(actual, expected, tolerance, message) \
    do { \
        double diff = fabs((actual) - (expected)); \
        if (diff < (tolerance)) { \
            tests_passed++; \
            printf("  [PASS] %s (%.6f ≈ %.6f)\n", message, actual, expected); \
        } else { \
            tests_failed++; \
            printf("  [FAIL] %s (%.6f != %.6f, diff=%.6f)\n", \
                   message, actual, expected, diff); \
        } \
    } while(0)

/* ============================================================================
 * Mock Signal Strength Function
 * ============================================================================ */

/**
 * @brief Synthetic signal strength function for testing
 * 
 * Creates a Gaussian-shaped signal peak centered at (0, 0) with
 * maximum strength of 1.0.
 */
double mock_signal_strength(double azimuth, double elevation, void* user_data) {
    (void)user_data;  // Unused
    
    // Gaussian peak centered at origin
    double sigma = 0.05;  // Width of the peak
    double az_term = azimuth * azimuth / (2.0 * sigma * sigma);
    double el_term = elevation * elevation / (2.0 * sigma * sigma);
    
    return exp(-(az_term + el_term));
}

/**
 * @brief Offset signal strength function for misalignment testing
 */
double mock_signal_strength_offset(double azimuth, double elevation, void* user_data) {
    (void)user_data;
    
    // Peak offset from origin
    double peak_az = 0.05;
    double peak_el = 0.03;
    double sigma = 0.05;
    
    double az_diff = azimuth - peak_az;
    double el_diff = elevation - peak_el;
    double az_term = az_diff * az_diff / (2.0 * sigma * sigma);
    double el_term = el_diff * el_diff / (2.0 * sigma * sigma);
    
    return exp(-(az_term + el_term));
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

void test_signal_map_creation(void) {
    printf("\n=== Test: Signal Map Creation ===\n");
    
    SignalMap* map = signal_map_create(TEST_MAP_SIZE, TEST_MAP_SIZE,
                                       TEST_MAP_RANGE, TEST_MAP_RANGE,
                                       0.0, 0.0);
    
    TEST_ASSERT(map != NULL, "Signal map created");
    TEST_ASSERT(map->azimuth_samples == TEST_MAP_SIZE, "Azimuth samples correct");
    TEST_ASSERT(map->elevation_samples == TEST_MAP_SIZE, "Elevation samples correct");
    TEST_ASSERT(map->data != NULL, "Map data allocated");
    
    signal_map_free(map);
}

void test_signal_map_operations(void) {
    printf("\n=== Test: Signal Map Operations ===\n");
    
    SignalMap* map = signal_map_create(11, 11, 0.2, 0.2, 0.0, 0.0);
    TEST_ASSERT(map != NULL, "Map created");
    
    // Test set and get
    int result = signal_map_set(map, 0.0, 0.0, 1.0);
    TEST_ASSERT(result == FSO_SUCCESS, "Set center value");
    
    double strength;
    result = signal_map_get(map, 0.0, 0.0, &strength);
    TEST_ASSERT(result == FSO_SUCCESS, "Get center value");
    TEST_ASSERT_NEAR(strength, 1.0, TEST_TOLERANCE, "Center value correct");
    
    // Test interpolation
    result = signal_map_set(map, 0.02, 0.0, 0.8);
    TEST_ASSERT(result == FSO_SUCCESS, "Set offset value");
    
    result = signal_map_get(map, 0.01, 0.0, &strength);
    TEST_ASSERT(result == FSO_SUCCESS, "Get interpolated value");
    TEST_ASSERT(strength > 0.8 && strength < 1.0, "Interpolation in range");
    
    signal_map_free(map);
}

void test_pid_controller(void) {
    printf("\n=== Test: PID Controller ===\n");
    
    PIDController* pid = pid_create(1.0, 0.1, 0.05, 100.0, 1.0);
    TEST_ASSERT(pid != NULL, "PID controller created");
    TEST_ASSERT_NEAR(pid->kp, 1.0, TEST_TOLERANCE, "Kp correct");
    TEST_ASSERT_NEAR(pid->ki, 0.1, TEST_TOLERANCE, "Ki correct");
    TEST_ASSERT_NEAR(pid->kd, 0.05, TEST_TOLERANCE, "Kd correct");
    
    // Test PID update
    double output_az, output_el;
    int result = pid_update(pid, 0.1, 0.05, &output_az, &output_el);
    TEST_ASSERT(result == FSO_SUCCESS, "PID update successful");
    TEST_ASSERT(output_az != 0.0, "Azimuth output non-zero");
    TEST_ASSERT(output_el != 0.0, "Elevation output non-zero");
    
    // Test reset
    pid_reset(pid);
    TEST_ASSERT_NEAR(pid->integral_az, 0.0, TEST_TOLERANCE, "Integral reset");
    
    pid_free(pid);
}

void test_beam_tracker_init(void) {
    printf("\n=== Test: Beam Tracker Initialization ===\n");
    
    BeamTracker tracker;
    int result = beam_track_init(&tracker, 0.0, 0.0,
                                 TEST_MAP_SIZE, TEST_MAP_SIZE,
                                 TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    TEST_ASSERT(result == FSO_SUCCESS, "Tracker initialized");
    TEST_ASSERT_NEAR(tracker.azimuth, 0.0, TEST_TOLERANCE, "Initial azimuth");
    TEST_ASSERT_NEAR(tracker.elevation, 0.0, TEST_TOLERANCE, "Initial elevation");
    TEST_ASSERT(tracker.strength_map != NULL, "Signal map created");
    TEST_ASSERT(tracker.pid != NULL, "PID controller created");
    
    beam_track_free(&tracker);
}

void test_gradient_estimation(void) {
    printf("\n=== Test: Gradient Estimation ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.0, 0.0,
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Populate map with synthetic data
    for (int i = 0; i < 11; i++) {
        for (int j = 0; j < 11; j++) {
            double az = -0.05 + i * 0.01;
            double el = -0.05 + j * 0.01;
            double strength = mock_signal_strength(az, el, NULL);
            signal_map_set(tracker.strength_map, az, el, strength);
        }
    }
    
    // Estimate gradient at origin (should be near zero)
    double grad_az, grad_el;
    int result = beam_track_estimate_gradient(&tracker, 0.01, &grad_az, &grad_el);
    TEST_ASSERT(result == FSO_SUCCESS, "Gradient estimation successful");
    TEST_ASSERT(fabs(grad_az) < 0.1, "Azimuth gradient small at peak");
    TEST_ASSERT(fabs(grad_el) < 0.1, "Elevation gradient small at peak");
    
    beam_track_free(&tracker);
}

void test_beam_scanning(void) {
    printf("\n=== Test: Beam Scanning ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.0, 0.0,
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Perform scan
    int result = beam_track_scan(&tracker, 0.2, 0.2, 0.02,
                                mock_signal_strength, NULL);
    TEST_ASSERT(result == FSO_SUCCESS, "Scan completed");
    TEST_ASSERT(tracker.scan_count == 1, "Scan count incremented");
    
    // Check that peak was found near origin
    TEST_ASSERT(fabs(tracker.azimuth) < 0.05, "Peak azimuth near origin");
    TEST_ASSERT(fabs(tracker.elevation) < 0.05, "Peak elevation near origin");
    TEST_ASSERT(tracker.signal_strength > 0.9, "Peak strength high");
    
    beam_track_free(&tracker);
}

void test_peak_finding(void) {
    printf("\n=== Test: Peak Finding ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.0, 0.0,
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Populate map
    beam_track_scan(&tracker, 0.2, 0.2, 0.02, mock_signal_strength, NULL);
    
    // Find peak
    double peak_az, peak_el, peak_strength;
    int result = beam_track_find_peak(&tracker, &peak_az, &peak_el, &peak_strength);
    TEST_ASSERT(result == FSO_SUCCESS, "Peak finding successful");
    TEST_ASSERT(fabs(peak_az) < 0.05, "Peak azimuth correct");
    TEST_ASSERT(fabs(peak_el) < 0.05, "Peak elevation correct");
    TEST_ASSERT(peak_strength > 0.9, "Peak strength correct");
    
    beam_track_free(&tracker);
}

void test_gradient_descent_update(void) {
    printf("\n=== Test: Gradient Descent Update ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.05, 0.03,  // Start offset from peak
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Populate map
    beam_track_scan(&tracker, 0.2, 0.2, 0.01, mock_signal_strength, NULL);
    
    // Reset position to offset
    tracker.azimuth = 0.05;
    tracker.elevation = 0.03;
    
    // Perform several gradient descent updates
    for (int i = 0; i < 20; i++) {
        double strength = mock_signal_strength(tracker.azimuth, tracker.elevation, NULL);
        beam_track_update(&tracker, strength);
    }
    
    // Should have moved closer to origin
    TEST_ASSERT(fabs(tracker.azimuth) < 0.05, "Moved toward peak in azimuth");
    TEST_ASSERT(fabs(tracker.elevation) < 0.03, "Moved toward peak in elevation");
    TEST_ASSERT(tracker.signal_strength > 0.8, "Signal strength improved");
    
    beam_track_free(&tracker);
}

void test_misalignment_detection(void) {
    printf("\n=== Test: Misalignment Detection ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.0, 0.0,
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Set threshold
    beam_track_set_threshold(&tracker, 0.5);
    TEST_ASSERT_NEAR(tracker.signal_threshold, 0.5, TEST_TOLERANCE, "Threshold set");
    
    // Test with strong signal (aligned)
    int misaligned = beam_track_check_misalignment(&tracker, 0.8);
    TEST_ASSERT(misaligned == 0, "Strong signal: aligned");
    TEST_ASSERT(tracker.misaligned == 0, "Misaligned flag clear");
    
    // Test with weak signal (misaligned)
    misaligned = beam_track_check_misalignment(&tracker, 0.3);
    TEST_ASSERT(misaligned == 1, "Weak signal: misaligned");
    TEST_ASSERT(tracker.misaligned == 1, "Misaligned flag set");
    
    beam_track_free(&tracker);
}

void test_calibration(void) {
    printf("\n=== Test: Calibration ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.1, 0.1,  // Start far from peak
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Perform calibration
    int result = beam_track_calibrate(&tracker, 0.3, 0.3, 0.03, 0.01,
                                     mock_signal_strength, NULL);
    TEST_ASSERT(result == FSO_SUCCESS, "Calibration successful");
    
    // Should have found peak near origin
    TEST_ASSERT(fabs(tracker.azimuth) < 0.05, "Calibrated azimuth near peak");
    TEST_ASSERT(fabs(tracker.elevation) < 0.05, "Calibrated elevation near peak");
    TEST_ASSERT(tracker.signal_strength > 0.9, "Calibrated signal strong");
    TEST_ASSERT(tracker.scan_count >= 2, "Multiple scans performed");
    
    beam_track_free(&tracker);
}

void test_reacquisition(void) {
    printf("\n=== Test: Reacquisition ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.0, 0.0,
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Simulate signal loss by moving far from peak
    tracker.azimuth = 0.15;
    tracker.elevation = 0.15;
    tracker.signal_strength = 0.01;
    tracker.misaligned = 1;
    
    // Perform reacquisition
    int result = beam_track_reacquire(&tracker, 0.4, 0.4, 0.02,
                                     mock_signal_strength, NULL);
    TEST_ASSERT(result == FSO_SUCCESS, "Reacquisition successful");
    
    // Should have found peak
    TEST_ASSERT(fabs(tracker.azimuth) < 0.05, "Reacquired azimuth");
    TEST_ASSERT(fabs(tracker.elevation) < 0.05, "Reacquired elevation");
    TEST_ASSERT(tracker.signal_strength > 0.8, "Reacquired signal");
    TEST_ASSERT(tracker.misaligned == 0, "Alignment restored");
    
    beam_track_free(&tracker);
}

void test_pid_tracking(void) {
    printf("\n=== Test: PID Tracking ===\n");
    
    BeamTracker tracker;
    beam_track_init(&tracker, 0.05, 0.03,
                   TEST_MAP_SIZE, TEST_MAP_SIZE,
                   TEST_MAP_RANGE, TEST_MAP_RANGE);
    
    // Configure PID
    beam_track_configure_pid(&tracker, 0.5, 0.05, 0.01, 100.0, 0.5);
    
    // Track toward origin
    double target_az = 0.0;
    double target_el = 0.0;
    
    for (int i = 0; i < 50; i++) {
        double strength = mock_signal_strength(tracker.azimuth, tracker.elevation, NULL);
        beam_track_pid_update(&tracker, target_az, target_el, strength);
    }
    
    // Should have converged to target
    TEST_ASSERT(fabs(tracker.azimuth - target_az) < 0.01, "PID azimuth converged");
    TEST_ASSERT(fabs(tracker.elevation - target_el) < 0.01, "PID elevation converged");
    
    beam_track_free(&tracker);
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("========================================\n");
    printf("Beam Tracking Test Suite\n");
    printf("========================================\n");
    
    // Set log level to reduce output during tests
    fso_set_log_level(LOG_WARNING);
    
    // Run tests
    test_signal_map_creation();
    test_signal_map_operations();
    test_pid_controller();
    test_beam_tracker_init();
    test_gradient_estimation();
    test_beam_scanning();
    test_peak_finding();
    test_gradient_descent_update();
    test_misalignment_detection();
    test_calibration();
    test_reacquisition();
    test_pid_tracking();
    
    // Print summary
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    printf("========================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
