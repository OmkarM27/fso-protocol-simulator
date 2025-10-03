/**
 * @file signal_processing.h
 * @brief Signal processing module with OpenMP parallelization
 * 
 * This module provides parallelized DSP operations for real-time signal
 * analysis and conditioning, including FFT, filtering, and channel estimation.
 */

#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include "../fso.h"
#include <complex.h>
#include <fftw3.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* ============================================================================
 * Signal Processor Structure
 * ============================================================================ */

/**
 * @brief Signal processing context
 * 
 * Contains configuration and state for parallel signal processing operations.
 */
typedef struct {
    int num_threads;              /**< Number of OpenMP threads */
    size_t buffer_size;           /**< Processing buffer size */
    int openmp_available;         /**< Flag indicating OpenMP availability */
    
    /* FFT plans */
    fftw_plan fft_forward_plan;   /**< Forward FFT plan */
    fftw_plan fft_inverse_plan;   /**< Inverse FFT plan */
    size_t fft_size;              /**< Size of FFT plans */
    
    /* Filter state */
    double* filter_coeffs;        /**< Filter coefficients */
    int filter_length;            /**< Number of filter taps */
    
    /* Thread-local buffers (allocated per thread) */
    double** thread_buffers;      /**< Per-thread work buffers */
    size_t thread_buffer_size;    /**< Size of each thread buffer */
} SignalProcessor;

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize signal processor
 * 
 * Creates a signal processor context with specified configuration.
 * If OpenMP is unavailable, falls back to serial processing with a warning.
 * 
 * @param sp Pointer to signal processor structure
 * @param num_threads Desired number of threads (1-16), 0 for auto-detect
 * @param buffer_size Processing buffer size in samples
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note If num_threads is 0, uses omp_get_max_threads() or 1 if OpenMP unavailable
 * @note FFT plans are created on-demand when sp_fft is first called
 */
int sp_init(SignalProcessor* sp, int num_threads, size_t buffer_size);

/**
 * @brief Free signal processor resources
 * 
 * Cleans up all allocated resources including FFT plans and thread buffers.
 * 
 * @param sp Pointer to signal processor structure
 */
void sp_free(SignalProcessor* sp);

/**
 * @brief Get number of threads being used
 * 
 * @param sp Pointer to signal processor structure
 * @return Number of threads, or -1 if sp is NULL
 */
int sp_get_num_threads(const SignalProcessor* sp);

/**
 * @brief Check if OpenMP is available
 * 
 * @param sp Pointer to signal processor structure
 * @return 1 if OpenMP is available, 0 otherwise
 */
int sp_is_openmp_available(const SignalProcessor* sp);

/* ============================================================================
 * FFT Operations
 * ============================================================================ */

/**
 * @brief Perform forward FFT with parallel execution
 * 
 * Computes the Discrete Fourier Transform of the input signal using FFTW
 * with OpenMP parallelization.
 * 
 * @param sp Pointer to signal processor structure
 * @param input Input real signal array
 * @param output Output complex frequency domain array
 * @param length Length of input/output arrays
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Output array must have space for (length/2 + 1) complex samples for real input
 * @note FFT plan is created on first call and reused for subsequent calls of same size
 */
int sp_fft(SignalProcessor* sp, const double* input, 
           double complex* output, size_t length);

/**
 * @brief Perform inverse FFT with parallel execution
 * 
 * Computes the inverse Discrete Fourier Transform to convert frequency
 * domain signal back to time domain.
 * 
 * @param sp Pointer to signal processor structure
 * @param input Input complex frequency domain array
 * @param output Output real time domain array
 * @param length Length of output array (input has length/2 + 1 complex samples)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sp_ifft(SignalProcessor* sp, const double complex* input,
            double* output, size_t length);

/* ============================================================================
 * Filtering Operations
 * ============================================================================ */

