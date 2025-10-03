/**
 * @file beam_tracking.h
 * @brief Beam tracking and alignment algorithms for FSO communication
 * 
 * This module implements automated beam tracking algorithms to maintain
 * optical alignment between transmitter and receiver despite environmental
 * disturbances. Includes gradient descent optimization, PID feedback control,
 * signal strength mapping, and misalignment detection/recovery.
 */

#ifndef BEAM_TRACKING_H
#define BEAM_TRACKING_H

#include "../fso.h"
#include <stddef.h>

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief 2D signal strength map for beam tracking
 * 
 * Stores signal strength measurements across azimuth and elevation angles
 * to enable gradient-based beam optimization.
 */
typedef struct {
    double* data;              /**< Flattened 2D array of signal strengths */
    size_t azimuth_samples;    /**< Number of samples in azimuth dimension */
    size_t elevation_samples;  /**< Number of samples in elevation dimension */
    double azimuth_min;        /**< Minimum azimuth angle (radians) */
    double azimuth_max;        /**< Maximum azimuth angle (radians) */
    double elevation_min;      /**< Minimum elevation angle (radians) */
    double elevation_max;      /**< Maximum elevation angle (radians) */
    double azimuth_resolution; /**< Angular resolution in azimuth (radians) */
    double elevation_resolution; /**< Angular resolution in elevation (radians) */
} SignalMap;

/**
 * @brief PID controller state for beam tracking feedback control
 */
typedef struct {
    double kp;                 /**< Proportional gain */
    double ki;                 /**< Integral gain */
    double kd;                 /**< Derivative gain */
    double integral_az;        /**< Integral term for azimuth */
    double integral_el;        /**< Integral term for elevation */
    double prev_error_az;      /**< Previous error for azimuth (for derivative) */
    double prev_error_el;      /**< Previous error for elevation (for derivative) */
    double integral_limit;     /**< Anti-windup limit for integral term */
    double update_rate;        /**< Control loop update rate (Hz) */
    double dt;                 /**< Time step (1/update_rate) */
} PIDController;

/**
 * @brief Beam tracker state and configuration
 * 
 * Maintains current beam position, tracking parameters, signal strength map,
 * and control loop state for automated beam alignment.
 */
typedef struct {
    // Current beam position
    double azimuth;            /**< Current azimuth angle (radians) */
    double elevation;          /**< Current elevation angle (radians) */
    double signal_strength;    /**< Current received signal strength */
    
    // Gradient descent parameters
    double step_size;          /**< Gradient descent step size (learning rate) */
    double momentum;           /**< Momentum coefficient (0-1) */
    double velocity_az;        /**< Velocity for azimuth (momentum term) */
    double velocity_el;        /**< Velocity for elevation (momentum term) */
    
    // Adaptive step size parameters
    double step_size_min;      /**< Minimum step size */
    double step_size_max;      /**< Maximum step size */
    double step_adapt_factor;  /**< Step size adaptation factor */
    
    // Convergence tracking
    int convergence_count;     /**< Iterations since last significant update */
    int convergence_threshold; /**< Iterations required to declare convergence */
    double convergence_epsilon; /**< Minimum change to reset convergence counter */
    
    // Signal strength map
    SignalMap* strength_map;   /**< 2D signal strength map */
    
    // PID controller
    PIDController* pid;        /**< PID feedback controller */
    
    // Misalignment detection
    double signal_threshold;   /**< Minimum acceptable signal strength */
    int misaligned;            /**< Flag indicating misalignment detected */
    int reacquisition_mode;    /**< Flag indicating reacquisition in progress */
    
    // Statistics
    int update_count;          /**< Total number of tracking updates */
    int scan_count;            /**< Total number of full scans performed */
} BeamTracker;

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize beam tracker with initial position
 * 
 * Creates and initializes a beam tracker with specified initial beam position.
 * Allocates signal strength map and PID controller.
 * 
 * @param tracker Pointer to BeamTracker structure to initialize
 * @param initial_az Initial azimuth angle (radians)
 * @param initial_el Initial elevation angle (radians)
 * @param map_az_samples Number of azimuth samples in signal map
 * @param map_el_samples Number of elevation samples in signal map
 * @param map_az_range Angular range for azimuth map (radians, ±range/2)
 * @param map_el_range Angular range for elevation map (radians, ±range/2)
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.1: Initial calibration routines
 */
