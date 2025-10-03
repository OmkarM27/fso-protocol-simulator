/**
 * @file channel_estimation.c
 * @brief Channel estimation algorithms implementation
 */

#include "signal_processing.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MODULE_NAME "ChannelEstimation"

/* ============================================================================
 * Channel Estimation Algorithms
 * ============================================================================ */

int sp_channel_estimate_pilot(SignalProcessor* sp,
                               const double complex* received,
                               const double complex* pilots,
                               const size_t* pilot_positions,
                               size_t num_pilots,
                               double complex* channel_estimate,
                               size_t estimate_length) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(received);
    FSO_CHECK_NULL(pilots);
    FSO_CHECK_NULL(pilot_positions);
    FSO_CHECK_NULL(channel_estimate);
    FSO_CHECK_PARAM(num_pilots > 0);
    FSO_CHECK_PARAM(estimate_length > 0);
    
    FSO_LOG_DEBUG(MODULE_NAME, "Pilot-based estimation: %zu pilots, length=%zu",
                  num_pilots, estimate_length);
    
    // Allocate array for pilot channel estimates
    double complex* pilot_estimates = (double complex*)malloc(
        num_pilots * sizeof(double complex));
    if (pilot_estimates == NULL) {
        FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate pilot estimates");
        return FSO_ERROR_MEMORY;
    }
    
    // Compute channel estimate at pilot positions (parallel)
#ifdef _OPENMP
    if (sp->openmp_available) {
        #pragma omp parallel for num_threads(sp->num_threads)
        for (size_t i = 0; i < num_pilots; i++) {
            size_t pos = pilot_positions[i];
            if (pos < estimate_length) {
                // H = Y / X (received / transmitted)
                double complex pilot = pilots[i];
                if (cabs(pilot) > 1e-10) {
                    pilot_estimates[i] = received[pos] / pilot;
                } else {
                    pilot_estimates[i] = 0.0 + 0.0 * I;
                }
            } else {
                pilot_estimates[i] = 0.0 + 0.0 * I;
            }
        }
    } else
#endif
    {
        // Serial computation
        for (size_t i = 0; i < num_pilots; i++) {
            size_t pos = pilot_positions[i];
            if (pos < estimate_length) {
                double complex pilot = pilots[i];
                if (cabs(pilot) > 1e-10) {
                    pilot_estimates[i] = received[pos] / pilot;
                } else {
                    pilot_estimates[i] = 0.0 + 0.0 * I;
                }
            } else {
                pilot_estimates[i] = 0.0 + 0.0 * I;
            }
        }
    }
    
    // Interpolate channel estimates for all positions
    // Using linear interpolation between pilots
#ifdef _OPENMP
    if (sp->openmp_available) {
        #pragma omp parallel for num_threads(sp->num_threads)
        for (size_t n = 0; n < estimate_length; n++) {
            // Find surrounding pilots
            size_t left_idx = 0;
            size_t right_idx = num_pilots - 1;
            
            for (size_t i = 0; i < num_pilots - 1; i++) {
                if (pilot_positions[i] <= n && pilot_positions[i + 1] > n) {
                    left_idx = i;
                    right_idx = i + 1;
                    break;
                }
            }
            
            // Linear interpolation
            if (left_idx == right_idx) {
                channel_estimate[n] = pilot_estimates[left_idx];
            } else {
                size_t left_pos = pilot_positions[left_idx];
                size_t right_pos = pilot_positions[right_idx];
                double alpha = (double)(n - left_pos) / (double)(right_pos - left_pos);
                
                channel_estimate[n] = (1.0 - alpha) * pilot_estimates[left_idx] +
                                     alpha * pilot_estimates[right_idx];
            }
        }
    } else