/**
 * @brief Apply moving average filter with OpenMP parallelization
 * 
 * Computes the moving average of the input signal with specified window size.
 * Uses parallel for loop to process output samples independently.
 * 
 * @param sp Pointer to signal processor structure
 * @param input Input signal array
 * @param output Output filtered signal array
 * @param length Length of input/output arrays
 * @param window Window size for moving average
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Output array must be pre-allocated with same size as input
 * @note Edge samples use partial windows
 */
int sp_moving_average(SignalProcessor* sp, const double* input,
                      double* output, size_t length, int window);

/**
 * @brief Apply adaptive LMS filter with parallel weight updates
 * 
 * Implements Least Mean Squares adaptive filter with parallel processing.
 * Updates filter weights to minimize error between output and desired signal.
 * 
 * @param sp Pointer to signal processor structure
 * @param input Input signal array
 * @param desired Desired output signal array
 * @param output Actual output signal array
 * @param length Length of input/output arrays
 * @param mu Step size (learning rate), typically 0.001 to 0.1
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Filter coefficients are stored in sp->filter_coeffs
 * @note Filter length is sp->filter_length
 */
int sp_adaptive_filter(SignalProcessor* sp, const double* input,
                       const double* desired, double* output,
                       size_t length, double mu);

/**
 * @brief Perform convolution using overlap-add method with parallel FFT
 * 
 * Computes linear convolution of signal with kernel using FFT-based
 * overlap-add method for efficiency with long kernels.
 * 
 * @param sp Pointer to signal processor structure
 * @param signal Input signal array
 * @param kernel Convolution kernel array
 * @param output Output convolved signal array
 * @param sig_len Length of input signal
 * @param kernel_len Length of kernel
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note Output length is sig_len + kernel_len - 1
 * @note Output array must be pre-allocated
 */
int sp_convolution(SignalProcessor* sp, const double* signal,
                   const double* kernel, double* output,
                   size_t sig_len, size_t kernel_len);

/* ============================================================================
 * Channel Estimation
 * ============================================================================ */

/**
 * @brief Pilot-based channel estimation
 * 
 * Estimates channel response using known pilot symbols.
 * 
 * @param sp Pointer to signal processor structure
 * @param received Received signal array
 * @param pilots Known pilot symbols array
 * @param pilot_positions Positions of pilots in received signal
 * @param num_pilots Number of pilot symbols
 * @param channel_estimate Output channel estimate array
 * @param estimate_length Length of channel estimate
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sp_channel_estimate_pilot(SignalProcessor* sp,
                               const double complex* received,
                               const double complex* pilots,
                               const size_t* pilot_positions,
                               size_t num_pilots,
                               double complex* channel_estimate,
                               size_t estimate_length);

/**
 * @brief Least-squares channel estimation
 * 
 * Estimates channel impulse response using least-squares method.
 * 
 * @param sp Pointer to signal processor structure
 * @param received Received signal array
 * @param transmitted Known transmitted signal array
 * @param length Length of signal arrays
 * @param channel_estimate Output channel impulse response
 * @param channel_length Length of channel impulse response
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sp_channel_estimate_ls(SignalProcessor* sp,
                            const double complex* received,
                            const double complex* transmitted,
                            size_t length,
                            double complex* channel_estimate,
                            size_t channel_length);

/**
 * @brief Estimate noise variance
 * 
 * Estimates noise variance from received signal using pilot symbols
 * or decision-directed method.
 * 
 * @param sp Pointer to signal processor structure
 * @param received Received signal array
 * @param expected Expected signal array (pilots or decisions)
 * @param length Length of arrays
 * @param noise_variance Output noise variance estimate
 * @return FSO_SUCCESS on success, error code otherwise
 */
int sp_noise_variance_estimate(SignalProcessor* sp,
                                const double complex* received,
                                const double complex* expected,
                                size_t length,
                                double* noise_variance);

#endif /* SIGNAL_PROCESSING_H */