int beam_track_init(BeamTracker* tracker, 
                    double initial_az, 
                    double initial_el,
                    size_t map_az_samples,
                    size_t map_el_samples,
                    double map_az_range,
                    double map_el_range);

/**
 * @brief Free beam tracker resources
 * 
 * Releases all memory allocated for signal map and PID controller.
 * 
 * @param tracker Pointer to BeamTracker to free
 */
void beam_track_free(BeamTracker* tracker);

/* ============================================================================
 * Signal Strength Map Operations
 * ============================================================================ */

/**
 * @brief Create signal strength map
 * 
 * @param azimuth_samples Number of samples in azimuth
 * @param elevation_samples Number of samples in elevation
 * @param azimuth_range Angular range for azimuth (radians)
 * @param elevation_range Angular range for elevation (radians)
 * @param center_az Center azimuth angle (radians)
 * @param center_el Center elevation angle (radians)
 * @return Pointer to allocated SignalMap, or NULL on failure
 */
SignalMap* signal_map_create(size_t azimuth_samples,
                             size_t elevation_samples,
                             double azimuth_range,
                             double elevation_range,
                             double center_az,
                             double center_el);

/**
 * @brief Free signal strength map
 * 
 * @param map Pointer to SignalMap to free
 */
void signal_map_free(SignalMap* map);

/**
 * @brief Set signal strength value at specific angle
 * 
 * @param map Signal map
 * @param azimuth Azimuth angle (radians)
 * @param elevation Elevation angle (radians)
 * @param strength Signal strength value
 * @return FSO_SUCCESS on success, error code on failure
 */
int signal_map_set(SignalMap* map, double azimuth, double elevation, double strength);

/**
 * @brief Get signal strength value at specific angle (with interpolation)
 * 
 * Uses bilinear interpolation for smooth gradient estimation.
 * 
 * @param map Signal map
 * @param azimuth Azimuth angle (radians)
 * @param elevation Elevation angle (radians)
 * @param strength Output: interpolated signal strength
 * @return FSO_SUCCESS on success, error code on failure
 */
int signal_map_get(const SignalMap* map, double azimuth, double elevation, double* strength);

/**
 * @brief Clear all values in signal map
 * 
 * @param map Signal map to clear
 */
void signal_map_clear(SignalMap* map);

/* ============================================================================
 * PID Controller Operations
 * ============================================================================ */

/**
 * @brief Create PID controller
 * 
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 * @param update_rate Control loop update rate (Hz)
 * @param integral_limit Anti-windup limit
 * @return Pointer to allocated PIDController, or NULL on failure
 */
PIDController* pid_create(double kp, double ki, double kd, 
                          double update_rate, double integral_limit);

/**
 * @brief Free PID controller
 * 
 * @param pid Pointer to PIDController to free
 */
void pid_free(PIDController* pid);

/**
 * @brief Reset PID controller state
 * 
 * Clears integral and derivative terms.
 * 
 * @param pid PID controller to reset
 */
void pid_reset(PIDController* pid);

/**
 * @brief Update PID controller
 * 
 * Computes control output based on error signal.
 * 
 * @param pid PID controller
 * @param error_az Error in azimuth
 * @param error_el Error in elevation
 * @param output_az Output: control signal for azimuth
 * @param output_el Output: control signal for elevation
 * @return FSO_SUCCESS on success, error code on failure
 */
int pid_update(PIDController* pid, 
               double error_az, double error_el,
               double* output_az, double* output_el);

/* ============================================================================
 * Gradient Descent Beam Tracking
 * ============================================================================ */

