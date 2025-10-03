/**
 * @file channel.h
 * @brief Atmospheric turbulence channel modeling for FSO communications
 * 
 * This module implements realistic atmospheric channel effects including:
 * - Log-normal fading and scintillation
 * - Weather-based attenuation (fog, rain, snow)
 * - Path loss calculations
 * - Temporal correlation modeling
 * 
 * Mathematical models:
 * - Rytov variance: σ_χ² = 0.5 * C_n² * k^(7/6) * L^(11/6)
 * - Scintillation index: σ_I² = exp(4σ_χ²) - 1
 * - Free-space path loss: L_fs = (4πd/λ)²
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include "../fso.h"

/* ============================================================================
 * Channel Model Structures
 * ============================================================================ */

/**
 * @brief Channel model state and configuration
 * 
 * Contains all parameters needed to model atmospheric turbulence effects
 * on free-space optical links.
 */
typedef struct {
    /* Link geometry */
    double link_distance;        /**< Propagation distance in meters */
    double wavelength;           /**< Optical wavelength in meters */
    double beam_divergence;      /**< Beam divergence angle in radians */
    
    /* Atmospheric parameters */
    WeatherCondition weather;    /**< Current weather condition */
    double cn2;                  /**< Refractive index structure parameter (m^(-2/3)) */
    double temperature;          /**< Temperature in Celsius */
    double humidity;             /**< Relative humidity (0-1) */
    double visibility;           /**< Visibility in meters (for fog) */
    double rainfall_rate;        /**< Rainfall rate in mm/hr (for rain) */
    double snowfall_rate;        /**< Snowfall rate in mm/hr (for snow) */
    
    /* Fading state */
    double* fade_history;        /**< Temporal correlation buffer */
    int history_length;          /**< Length of fade history buffer */
    int history_index;           /**< Current position in circular buffer */
    double correlation_time;     /**< Correlation time in seconds */
    double last_fade_value;      /**< Last generated fade value */
    
    /* Cached calculations */
    double rytov_variance;       /**< Cached Rytov variance σ_χ² */
    double scintillation_index;  /**< Cached scintillation index σ_I² */
    double path_loss_db;         /**< Cached path loss in dB */
    double attenuation_db;       /**< Cached weather attenuation in dB */
    
    /* Random number generation */
    unsigned int rng_seed;       /**< Random number generator seed */
    
    /* Initialization flag */
    int initialized;             /**< 1 if channel is initialized, 0 otherwise */
} ChannelModel;

/* ============================================================================
 * Channel Model Functions
 * ============================================================================ */

/**
 * @brief Initialize channel model with basic parameters
 * 
 * @param channel Pointer to channel model structure
 * @param distance Link distance in meters (100 to 10000)
 * @param wavelength Optical wavelength in meters (typical: 1550nm = 1.55e-6)
 * @param weather Weather condition
 * @return FSO_SUCCESS on success, error code otherwise
 * 
 * @note This function validates all parameters and initializes internal state.
 *       The channel must be initialized before any other operations.
 */
int channel_init(ChannelModel* channel, double distance, double wavelength,
                 WeatherCondition weather);

/**
 * @brief Initialize channel model with extended parameters
 * 
 * @param channel Pointer to channel model structure
 * @param distance Link distance in meters
 * @param wavelength Optical wavelength in meters
 * @param weather Weather condition
 * @param cn2 Refractive index structure parameter (typical: 1e-15 to 1e-13)
 * @param correlation_time Temporal correlation time in seconds
 * @return FSO_SUCCESS on success, error code otherwise
 */
int channel_init_extended(ChannelModel* channel, double distance, 
                         double wavelength, WeatherCondition weather,
                         double cn2, double correlation_time);

/**
 * @brief Set weather-specific parameters
 * 
 * @param channel Pointer to channel model structure
 * @param visibility Visibility in meters (for fog, typical: 50-1000m)
 * @param rainfall_rate Rainfall rate in mm/hr (for rain, typical: 0-100)
 * @param snowfall_rate Snowfall rate in mm/hr (for snow, typical: 0-50)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int channel_set_weather_params(ChannelModel* channel, double visibility,
                               double rainfall_rate, double snowfall_rate);

/**
 * @brief Set atmospheric parameters
 * 
 * @param channel Pointer to channel model structure
 * @param temperature Temperature in Celsius
 * @param humidity Relative humidity (0-1)
 * @return FSO_SUCCESS on success, error code otherwise
 */
int channel_set_atmospheric_params(ChannelModel* channel, double temperature,
                                   double humidity);

/**
 * @brief Set beam divergence angle
 * 
 * @param channel Pointer to channel model structure
 * @param divergence Beam divergence angle in radians
 * @return FSO_SUCCESS on success, error code otherwise
 */
int channel_set_beam_divergence(ChannelModel* channel, double divergence);

/**
 * @brief Update channel model calculations
 * 
 * Recalculates Rytov variance, scintillation index, path loss, and
 * attenuation based on current parameters. Should be called after
 * changing any channel parameters.
 * 
 * @param channel Pointer to channel model structure
 * @return FSO_SUCCESS on success, error code otherwise
 */
