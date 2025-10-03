/**
 * @file fso.h
 * @brief Main header file for FSO Communication Protocol Suite
 * 
 * This file contains common types, error codes, and logging macros
 * used throughout the FSO communication system.
 */

#ifndef FSO_H
#define FSO_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    FSO_SUCCESS = 0,                /**< Operation completed successfully */
    FSO_ERROR_INVALID_PARAM = -1,   /**< Invalid parameter provided */
    FSO_ERROR_MEMORY = -2,          /**< Memory allocation failed */
    FSO_ERROR_NOT_INITIALIZED = -3, /**< Component not initialized */
    FSO_ERROR_CONVERGENCE = -4,     /**< Algorithm failed to converge */
    FSO_ERROR_UNSUPPORTED = -5,     /**< Unsupported operation or feature */
    FSO_ERROR_IO = -6               /**< Input/output error */
} FSOErrorCode;

/* ============================================================================
 * Common Types
 * ============================================================================ */

/**
 * @brief Complex number representation
 */
typedef struct {
    double real;  /**< Real component */
    double imag;  /**< Imaginary component */
} ComplexSample;

/**
 * @brief Signal buffer structure
 */
typedef struct {
    ComplexSample* samples;  /**< Array of complex samples */
    size_t length;           /**< Number of samples */
    double sample_rate;      /**< Sampling rate in Hz */
    double timestamp;        /**< Timestamp of first sample */
} SignalBuffer;

/**
 * @brief Modulation type enumeration
 */
typedef enum {
    MOD_OOK,   /**< On-Off Keying */
    MOD_PPM,   /**< Pulse Position Modulation */
    MOD_DPSK   /**< Differential Phase Shift Keying */
} ModulationType;

/**
 * @brief Forward Error Correction type enumeration
 */
typedef enum {
    FEC_REED_SOLOMON,  /**< Reed-Solomon codes */
    FEC_LDPC           /**< Low-Density Parity-Check codes */
} FECType;

/**
 * @brief Weather condition enumeration
 */
typedef enum {
    WEATHER_CLEAR,           /**< Clear atmospheric conditions */
    WEATHER_FOG,             /**< Foggy conditions */
    WEATHER_RAIN,            /**< Rainy conditions */
    WEATHER_SNOW,            /**< Snowy conditions */
    WEATHER_HIGH_TURBULENCE  /**< High atmospheric turbulence */
} WeatherCondition;

/* ============================================================================
 * Logging System
 * ============================================================================ */

/**
 * @brief Log level enumeration
 */
typedef enum {
    LOG_DEBUG,    /**< Debug messages */
    LOG_INFO,     /**< Informational messages */
    LOG_WARNING,  /**< Warning messages */
    LOG_ERROR     /**< Error messages */
} LogLevel;

/**
 * @brief Global log level (can be set by user)
 */
extern LogLevel g_log_level;

/**
 * @brief Set the global log level
 * @param level New log level to set
 */
void fso_set_log_level(LogLevel level);

/**
 * @brief Get the current global log level
 * @return Current log level
 */
LogLevel fso_get_log_level(void);

/**
 * @brief Get string representation of error code
 * @param error_code Error code to convert
 * @return String representation of error code
 */
const char* fso_error_string(FSOErrorCode error_code);

/**
 * @brief Get current timestamp string
 * @param buffer Buffer to store timestamp (min 32 bytes)
 * @return Pointer to buffer
 */
static inline char* fso_get_timestamp(char* buffer) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

/**
 * @brief Log a message with specified level
 * @param level Log level
 * @param module Module name
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
#define FSO_LOG(level, module, format, ...) \
    do { \
        if (level >= g_log_level) { \
            char timestamp[32]; \
            fso_get_timestamp(timestamp); \
            const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"}; \
            fprintf(stderr, "[%s] [%s] [%s] " format "\n", \
                    timestamp, level_str[level], module, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Convenience macros for different log levels
 */
#define FSO_LOG_DEBUG(module, format, ...)   FSO_LOG(LOG_DEBUG, module, format, ##__VA_ARGS__)
#define FSO_LOG_INFO(module, format, ...)    FSO_LOG(LOG_INFO, module, format, ##__VA_ARGS__)
#define FSO_LOG_WARNING(module, format, ...) FSO_LOG(LOG_WARNING, module, format, ##__VA_ARGS__)
#define FSO_LOG_ERROR(module, format, ...)   FSO_LOG(LOG_ERROR, module, format, ##__VA_ARGS__)

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/**
 * @brief Check if pointer is NULL and return error
 */
#define FSO_CHECK_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            FSO_LOG_ERROR("CHECK", "NULL pointer: %s", #ptr); \
            return FSO_ERROR_INVALID_PARAM; \
        } \
    } while(0)

/**
 * @brief Check parameter condition and return error if false
 */
