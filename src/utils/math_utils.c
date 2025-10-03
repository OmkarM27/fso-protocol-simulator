/**
 * @file math_utils.c
 * @brief Mathematical utility functions for FSO Communication Suite
 * 
 * Implements complex number operations, signal power calculations,
 * and dB conversion utilities.
 */

#include "../fso.h"
#include <math.h>

/* ============================================================================
 * Complex Number Operations
 * ============================================================================ */

/**
 * @brief Add two complex numbers
 * @param a First complex number
 * @param b Second complex number
 * @return Sum a + b
 */
ComplexSample fso_complex_add(ComplexSample a, ComplexSample b) {
    ComplexSample result;
    result.real = a.real + b.real;
    result.imag = a.imag + b.imag;
    return result;
}

/**
 * @brief Subtract two complex numbers
 * @param a First complex number
 * @param b Second complex number
 * @return Difference a - b
 */
ComplexSample fso_complex_sub(ComplexSample a, ComplexSample b) {
    ComplexSample result;
    result.real = a.real - b.real;
    result.imag = a.imag - b.imag;
    return result;
}

/**
 * @brief Multiply two complex numbers
 * @param a First complex number
 * @param b Second complex number
 * @return Product a * b
 */
ComplexSample fso_complex_mul(ComplexSample a, ComplexSample b) {
    ComplexSample result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

/**
 * @brief Divide two complex numbers
 * @param a Numerator
 * @param b Denominator
 * @return Quotient a / b
 */
ComplexSample fso_complex_div(ComplexSample a, ComplexSample b) {
    ComplexSample result;
    double denom = b.real * b.real + b.imag * b.imag;
    
    if (denom == 0.0) {
        FSO_LOG_ERROR("MATH", "Division by zero in complex division");
        result.real = 0.0;
        result.imag = 0.0;
        return result;
    }
    
    result.real = (a.real * b.real + a.imag * b.imag) / denom;
    result.imag = (a.imag * b.real - a.real * b.imag) / denom;
    return result;
}

/**
 * @brief Calculate magnitude of complex number
 * @param c Complex number
 * @return Magnitude |c| = sqrt(real^2 + imag^2)
 */
double fso_complex_magnitude(ComplexSample c) {
    return sqrt(c.real * c.real + c.imag * c.imag);
}

/**
 * @brief Calculate squared magnitude of complex number
 * @param c Complex number
 * @return Squared magnitude |c|^2 = real^2 + imag^2
 */
double fso_complex_magnitude_squared(ComplexSample c) {
    return c.real * c.real + c.imag * c.imag;
}

/**
 * @brief Calculate phase of complex number
 * @param c Complex number
 * @return Phase in radians, range [-π, π]
 */
double fso_complex_phase(ComplexSample c) {
    return atan2(c.imag, c.real);
}

/**
 * @brief Calculate complex conjugate
 * @param c Complex number
 * @return Conjugate c* = real - j*imag
 */
ComplexSample fso_complex_conjugate(ComplexSample c) {
    ComplexSample result;
    result.real = c.real;
    result.imag = -c.imag;
    return result;
}

/**
 * @brief Create complex number from magnitude and phase
 * @param magnitude Magnitude
 * @param phase Phase in radians
 * @return Complex number = magnitude * exp(j*phase)
 */
ComplexSample fso_complex_from_polar(double magnitude, double phase) {
    ComplexSample result;
    result.real = magnitude * cos(phase);
    result.imag = magnitude * sin(phase);
    return result;
}

/**
 * @brief Multiply complex number by real scalar
 * @param c Complex number
 * @param scalar Real scalar value
 * @return Product scalar * c
 */
ComplexSample fso_complex_scale(ComplexSample c, double scalar) {
    ComplexSample result;
    result.real = c.real * scalar;
    result.imag = c.imag * scalar;
    return result;
}

/* ============================================================================
 * Signal Power Calculations
 * ============================================================================ */

/**
 * @brief Calculate average power of real signal
 * @param signal Array of signal samples
 * @param length Number of samples
 * @return Average power (mean of squared samples)
 */
double fso_signal_power_real(const double* signal, size_t length) {
    if (signal == NULL || length == 0) {
        FSO_LOG_ERROR("MATH", "Invalid parameters for signal power calculation");
        return 0.0;
    }
    
    double sum = 0.0;
    for (size_t i = 0; i < length; i++) {
        sum += signal[i] * signal[i];
    }
    
    return sum / (double)length;
}

/**
 * @brief Calculate average power of complex signal
 * @param signal Array of complex signal samples
 * @param length Number of samples
 * @return Average power (mean of squared magnitudes)
 */
double fso_signal_power_complex(const ComplexSample* signal, size_t length) {
    if (signal == NULL || length == 0) {
        FSO_LOG_ERROR("MATH", "Invalid parameters for signal power calculation");
        return 0.0;
    }
    
    double sum = 0.0;
    for (size_t i = 0; i < length; i++) {
        sum += fso_complex_magnitude_squared(signal[i]);
    }
    
    return sum / (double)length;
}

/**
 * @brief Calculate RMS (Root Mean Square) value of real signal
 * @param signal Array of signal samples
 * @param length Number of samples
 * @return RMS value
 */
double fso_signal_rms(const double* signal, size_t length) {
    return sqrt(fso_signal_power_real(signal, length));
}

/**
 * @brief Calculate peak power of real signal
 * @param signal Array of signal samples
 * @param length Number of samples
 * @return Peak power (maximum squared sample)
 */
double fso_signal_peak_power(const double* signal, size_t length) {
    if (signal == NULL || length == 0) {
        FSO_LOG_ERROR("MATH", "Invalid parameters for peak power calculation");
        return 0.0;
    }
    
    double peak = 0.0;
    for (size_t i = 0; i < length; i++) {
        double power = signal[i] * signal[i];
        if (power > peak) {
            peak = power;
        }
    }
    
    return peak;
}

/**
 * @brief Calculate Signal-to-Noise Ratio (SNR)
 * @param signal_power Signal power (linear scale)
 * @param noise_power Noise power (linear scale)
 * @return SNR in dB
 */
double fso_calculate_snr(double signal_power, double noise_power) {
    if (noise_power <= 0.0) {
        FSO_LOG_WARNING("MATH", "Invalid noise power for SNR calculation");
        return INFINITY;
    }
    
    return fso_linear_to_db(signal_power / noise_power);
}

/* ============================================================================
 * dB Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert linear power to dB
 * @param linear_value Linear power value
 * @return Power in dB (10 * log10(linear_value))
 */
double fso_linear_to_db(double linear_value) {
    if (linear_value <= 0.0) {
        FSO_LOG_WARNING("MATH", "Non-positive value in linear to dB conversion");
        return -INFINITY;
    }
    
    return 10.0 * log10(linear_value);
}

/**
 * @brief Convert dB to linear power
 * @param db_value Power in dB
 * @return Linear power value (10^(db_value/10))
 */
double fso_db_to_linear(double db_value) {
    return pow(10.0, db_value / 10.0);
}

/**
 * @brief Convert linear amplitude to dBm (milliwatts)
 * @param linear_watts Power in watts
 * @return Power in dBm (10 * log10(linear_watts * 1000))
 */
double fso_watts_to_dbm(double linear_watts) {
    if (linear_watts <= 0.0) {
        FSO_LOG_WARNING("MATH", "Non-positive value in watts to dBm conversion");
        return -INFINITY;
    }
    
    return 10.0 * log10(linear_watts * 1000.0);
}

/**
 * @brief Convert dBm to linear power in watts
 * @param dbm_value Power in dBm
 * @return Power in watts (10^(dbm_value/10) / 1000)
 */
double fso_dbm_to_watts(double dbm_value) {
    return pow(10.0, dbm_value / 10.0) / 1000.0;
}

/**
 * @brief Convert linear amplitude ratio to dB
 * @param linear_value Linear amplitude ratio
 * @return Amplitude in dB (20 * log10(linear_value))
 */
double fso_amplitude_to_db(double linear_value) {
    if (linear_value <= 0.0) {
        FSO_LOG_WARNING("MATH", "Non-positive value in amplitude to dB conversion");
        return -INFINITY;
    }
    
    return 20.0 * log10(linear_value);
}

/**
 * @brief Convert dB to linear amplitude ratio
 * @param db_value Amplitude in dB
 * @return Linear amplitude ratio (10^(db_value/20))
 */
double fso_db_to_amplitude(double db_value) {
    return pow(10.0, db_value / 20.0);
}