int channel_update_calculations(ChannelModel* channel);

/**
 * @brief Free channel model resources
 * 
 * @param channel Pointer to channel model structure
 */
void channel_free(ChannelModel* channel);

/**
 * @brief Get channel model information string
 * 
 * @param channel Pointer to channel model structure
 * @param buffer Buffer to store information string
 * @param buffer_size Size of buffer
 * @return Number of characters written
 */
int channel_get_info(const ChannelModel* channel, char* buffer, size_t buffer_size);

/* ============================================================================
 * Fading and Attenuation Functions
 * ============================================================================ */

/**
 * @brief Calculate Rytov variance for atmospheric turbulence
 * 
 * Rytov variance: σ_χ² = 0.5 * C_n² * k^(7/6) * L^(11/6)
 * where k = 2π/λ is the wave number
 * 
 * @param cn2 Refractive index structure parameter (m^(-2/3))
 * @param wavelength Optical wavelength in meters
 * @param distance Link distance in meters
 * @return Rytov variance σ_χ²
 */
double channel_calculate_rytov_variance(double cn2, double wavelength, double distance);

/**
 * @brief Calculate scintillation index from Rytov variance
 * 
 * Scintillation index: σ_I² = exp(4σ_χ²) - 1
 * 
 * @param rytov_variance Rytov variance σ_χ²
 * @return Scintillation index σ_I²
 */
double channel_calculate_scintillation_index(double rytov_variance);

/**
 * @brief Generate log-normal fading sample
 * 
 * Generates a random fading coefficient from log-normal distribution
 * based on the scintillation index.
 * 
 * @param channel Pointer to channel model structure
 * @return Fading coefficient (linear scale)
 */
double channel_generate_fading(ChannelModel* channel);

/**
 * @brief Generate temporally correlated fading sample
 * 
 * Uses AR(1) process to generate time-correlated fading:
 * X(t) = ρ*X(t-1) + sqrt(1-ρ²)*W(t)
 * where ρ is the correlation coefficient and W(t) is white noise
 * 
 * @param channel Pointer to channel model structure
 * @param time_step Time step in seconds
 * @return Fading coefficient (linear scale)
 */
double channel_generate_correlated_fading(ChannelModel* channel, double time_step);

/**
 * @brief Calculate weather-based attenuation
 * 
 * Calculates attenuation in dB/km based on weather conditions:
 * - Clear: ~0.1 dB/km
 * - Fog: Kim model (visibility-based)
 * - Rain: Carbonneau model (rainfall rate)
 * - Snow: Snowfall rate model
 * 
 * @param channel Pointer to channel model structure
 * @return Attenuation in dB/km
 */
double channel_calculate_attenuation(ChannelModel* channel);

/**
 * @brief Calculate free-space path loss
 * 
 * Free-space path loss: L_fs = (4πd/λ)²
 * Returns value in dB: L_fs(dB) = 20*log10(4πd/λ)
 * 
 * @param distance Link distance in meters
 * @param wavelength Optical wavelength in meters
 * @return Path loss in dB
 */
double channel_calculate_path_loss(double distance, double wavelength);

/**
 * @brief Calculate geometric loss due to beam divergence
 * 
 * @param distance Link distance in meters
 * @param divergence Beam divergence angle in radians
 * @param receiver_aperture Receiver aperture diameter in meters
 * @return Geometric loss in dB
 */
double channel_calculate_geometric_loss(double distance, double divergence,
                                        double receiver_aperture);

/**
 * @brief Calculate atmospheric absorption
 * 
 * Molecular absorption by atmospheric gases (primarily water vapor and CO2)
 * 
 * @param wavelength Optical wavelength in meters
 * @param distance Link distance in meters
 * @param humidity Relative humidity (0-1)
 * @return Absorption loss in dB
 */
double channel_calculate_atmospheric_absorption(double wavelength, double distance,
                                                double humidity);

/**
 * @brief Apply channel effects to input signal power
 * 
 * Combines fading, attenuation, and path loss to calculate received power.
 * Also adds AWGN (Additive White Gaussian Noise) if noise_power > 0.
 * 
 * @param channel Pointer to channel model structure
 * @param input_power Input signal power in watts
 * @param noise_power Noise power in watts (0 for no noise)
 * @param time_step Time step for temporal correlation (seconds)
 * @return Received signal power in watts
 */
double channel_apply_effects(ChannelModel* channel, double input_power,
                             double noise_power, double time_step);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get default C_n² value for weather condition
 * 
 * @param weather Weather condition
 * @return Typical C_n² value in m^(-2/3)
 */
double channel_get_default_cn2(WeatherCondition weather);

/**
 * @brief Get weather condition name string
 * 
 * @param weather Weather condition
 * @return String representation of weather condition
 */
const char* channel_get_weather_name(WeatherCondition weather);

/**
 * @brief Validate channel parameters
 * 
 * @param distance Link distance in meters
 * @param wavelength Optical wavelength in meters
 * @param cn2 Refractive index structure parameter
 * @return FSO_SUCCESS if valid, error code otherwise
 */
int channel_validate_params(double distance, double wavelength, double cn2);

#endif /* CHANNEL_H */