/**
 * @brief Estimate gradient of signal strength using finite differences
 * 
 * Computes numerical gradient ∇S(θ) at current beam position using
 * finite difference approximation.
 * 
 * @param tracker Beam tracker
 * @param delta_angle Step size for finite difference (radians)
 * @param grad_az Output: gradient in azimuth direction
 * @param grad_el Output: gradient in elevation direction
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.2: Gradient descent algorithms
 */
int beam_track_estimate_gradient(BeamTracker* tracker,
                                 double delta_angle,
                                 double* grad_az,
                                 double* grad_el);

/**
 * @brief Update beam position using gradient descent
 * 
 * Implements gradient descent update: θ_new = θ_old + α * ∇S(θ)
 * with momentum term: v_new = β*v_old + α*∇S(θ), θ_new = θ_old + v_new
 * 
 * @param tracker Beam tracker
 * @param measured_strength Current measured signal strength
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.2: Gradient descent optimization
 * @note Requirement 3.3: Signal strength maps
 */
int beam_track_update(BeamTracker* tracker, double measured_strength);

/**
 * @brief Adapt step size based on convergence rate
 * 
 * Increases step size if making progress, decreases if oscillating.
 * 
 * @param tracker Beam tracker
 * @param improvement Signal strength improvement from last update
 */
void beam_track_adapt_step_size(BeamTracker* tracker, double improvement);

/**
 * @brief Check if beam tracking has converged
 * 
 * @param tracker Beam tracker
 * @return 1 if converged, 0 otherwise
 */
int beam_track_is_converged(const BeamTracker* tracker);

/* ============================================================================
 * PID Feedback Control Loop
 * ============================================================================ */

/**
 * @brief Update beam position using PID feedback control
 * 
 * Implements PID control loop for smooth beam tracking with disturbance
 * rejection. Uses measured signal strength to compute error and applies
 * PID control to adjust beam position.
 * 
 * @param tracker Beam tracker
 * @param target_az Target azimuth angle (radians)
 * @param target_el Target elevation angle (radians)
 * @param measured_strength Current measured signal strength
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.4: Feedback control loop with 100 Hz update rate
 */
int beam_track_pid_update(BeamTracker* tracker,
                          double target_az,
                          double target_el,
                          double measured_strength);

/**
 * @brief Configure PID controller parameters
 * 
 * @param tracker Beam tracker
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 * @param update_rate Control loop update rate (Hz)
 * @param integral_limit Anti-windup limit
 * @return FSO_SUCCESS on success, error code on failure
 */
int beam_track_configure_pid(BeamTracker* tracker,
                             double kp, double ki, double kd,
                             double update_rate, double integral_limit);

/**
 * @brief Reset PID controller state
 * 
 * Clears integral and derivative terms. Useful when restarting tracking
 * or after large disturbances.
 * 
 * @param tracker Beam tracker
 * @return FSO_SUCCESS on success, error code on failure
 */
int beam_track_reset_pid(BeamTracker* tracker);

/* ============================================================================
 * Beam Scanning and Signal Mapping
 * ============================================================================ */

/**
 * @brief Callback function for beam scanning
 * 
 * User-provided function to measure signal strength at a given beam position.
 * 
 * @param azimuth Azimuth angle to measure (radians)
 * @param elevation Elevation angle to measure (radians)
 * @param user_data User-provided context data
 * @return Measured signal strength at the specified position
 */
typedef double (*BeamScanCallback)(double azimuth, double elevation, void* user_data);

/**
 * @brief Perform full angular scan to build signal strength map
 * 
 * Scans across specified azimuth and elevation ranges, measuring signal
 * strength at each point and updating the signal strength map.
 * 
 * @param tracker Beam tracker
 * @param az_range Angular range for azimuth scan (radians, ±range/2 from current)
 * @param el_range Angular range for elevation scan (radians, ±range/2 from current)
 * @param resolution Angular resolution for scan (radians)
 * @param callback Function to measure signal strength at each point
 * @param user_data User data passed to callback function
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.3: Signal strength maps across azimuth and elevation
 */
int beam_track_scan(BeamTracker* tracker,
                    double az_range,
                    double el_range,
                    double resolution,
                    BeamScanCallback callback,
                    void* user_data);