#endif
    {
        // Serial interpolation
        for (size_t n = 0; n < estimate_length; n++) {
            size_t left_idx = 0;
            size_t right_idx = num_pilots - 1;
            
            for (size_t i = 0; i < num_pilots - 1; i++) {
                if (pilot_positions[i] <= n && pilot_positions[i + 1] > n) {
                    left_idx = i;
                    right_idx = i + 1;
                    break;
                }
            }
            
            if (left_idx == right_idx) {
                channel_estimate[n] = pilot_estimates[left_idx];
            } else {
                size_t left_pos = pilot_positions[left_idx];
                size_t right_pos = pilot_positions[right_idx];
                double alpha = (double)(n - left_pos) / (double)(right_pos - left_pos);
                
                channel_estimate[n] = (1.0 - alpha) * pilot_estimates[left_idx] +
                                     alpha * pilot_estimates[right_idx];
            }
        }
    }
    
    free(pilot_estimates);
    
    return FSO_SUCCESS;
}

int sp_channel_estimate_ls(SignalProcessor* sp,
                            const double complex* received,
                            const double complex* transmitted,
                            size_t length,
                            double complex* channel_estimate,
                            size_t channel_length) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(received);
    FSO_CHECK_NULL(transmitted);
    FSO_CHECK_NULL(channel_estimate);
    FSO_CHECK_PARAM(length > 0);
    FSO_CHECK_PARAM(channel_length > 0);
    FSO_CHECK_PARAM(channel_length <= length);
    
    FSO_LOG_DEBUG(MODULE_NAME, "Least-squares estimation: length=%zu, channel_len=%zu",
                  length, channel_length);
    
    // Simple least-squares: minimize ||Y - X*H||^2
    // For each tap of the channel, compute correlation
    
    // Initialize channel estimate to zero
    memset(channel_estimate, 0, channel_length * sizeof(double complex));
    
    // Compute each channel tap (parallel over taps)
#ifdef _OPENMP
    if (sp->openmp_available) {
        #pragma omp parallel for num_threads(sp->num_threads)
        for (size_t k = 0; k < channel_length; k++) {
            double complex numerator = 0.0 + 0.0 * I;
            double denominator = 0.0;
            
            // Correlation between received and delayed transmitted
            for (size_t n = k; n < length; n++) {
                numerator += received[n] * conj(transmitted[n - k]);
                denominator += cabs(transmitted[n - k]) * cabs(transmitted[n - k]);
            }
            
            if (denominator > 1e-10) {
                channel_estimate[k] = numerator / denominator;
            } else {
                channel_estimate[k] = 0.0 + 0.0 * I;
            }
        }
    } else
#endif
    {
        // Serial computation
        for (size_t k = 0; k < channel_length; k++) {
            double complex numerator = 0.0 + 0.0 * I;
            double denominator = 0.0;
            
            for (size_t n = k; n < length; n++) {
                numerator += received[n] * conj(transmitted[n - k]);
                denominator += cabs(transmitted[n - k]) * cabs(transmitted[n - k]);
            }
            
            if (denominator > 1e-10) {
                channel_estimate[k] = numerator / denominator;
            } else {
                channel_estimate[k] = 0.0 + 0.0 * I;
            }
        }
    }
    
    return FSO_SUCCESS;
}

int sp_noise_variance_estimate(SignalProcessor* sp,
                                const double complex* received,
                                const double complex* expected,
                                size_t length,
                                double* noise_variance) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(received);
    FSO_CHECK_NULL(expected);
    FSO_CHECK_NULL(noise_variance);
    FSO_CHECK_PARAM(length > 0);
    
    FSO_LOG_DEBUG(MODULE_NAME, "Noise variance estimation: length=%zu", length);
    
    // Compute mean squared error between received and expected
    double sum_squared_error = 0.0;
    
#ifdef _OPENMP
    if (sp->openmp_available) {
        #pragma omp parallel for reduction(+:sum_squared_error) num_threads(sp->num_threads)
        for (size_t i = 0; i < length; i++) {
            double complex error = received[i] - expected[i];
            sum_squared_error += cabs(error) * cabs(error);
        }
    } else
#endif
    {
        // Serial computation
        for (size_t i = 0; i < length; i++) {
            double complex error = received[i] - expected[i];
            sum_squared_error += cabs(error) * cabs(error);
        }
    }
    
    // Variance is average squared error
    *noise_variance = sum_squared_error / (double)length;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Estimated noise variance: %f", *noise_variance);
    
    return FSO_SUCCESS;
}
