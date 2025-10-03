/**
 * @file filtering.c
 * @brief Parallel filtering operations implementation
 */

#include "signal_processing.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MODULE_NAME "Filtering"

/* ============================================================================
 * Filtering Operations
 * ============================================================================ */

int sp_moving_average(SignalProcessor* sp, const double* input,
                      double* output, size_t length, int window) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(input);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(length > 0);
    FSO_CHECK_PARAM(window > 0);
    
    // Clamp window size to signal length
    if (window > (int)length) {
        window = (int)length;
    }
    
    FSO_LOG_DEBUG(MODULE_NAME, "Moving average: length=%zu, window=%d, threads=%d",
                  length, window, sp->num_threads);
    
    // Parallel implementation using OpenMP
#ifdef _OPENMP
    if (sp->openmp_available) {
        #pragma omp parallel for num_threads(sp->num_threads)
        for (size_t i = 0; i < length; i++) {
            double sum = 0.0;
            int count = 0;
            
            // Determine window bounds (handle edges)
            int start = (int)i - window / 2;
            int end = start + window;
            
            if (start < 0) start = 0;
            if (end > (int)length) end = (int)length;
            
            // Compute average
            for (int j = start; j < end; j++) {
                sum += input[j];
                count++;
            }
            
            output[i] = sum / count;
        }
    } else
#endif
    {
        // Serial fallback
        for (size_t i = 0; i < length; i++) {
            double sum = 0.0;
            int count = 0;
            
            int start = (int)i - window / 2;
            int end = start + window;
            
            if (start < 0) start = 0;
            if (end > (int)length) end = (int)length;
            
            for (int j = start; j < end; j++) {
                sum += input[j];
                count++;
            }
            
            output[i] = sum / count;
        }
    }
    
    return FSO_SUCCESS;
}

int sp_adaptive_filter(SignalProcessor* sp, const double* input,
                       const double* desired, double* output,
                       size_t length, double mu) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(input);
    FSO_CHECK_NULL(desired);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(length > 0);
    FSO_CHECK_PARAM(mu > 0.0 && mu < 1.0);
    
    // Initialize filter if not already done
    if (sp->filter_coeffs == NULL) {
        sp->filter_length = 32;  // Default filter length
        sp->filter_coeffs = (double*)calloc(sp->filter_length, sizeof(double));
        if (sp->filter_coeffs == NULL) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate filter coefficients");
            return FSO_ERROR_MEMORY;
        }
        FSO_LOG_DEBUG(MODULE_NAME, "Initialized adaptive filter with %d taps", 
                     sp->filter_length);
    }
    
    int filter_len = sp->filter_length;
    double* weights = sp->filter_coeffs;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Adaptive LMS filter: length=%zu, mu=%f, taps=%d",
                  length, mu, filter_len);
    
    // LMS algorithm - must be sequential due to weight updates
    for (size_t n = 0; n < length; n++) {
        // Compute filter output
        double y = 0.0;
        for (int k = 0; k < filter_len; k++) {
            int idx = (int)n - k;
            if (idx >= 0) {
                y += weights[k] * input[idx];
            }
        }
        output[n] = y;
        
        // Compute error
        double error = desired[n] - y;
        
        // Update weights in parallel
#ifdef _OPENMP
        if (sp->openmp_available) {
            #pragma omp parallel for num_threads(sp->num_threads)
            for (int k = 0; k < filter_len; k++) {
                int idx = (int)n - k;
                if (idx >= 0) {
                    weights[k] += 2.0 * mu * error * input[idx];
                }
            }
        } else
#endif
        {
            // Serial weight update
            for (int k = 0; k < filter_len; k++) {
                int idx = (int)n - k;
                if (idx >= 0) {
                    weights[k] += 2.0 * mu * error * input[idx];
                }
            }
        }
    }
    
    return FSO_SUCCESS;
}