/**
 * @brief Find peak signal strength in the signal map
 * 
 * Searches the signal strength map for the position with maximum signal.
 * 
 * @param tracker Beam tracker
 * @param peak_az Output: azimuth of peak signal (radians)
 * @param peak_el Output: elevation of peak signal (radians)
 * @param peak_strength Output: peak signal strength value
 * @return FSO_SUCCESS on success, error code on failure
 */
int beam_track_find_peak(const BeamTracker* tracker,
                         double* peak_az,
                         double* peak_el,
                         double* peak_strength);

/**
 * @brief Update signal strength map with new measurement
 * 
 * Adds a signal strength measurement to the map at the specified position.
 * 
 * @param tracker Beam tracker
 * @param azimuth Azimuth angle (radians)
 * @param elevation Elevation angle (radians)
 * @param strength Signal strength value
 * @return FSO_SUCCESS on success, error code on failure
 */
int beam_track_update_map(BeamTracker* tracker,
                          double azimuth,
                          double elevation,
                          double strength);

/* ============================================================================
 * Misalignment Detection and Recovery
 * ============================================================================ */

/**
 * @brief Check if beam is misaligned based on signal strength threshold
 * 
 * Monitors signal strength and detects when it falls below acceptable threshold,
 * indicating beam misalignment.
 * 
 * @param tracker Beam tracker
 * @param measured_strength Current measured signal strength
 * @return 1 if misaligned, 0 if aligned
 * 
 * @note Requirement 3.5: Misalignment detection
 */
int beam_track_check_misalignment(BeamTracker* tracker, double measured_strength);

/**
 * @brief Set signal strength threshold for misalignment detection
 * 
 * @param tracker Beam tracker
 * @param threshold Minimum acceptable signal strength (0.0 to 1.0)
 * @return FSO_SUCCESS on success, error code on failure
 */
int beam_track_set_threshold(BeamTracker* tracker, double threshold);

/**
 * @brief Perform beam reacquisition procedure
 * 
 * Executes reacquisition sequence when signal is lost:
 * 1. Perform full angular scan to find signal
 * 2. Move to peak signal position
 * 3. Resume normal tracking
 * 
 * @param tracker Beam tracker
 * @param az_search_range Azimuth search range (radians)
 * @param el_search_range Elevation search range (radians)
 * @param resolution Scan resolution (radians)
 * @param callback Function to measure signal strength
 * @param user_data User data for callback
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.5: Reacquisition procedure when signal lost
 */
int beam_track_reacquire(BeamTracker* tracker,
                         double az_search_range,
                         double el_search_range,
                         double resolution,
                         BeamScanCallback callback,
                         void* user_data);

/**
 * @brief Perform initial calibration routine
 * 
 * Establishes baseline alignment by performing a coarse scan followed
 * by fine adjustment around the peak.
 * 
 * @param tracker Beam tracker
 * @param az_range Azimuth calibration range (radians)
 * @param el_range Elevation calibration range (radians)
 * @param coarse_resolution Coarse scan resolution (radians)
 * @param fine_resolution Fine scan resolution (radians)
 * @param callback Function to measure signal strength
 * @param user_data User data for callback
 * @return FSO_SUCCESS on success, error code on failure
 * 
 * @note Requirement 3.1: Initial calibration routines
 */
int beam_track_calibrate(BeamTracker* tracker,
                         double az_range,
                         double el_range,
                         double coarse_resolution,
                         double fine_resolution,
                         BeamScanCallback callback,
                         void* user_data);

/**
 * @brief Get current tracking status
 * 
 * @param tracker Beam tracker
 * @param is_aligned Output: 1 if aligned, 0 if misaligned
 * @param is_converged Output: 1 if converged, 0 otherwise
 * @param is_reacquiring Output: 1 if in reacquisition mode, 0 otherwise
 * @return FSO_SUCCESS on success, error code on failure
 */
int beam_track_get_status(const BeamTracker* tracker,
                          int* is_aligned,
                          int* is_converged,
                          int* is_reacquiring);

#endif /* BEAM_TRACKING_H */