#define FSO_CHECK_PARAM(condition) \
    do { \
        if (!(condition)) { \
            FSO_LOG_ERROR("CHECK", "Parameter check failed: %s", #condition); \
            return FSO_ERROR_INVALID_PARAM; \
        } \
    } while(0)

/**
 * @brief Minimum of two values
 */
#define FSO_MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Maximum of two values
 */
#define FSO_MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Clamp value between min and max
 */
#define FSO_CLAMP(x, min, max) FSO_MAX(min, FSO_MIN(x, max))

/* ============================================================================
 * Complex Number Operations
 * ============================================================================ */

/**
 * @brief Add two complex numbers
 * @param a First complex number
 * @param b Second complex number
 * @return Sum a + b
 */
ComplexSample fso_complex_add(ComplexSample a, ComplexSample b);

/**
 * @brief Subtract two complex numbers
 * @param a First complex number
 * @param b Second complex number
 * @return Difference a - b
 */
ComplexSample fso_complex_sub(ComplexSample a, ComplexSample b);

/**
 * @brief Multiply two complex numbers
 * @param a First complex number
 * @param b Second complex number
 * @return Product a * b
 */
ComplexSample fso_complex_mul(ComplexSample a, ComplexSample b);

/**
 * @brief Divide two complex numbers
 * @param a Numerator
 * @param b Denominator
 * @return Quotient a / b
 */
ComplexSample fso_complex_div(ComplexSample a, ComplexSample b);

/**
 * @brief Calculate magnitude of complex number
 * @param c Complex number
 * @return Magnitude |c|
 */
double fso_complex_magnitude(ComplexSample c);

/**
 * @brief Calculate squared magnitude of complex number
 * @param c Complex number
 * @return Squared magnitude |c|^2
 */
double fso_complex_magnitude_squared(ComplexSample c);

/**
 * @brief Calculate phase of complex number
 * @param c Complex number
 * @return Phase in radians
 */
double fso_complex_phase(ComplexSample c);

/**
 * @brief Calculate complex conjugate
 * @param c Complex number
 * @return Conjugate c*
 */
ComplexSample fso_complex_conjugate(ComplexSample c);

/**
 * @brief Create complex number from magnitude and phase
 * @param magnitude Magnitude
 * @param phase Phase in radians
 * @return Complex number
 */
ComplexSample fso_complex_from_polar(double magnitude, double phase);

/**
 * @brief Multiply complex number by real scalar
 * @param c Complex number
 * @param scalar Real scalar value
 * @return Product scalar * c
 */
ComplexSample fso_complex_scale(ComplexSample c, double scalar);

/* ============================================================================
 * Signal Power Calculations
 * ============================================================================ */

/**
 * @brief Calculate average power of real signal
 * @param signal Array of signal samples
 * @param length Number of samples
 * @return Average power
 */
double fso_signal_power_real(const double* signal, size_t length);

/**
 * @brief Calculate average power of complex signal
 * @param signal Array of complex signal samples
 * @param length Number of samples
 * @return Average power
 */
double fso_signal_power_complex(const ComplexSample* signal, size_t length);

/**
 * @brief Calculate RMS value of real signal
 * @param signal Array of signal samples
 * @param length Number of samples
 * @return RMS value
 */
double fso_signal_rms(const double* signal, size_t length);

/**
 * @brief Calculate peak power of real signal
 * @param signal Array of signal samples
 * @param length Number of samples
 * @return Peak power
 */
double fso_signal_peak_power(const double* signal, size_t length);

/**
 * @brief Calculate Signal-to-Noise Ratio (SNR)
 * @param signal_power Signal power (linear scale)
 * @param noise_power Noise power (linear scale)
 * @return SNR in dB
 */
double fso_calculate_snr(double signal_power, double noise_power);

/* ============================================================================
 * dB Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert linear power to dB
 * @param linear_value Linear power value
 * @return Power in dB
 */
double fso_linear_to_db(double linear_value);

/**
 * @brief Convert dB to linear power
 * @param db_value Power in dB
 * @return Linear power value
 */
double fso_db_to_linear(double db_value);

/**
 * @brief Convert linear power in watts to dBm
 * @param linear_watts Power in watts
 * @return Power in dBm
 */
double fso_watts_to_dbm(double linear_watts);

/**
 * @brief Convert dBm to linear power in watts
 * @param dbm_value Power in dBm
 * @return Power in watts
 */
double fso_dbm_to_watts(double dbm_value);

/**
 * @brief Convert linear amplitude ratio to dB
 * @param linear_value Linear amplitude ratio
 * @return Amplitude in dB
 */
double fso_amplitude_to_db(double linear_value);

/**
 * @brief Convert dB to linear amplitude ratio
 * @param db_value Amplitude in dB
 * @return Linear amplitude ratio
 */
double fso_db_to_amplitude(double db_value);

/* ============================================================================
 * Random Number Generation
 * ============================================================================ */

/**
 * @brief Initialize random number generator with seed
 * @param seed Seed value (use 0 for time-based seed)
 */
void fso_random_init(unsigned int seed);

/**
 * @brief Set seed for current thread
 * @param seed Seed value
 */
void fso_random_set_seed(unsigned int seed);

/**
 * @brief Get current seed for current thread
 * @return Current seed value
 */
unsigned int fso_random_get_seed(void);

/**
 * @brief Generate uniform random number in [0, 1)
 * @return Random number in [0, 1)
 */
double fso_random_uniform(void);

/**
 * @brief Generate uniform random number in [min, max)
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 * @return Random number in [min, max)
 */
double fso_random_uniform_range(double min, double max);

/**
 * @brief Generate Gaussian (normal) random number using Box-Muller transform
 * @param mean Mean of the distribution
 * @param stddev Standard deviation of the distribution
 * @return Random number from N(mean, stddev^2)
 */
double fso_random_gaussian(double mean, double stddev);

/**
 * @brief Generate standard normal random number (mean=0, stddev=1)
 * @return Random number from N(0, 1)
 */
double fso_random_normal(void);

/**
 * @brief Generate random integer in [min, max]
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return Random integer in [min, max]
 */
int fso_random_int(int min, int max);

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FSO_PI 3.14159265358979323846
#define FSO_SPEED_OF_LIGHT 299792458.0  /**< Speed of light in m/s */

#endif /* FSO_H */