int sp_convolution(SignalProcessor* sp, const double* signal,
                   const double* kernel, double* output,
                   size_t sig_len, size_t kernel_len) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(signal);
    FSO_CHECK_NULL(kernel);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(sig_len > 0);
    FSO_CHECK_PARAM(kernel_len > 0);
    
    size_t output_len = sig_len + kernel_len - 1;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Convolution: sig_len=%zu, kernel_len=%zu",
                  sig_len, kernel_len);
    
    // For small kernels, use direct convolution
    if (kernel_len < 64) {
        // Direct convolution with parallel outer loop
#ifdef _OPENMP
        if (sp->openmp_available) {
            #pragma omp parallel for num_threads(sp->num_threads)
            for (size_t n = 0; n < output_len; n++) {
                double sum = 0.0;
                for (size_t k = 0; k < kernel_len; k++) {
                    if (n >= k && (n - k) < sig_len) {
                        sum += signal[n - k] * kernel[k];
                    }
                }
                output[n] = sum;
            }
        } else
#endif
        {
            // Serial direct convolution
            for (size_t n = 0; n < output_len; n++) {
                double sum = 0.0;
                for (size_t k = 0; k < kernel_len; k++) {
                    if (n >= k && (n - k) < sig_len) {
                        sum += signal[n - k] * kernel[k];
                    }
                }
                output[n] = sum;
            }
        }
    } else {
        // FFT-based convolution for large kernels (overlap-add method)
        size_t fft_size = 1;
        while (fft_size < (sig_len + kernel_len - 1)) {
            fft_size *= 2;
        }
        
        // Allocate FFT buffers
        double* sig_padded = (double*)fftw_malloc(fft_size * sizeof(double));
        double* ker_padded = (double*)fftw_malloc(fft_size * sizeof(double));
        double complex* sig_fft = (double complex*)fftw_malloc(
            ((fft_size / 2) + 1) * sizeof(double complex));
        double complex* ker_fft = (double complex*)fftw_malloc(
            ((fft_size / 2) + 1) * sizeof(double complex));
        double complex* prod_fft = (double complex*)fftw_malloc(
            ((fft_size / 2) + 1) * sizeof(double complex));
        double* result = (double*)fftw_malloc(fft_size * sizeof(double));
        
        if (!sig_padded || !ker_padded || !sig_fft || !ker_fft || !prod_fft || !result) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate FFT convolution buffers");
            if (sig_padded) fftw_free(sig_padded);
            if (ker_padded) fftw_free(ker_padded);
            if (sig_fft) fftw_free(sig_fft);
            if (ker_fft) fftw_free(ker_fft);
            if (prod_fft) fftw_free(prod_fft);
            if (result) fftw_free(result);
            return FSO_ERROR_MEMORY;
        }
        
        // Zero-pad inputs
        memcpy(sig_padded, signal, sig_len * sizeof(double));
        memset(sig_padded + sig_len, 0, (fft_size - sig_len) * sizeof(double));
        
        memcpy(ker_padded, kernel, kernel_len * sizeof(double));
        memset(ker_padded + kernel_len, 0, (fft_size - kernel_len) * sizeof(double));
        
        // Compute FFTs
        int ret1 = sp_fft(sp, sig_padded, sig_fft, fft_size);
        int ret2 = sp_fft(sp, ker_padded, ker_fft, fft_size);
        
        if (ret1 != FSO_SUCCESS || ret2 != FSO_SUCCESS) {
            FSO_LOG_ERROR(MODULE_NAME, "FFT failed in convolution");
            fftw_free(sig_padded);
            fftw_free(ker_padded);
            fftw_free(sig_fft);
            fftw_free(ker_fft);
            fftw_free(prod_fft);
            fftw_free(result);
            return FSO_ERROR_MEMORY;
        }
        
        // Multiply in frequency domain (parallel)
        size_t fft_len = (fft_size / 2) + 1;
#ifdef _OPENMP
        if (sp->openmp_available) {
            #pragma omp parallel for num_threads(sp->num_threads)
            for (size_t i = 0; i < fft_len; i++) {
                prod_fft[i] = sig_fft[i] * ker_fft[i];
            }
        } else
#endif
        {
            for (size_t i = 0; i < fft_len; i++) {
                prod_fft[i] = sig_fft[i] * ker_fft[i];
            }
        }
        
        // Inverse FFT
        int ret3 = sp_ifft(sp, prod_fft, result, fft_size);
        if (ret3 != FSO_SUCCESS) {
            FSO_LOG_ERROR(MODULE_NAME, "Inverse FFT failed in convolution");
            fftw_free(sig_padded);
            fftw_free(ker_padded);
            fftw_free(sig_fft);
            fftw_free(ker_fft);
            fftw_free(prod_fft);
            fftw_free(result);
            return FSO_ERROR_MEMORY;
        }
        
        // Copy result
        memcpy(output, result, output_len * sizeof(double));
        
        // Cleanup
        fftw_free(sig_padded);
        fftw_free(ker_padded);
        fftw_free(sig_fft);
        fftw_free(ker_fft);
        fftw_free(prod_fft);
        fftw_free(result);
    }
    
    return FSO_SUCCESS;
}
