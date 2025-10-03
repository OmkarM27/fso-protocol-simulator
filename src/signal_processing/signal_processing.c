/**
 * @file signal_processing.c
 * @brief Signal processing implementation with OpenMP parallelization
 */

#include "signal_processing.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MODULE_NAME "SignalProcessing"

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int sp_init(SignalProcessor* sp, int num_threads, size_t buffer_size) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_PARAM(buffer_size > 0);
    FSO_CHECK_PARAM(num_threads >= 0 && num_threads <= 16);
    
    // Initialize structure
    memset(sp, 0, sizeof(SignalProcessor));
    sp->buffer_size = buffer_size;
    
    // Check OpenMP availability and configure threads
#ifdef _OPENMP
    sp->openmp_available = 1;
    
    // Auto-detect thread count if requested
    if (num_threads == 0) {
        num_threads = omp_get_max_threads();
        FSO_LOG_INFO(MODULE_NAME, "Auto-detected %d OpenMP threads", num_threads);
    }
    
    // Clamp to valid range
    num_threads = FSO_CLAMP(num_threads, 1, 16);
    sp->num_threads = num_threads;
    
    // Set OpenMP thread count
    omp_set_num_threads(num_threads);
    
    FSO_LOG_INFO(MODULE_NAME, "Initialized with OpenMP support (%d threads)", 
                 num_threads);
#else
    sp->openmp_available = 0;
    sp->num_threads = 1;
    
    if (num_threads > 1) {
        FSO_LOG_WARNING(MODULE_NAME, 
                       "OpenMP not available, falling back to serial processing");
    } else {
        FSO_LOG_INFO(MODULE_NAME, "Initialized in serial mode");
    }
#endif
    
    // Initialize FFTW with thread support if available
#ifdef _OPENMP
    if (fftw_init_threads() == 0) {
        FSO_LOG_WARNING(MODULE_NAME, "Failed to initialize FFTW threads");
        sp->openmp_available = 0;
        sp->num_threads = 1;
    } else {
        fftw_plan_with_nthreads(sp->num_threads);
        FSO_LOG_DEBUG(MODULE_NAME, "FFTW threads initialized");
    }
#endif
    
    // FFT plans will be created on-demand
    sp->fft_forward_plan = NULL;
    sp->fft_inverse_plan = NULL;
    sp->fft_size = 0;
    
    // Initialize filter state
    sp->filter_coeffs = NULL;
    sp->filter_length = 0;
    
    // Allocate thread-local buffers
    sp->thread_buffer_size = buffer_size;
    sp->thread_buffers = (double**)calloc(sp->num_threads, sizeof(double*));
    if (sp->thread_buffers == NULL) {
        FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate thread buffer array");
        return FSO_ERROR_MEMORY;
    }
    
    for (int i = 0; i < sp->num_threads; i++) {
        sp->thread_buffers[i] = (double*)fftw_malloc(buffer_size * sizeof(double));
        if (sp->thread_buffers[i] == NULL) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate thread buffer %d", i);
            // Clean up previously allocated buffers
            for (int j = 0; j < i; j++) {
                fftw_free(sp->thread_buffers[j]);
            }
            free(sp->thread_buffers);
            return FSO_ERROR_MEMORY;
        }
    }
    
    FSO_LOG_DEBUG(MODULE_NAME, "Allocated %d thread buffers of size %zu", 
                  sp->num_threads, buffer_size);
    
    return FSO_SUCCESS;
}

void sp_free(SignalProcessor* sp) {
    if (sp == NULL) {
        return;
    }
    
    // Destroy FFT plans
    if (sp->fft_forward_plan != NULL) {
        fftw_destroy_plan(sp->fft_forward_plan);
        sp->fft_forward_plan = NULL;
    }
    
    if (sp->fft_inverse_plan != NULL) {
        fftw_destroy_plan(sp->fft_inverse_plan);
        sp->fft_inverse_plan = NULL;
    }
    
    // Free filter coefficients
    if (sp->filter_coeffs != NULL) {
        free(sp->filter_coeffs);
        sp->filter_coeffs = NULL;
    }
    
    // Free thread buffers
    if (sp->thread_buffers != NULL) {
        for (int i = 0; i < sp->num_threads; i++) {
            if (sp->thread_buffers[i] != NULL) {
                fftw_free(sp->thread_buffers[i]);
            }
        }
        free(sp->thread_buffers);
        sp->thread_buffers = NULL;
    }
    
    // Cleanup FFTW threads
#ifdef _OPENMP
    fftw_cleanup_threads();
#endif
    
    fftw_cleanup();
    
    FSO_LOG_DEBUG(MODULE_NAME, "Signal processor freed");
}

int sp_get_num_threads(const SignalProcessor* sp) {
    if (sp == NULL) {
        return -1;
    }
    return sp->num_threads;
}

int sp_is_openmp_available(const SignalProcessor* sp) {
    if (sp == NULL) {
        return 0;
    }
    return sp->openmp_available;
}

/* ============================================================================
 * FFT Operations
 * ============================================================================ */

int sp_fft(SignalProcessor* sp, const double* input, 
           double complex* output, size_t length) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(input);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(length > 0);
    
    // Check if we need to create or recreate FFT plan
    if (sp->fft_forward_plan == NULL || sp->fft_size != length) {
        // Destroy old plan if it exists
        if (sp->fft_forward_plan != NULL) {
            fftw_destroy_plan(sp->fft_forward_plan);
            FSO_LOG_DEBUG(MODULE_NAME, "Destroyed old FFT plan (size %zu)", sp->fft_size);
        }
        
        // Allocate temporary buffers for planning
        double* in_temp = (double*)fftw_malloc(length * sizeof(double));
        fftw_complex* out_temp = (fftw_complex*)fftw_malloc(
            ((length / 2) + 1) * sizeof(fftw_complex));
        
        if (in_temp == NULL || out_temp == NULL) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate FFT planning buffers");
            if (in_temp) fftw_free(in_temp);
            if (out_temp) fftw_free(out_temp);
            return FSO_ERROR_MEMORY;
        }
        
        // Create new plan with FFTW_MEASURE for optimal performance
        // Note: FFTW_MEASURE may take some time but provides better performance
        sp->fft_forward_plan = fftw_plan_dft_r2c_1d(
            (int)length, in_temp, out_temp, FFTW_MEASURE);
        
        if (sp->fft_forward_plan == NULL) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to create FFT plan");
            fftw_free(in_temp);
            fftw_free(out_temp);
            return FSO_ERROR_MEMORY;
        }
        
        sp->fft_size = length;
        
        fftw_free(in_temp);
        fftw_free(out_temp);
        
        FSO_LOG_DEBUG(MODULE_NAME, "Created FFT plan for size %zu", length);
    }
    
    // Allocate aligned buffers for actual computation
    double* in_aligned = (double*)fftw_malloc(length * sizeof(double));
    fftw_complex* out_aligned = (fftw_complex*)fftw_malloc(
        ((length / 2) + 1) * sizeof(fftw_complex));
    
    if (in_aligned == NULL || out_aligned == NULL) {
        FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate FFT computation buffers");
        if (in_aligned) fftw_free(in_aligned);
        if (out_aligned) fftw_free(out_aligned);
        return FSO_ERROR_MEMORY;
    }
    
    // Copy input data
    memcpy(in_aligned, input, length * sizeof(double));
    
    // Execute FFT with the existing plan
    fftw_execute_dft_r2c(sp->fft_forward_plan, in_aligned, out_aligned);
    
    // Copy output data (convert fftw_complex to double complex)
    size_t output_length = (length / 2) + 1;
    for (size_t i = 0; i < output_length; i++) {
        output[i] = out_aligned[i][0] + I * out_aligned[i][1];
    }
    
    fftw_free(in_aligned);
    fftw_free(out_aligned);
    
    FSO_LOG_DEBUG(MODULE_NAME, "Executed FFT on %zu samples", length);
    
    return FSO_SUCCESS;
}

int sp_ifft(SignalProcessor* sp, const double complex* input,
            double* output, size_t length) {
    FSO_CHECK_NULL(sp);
    FSO_CHECK_NULL(input);
    FSO_CHECK_NULL(output);
    FSO_CHECK_PARAM(length > 0);
    
    size_t input_length = (length / 2) + 1;
    
    // Check if we need to create or recreate inverse FFT plan
    if (sp->fft_inverse_plan == NULL || sp->fft_size != length) {
        // Destroy old plan if it exists
        if (sp->fft_inverse_plan != NULL) {
            fftw_destroy_plan(sp->fft_inverse_plan);
            FSO_LOG_DEBUG(MODULE_NAME, "Destroyed old inverse FFT plan (size %zu)", 
                         sp->fft_size);
        }
        
        // Allocate temporary buffers for planning
        fftw_complex* in_temp = (fftw_complex*)fftw_malloc(
            input_length * sizeof(fftw_complex));
        double* out_temp = (double*)fftw_malloc(length * sizeof(double));
        
        if (in_temp == NULL || out_temp == NULL) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate inverse FFT planning buffers");
            if (in_temp) fftw_free(in_temp);
            if (out_temp) fftw_free(out_temp);
            return FSO_ERROR_MEMORY;
        }
        
        // Create new inverse plan
        sp->fft_inverse_plan = fftw_plan_dft_c2r_1d(
            (int)length, in_temp, out_temp, FFTW_MEASURE);
        
        if (sp->fft_inverse_plan == NULL) {
            FSO_LOG_ERROR(MODULE_NAME, "Failed to create inverse FFT plan");
            fftw_free(in_temp);
            fftw_free(out_temp);
            return FSO_ERROR_MEMORY;
        }
        
        // Update size if forward plan wasn't created yet
        if (sp->fft_size != length) {
            sp->fft_size = length;
        }
        
        fftw_free(in_temp);
        fftw_free(out_temp);
        
        FSO_LOG_DEBUG(MODULE_NAME, "Created inverse FFT plan for size %zu", length);
    }
    
    // Allocate aligned buffers for actual computation
    fftw_complex* in_aligned = (fftw_complex*)fftw_malloc(
        input_length * sizeof(fftw_complex));
    double* out_aligned = (double*)fftw_malloc(length * sizeof(double));
    
    if (in_aligned == NULL || out_aligned == NULL) {
        FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate inverse FFT computation buffers");
        if (in_aligned) fftw_free(in_aligned);
        if (out_aligned) fftw_free(out_aligned);
        return FSO_ERROR_MEMORY;
    }
    
    // Copy input data (convert double complex to fftw_complex)
    for (size_t i = 0; i < input_length; i++) {
        in_aligned[i][0] = creal(input[i]);
        in_aligned[i][1] = cimag(input[i]);
    }
    
    // Execute inverse FFT
    fftw_execute_dft_c2r(sp->fft_inverse_plan, in_aligned, out_aligned);
    
    // Copy and normalize output (FFTW doesn't normalize)
    double norm_factor = 1.0 / (double)length;
    for (size_t i = 0; i < length; i++) {
        output[i] = out_aligned[i] * norm_factor;
    }
    
    fftw_free(in_aligned);
    fftw_free(out_aligned);
    
    FSO_LOG_DEBUG(MODULE_NAME, "Executed inverse FFT on %zu samples", length);
    
    return FSO_SUCCESS;
}
