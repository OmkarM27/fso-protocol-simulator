/**
 * @file channel.c
 * @brief Implementation of atmospheric turbulence channel modeling
 */

#include "channel.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Module name for logging */
#define MODULE_NAME "CHANNEL"

/* Default parameters */
#define DEFAULT_CN2_CLEAR 1e-15
#define DEFAULT_CN2_FOG 5e-15
#define DEFAULT_CN2_RAIN 1e-14
#define DEFAULT_CN2_SNOW 2e-14
#define DEFAULT_CN2_HIGH_TURB 1e-13
#define DEFAULT_CORRELATION_TIME 0.001  /* 1 ms */
#define DEFAULT_HISTORY_LENGTH 100
#define DEFAULT_BEAM_DIVERGENCE 1e-3    /* 1 mrad */

/* Parameter validation ranges */
#define MIN_DISTANCE 100.0              /* 100 meters */
#define MAX_DISTANCE 10000.0            /* 10 km */
#define MIN_WAVELENGTH 500e-9           /* 500 nm */
#define MAX_WAVELENGTH 2000e-9          /* 2000 nm */
#define MIN_CN2 1e-17
#define MAX_CN2 1e-12

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get default C_n² value for weather condition
 */
double channel_get_default_cn2(WeatherCondition weather) {
    switch (weather) {
        case WEATHER_CLEAR:
            return DEFAULT_CN2_CLEAR;
        case WEATHER_FOG:
            return DEFAULT_CN2_FOG;
        case WEATHER_RAIN:
            return DEFAULT_CN2_RAIN;
        case WEATHER_SNOW:
            return DEFAULT_CN2_SNOW;
        case WEATHER_HIGH_TURBULENCE:
            return DEFAULT_CN2_HIGH_TURB;
        default:
            return DEFAULT_CN2_CLEAR;
    }
}

/**
 * @brief Get weather condition name string
 */
const char* channel_get_weather_name(WeatherCondition weather) {
    switch (weather) {
        case WEATHER_CLEAR:
            return "Clear";
        case WEATHER_FOG:
            return "Fog";
        case WEATHER_RAIN:
            return "Rain";
        case WEATHER_SNOW:
            return "Snow";
        case WEATHER_HIGH_TURBULENCE:
            return "High Turbulence";
        default:
            return "Unknown";
    }
}

/**
 * @brief Validate channel parameters
 */
int channel_validate_params(double distance, double wavelength, double cn2) {
    if (distance < MIN_DISTANCE || distance > MAX_DISTANCE) {
        FSO_LOG_ERROR(MODULE_NAME, "Invalid distance: %.2f m (valid range: %.0f - %.0f m)",
                     distance, MIN_DISTANCE, MAX_DISTANCE);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (wavelength < MIN_WAVELENGTH || wavelength > MAX_WAVELENGTH) {
        FSO_LOG_ERROR(MODULE_NAME, "Invalid wavelength: %.2e m (valid range: %.2e - %.2e m)",
                     wavelength, MIN_WAVELENGTH, MAX_WAVELENGTH);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (cn2 < MIN_CN2 || cn2 > MAX_CN2) {
        FSO_LOG_ERROR(MODULE_NAME, "Invalid Cn2: %.2e (valid range: %.2e - %.2e)",
                     cn2, MIN_CN2, MAX_CN2);
        return FSO_ERROR_INVALID_PARAM;
    }
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

/**
 * @brief Initialize channel model with basic parameters
 */
int channel_init(ChannelModel* channel, double distance, double wavelength,
                 WeatherCondition weather) {
    FSO_CHECK_NULL(channel);
    
    /* Get default Cn2 for weather condition */
    double cn2 = channel_get_default_cn2(weather);
    
    /* Validate parameters */
    int result = channel_validate_params(distance, wavelength, cn2);
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    /* Initialize with extended parameters */
    return channel_init_extended(channel, distance, wavelength, weather,
                                 cn2, DEFAULT_CORRELATION_TIME);
}

/**
 * @brief Initialize channel model with extended parameters
 */
int channel_init_extended(ChannelModel* channel, double distance,
                         double wavelength, WeatherCondition weather,
                         double cn2, double correlation_time) {
    FSO_CHECK_NULL(channel);
    
    /* Validate parameters */
    int result = channel_validate_params(distance, wavelength, cn2);
    if (result != FSO_SUCCESS) {
        return result;
    }
    
    FSO_CHECK_PARAM(correlation_time > 0.0 && correlation_time < 1.0);
    
    /* Clear structure */
    memset(channel, 0, sizeof(ChannelModel));
    
    /* Set link geometry */
    channel->link_distance = distance;
    channel->wavelength = wavelength;
    channel->beam_divergence = DEFAULT_BEAM_DIVERGENCE;
    
    /* Set atmospheric parameters */
    channel->weather = weather;
    channel->cn2 = cn2;
    channel->temperature = 20.0;  /* Default 20°C */
    channel->humidity = 0.5;      /* Default 50% */
    channel->visibility = 1000.0; /* Default 1 km */
    channel->rainfall_rate = 0.0;
    channel->snowfall_rate = 0.0;
    
    /* Set weather-specific defaults */
    switch (weather) {
        case WEATHER_FOG:
            channel->visibility = 200.0;  /* 200m visibility */
            break;
        case WEATHER_RAIN:
            channel->rainfall_rate = 10.0;  /* 10 mm/hr */
            break;
        case WEATHER_SNOW:
            channel->snowfall_rate = 5.0;   /* 5 mm/hr */
            break;
        case WEATHER_HIGH_TURBULENCE:
            channel->cn2 = DEFAULT_CN2_HIGH_TURB;
            break;
        default:
            break;
    }
    
    /* Initialize fading state */
    channel->correlation_time = correlation_time;
    channel->history_length = DEFAULT_HISTORY_LENGTH;
    channel->history_index = 0;
    channel->last_fade_value = 1.0;  /* Start with no fading */
    
    /* Allocate fade history buffer */
    channel->fade_history = (double*)calloc(channel->history_length, sizeof(double));
    if (channel->fade_history == NULL) {
        FSO_LOG_ERROR(MODULE_NAME, "Failed to allocate fade history buffer");
        return FSO_ERROR_MEMORY;
    }
    
    /* Initialize history with 1.0 (no fading) */
    for (int i = 0; i < channel->history_length; i++) {
        channel->fade_history[i] = 1.0;
    }
    
    /* Initialize random number generator */
    channel->rng_seed = (unsigned int)time(NULL);
    fso_random_set_seed(channel->rng_seed);
    
    /* Calculate initial values */
    result = channel_update_calculations(channel);
    if (result != FSO_SUCCESS) {
        free(channel->fade_history);
        return result;
    }
    
    channel->initialized = 1;
    
    FSO_LOG_INFO(MODULE_NAME, "Channel initialized: distance=%.1f m, wavelength=%.0f nm, weather=%s",
                distance, wavelength * 1e9, channel_get_weather_name(weather));
    FSO_LOG_DEBUG(MODULE_NAME, "Cn2=%.2e, Rytov variance=%.4f, Scintillation index=%.4f",
                 cn2, channel->rytov_variance, channel->scintillation_index);
    
    return FSO_SUCCESS;
}

/**
 * @brief Set weather-specific parameters
 */
int channel_set_weather_params(ChannelModel* channel, double visibility,
                               double rainfall_rate, double snowfall_rate) {
    FSO_CHECK_NULL(channel);
    
    if (!channel->initialized) {
        FSO_LOG_ERROR(MODULE_NAME, "Channel not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    FSO_CHECK_PARAM(visibility > 0.0);
    FSO_CHECK_PARAM(rainfall_rate >= 0.0);
    FSO_CHECK_PARAM(snowfall_rate >= 0.0);
    
    channel->visibility = visibility;
    channel->rainfall_rate = rainfall_rate;
    channel->snowfall_rate = snowfall_rate;
    
    /* Recalculate attenuation */
    return channel_update_calculations(channel);
}

/**
 * @brief Set atmospheric parameters
 */
int channel_set_atmospheric_params(ChannelModel* channel, double temperature,
                                   double humidity) {
    FSO_CHECK_NULL(channel);
    
    if (!channel->initialized) {
        FSO_LOG_ERROR(MODULE_NAME, "Channel not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    FSO_CHECK_PARAM(temperature >= -50.0 && temperature <= 50.0);
    FSO_CHECK_PARAM(humidity >= 0.0 && humidity <= 1.0);
    
    channel->temperature = temperature;
    channel->humidity = humidity;
    
    /* Recalculate absorption */
    return channel_update_calculations(channel);
}

/**
 * @brief Set beam divergence angle
 */
int channel_set_beam_divergence(ChannelModel* channel, double divergence) {
    FSO_CHECK_NULL(channel);
    
    if (!channel->initialized) {
        FSO_LOG_ERROR(MODULE_NAME, "Channel not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    FSO_CHECK_PARAM(divergence > 0.0 && divergence < 0.1);  /* Max 100 mrad */
    
    channel->beam_divergence = divergence;
    
    return FSO_SUCCESS;
}

/**
 * @brief Update channel model calculations
 */
int channel_update_calculations(ChannelModel* channel) {
    FSO_CHECK_NULL(channel);
    
    if (!channel->initialized) {
        FSO_LOG_ERROR(MODULE_NAME, "Channel not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    /* Calculate Rytov variance */
    channel->rytov_variance = channel_calculate_rytov_variance(
        channel->cn2, channel->wavelength, channel->link_distance);
    
    /* Calculate scintillation index */
    channel->scintillation_index = channel_calculate_scintillation_index(
        channel->rytov_variance);
    
    /* Calculate path loss */
    channel->path_loss_db = channel_calculate_path_loss(
        channel->link_distance, channel->wavelength);
    
    /* Calculate weather attenuation */
    channel->attenuation_db = channel_calculate_attenuation(channel);
    
    FSO_LOG_DEBUG(MODULE_NAME, "Updated calculations: Rytov=%.4f, Scint=%.4f, PathLoss=%.2f dB, Atten=%.2f dB",
                 channel->rytov_variance, channel->scintillation_index,
                 channel->path_loss_db, channel->attenuation_db);
    
    return FSO_SUCCESS;
}

/**
 * @brief Free channel model resources
 */
void channel_free(ChannelModel* channel) {
    if (channel == NULL) {
        return;
    }
    
    if (channel->fade_history != NULL) {
        free(channel->fade_history);
        channel->fade_history = NULL;
    }
    
    channel->initialized = 0;
    
    FSO_LOG_DEBUG(MODULE_NAME, "Channel freed");
}

/**
 * @brief Get channel model information string
 */
int channel_get_info(const ChannelModel* channel, char* buffer, size_t buffer_size) {
    if (channel == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }
    
    if (!channel->initialized) {
        return snprintf(buffer, buffer_size, "Channel not initialized");
    }
    
    return snprintf(buffer, buffer_size,
        "Channel Model:\n"
        "  Distance: %.1f m\n"
        "  Wavelength: %.0f nm\n"
        "  Weather: %s\n"
        "  Cn2: %.2e m^(-2/3)\n"
        "  Rytov variance: %.4f\n"
        "  Scintillation index: %.4f\n"
        "  Path loss: %.2f dB\n"
        "  Attenuation: %.2f dB/km\n"
        "  Temperature: %.1f °C\n"
        "  Humidity: %.1f%%",
        channel->link_distance,
        channel->wavelength * 1e9,
        channel_get_weather_name(channel->weather),
        channel->cn2,
        channel->rytov_variance,
        channel->scintillation_index,
        channel->path_loss_db,
        channel->attenuation_db,
        channel->temperature,
        channel->humidity * 100.0);
}

/* ============================================================================
 * Fading Model Functions
 * ============================================================================ */

/**
 * @brief Calculate Rytov variance for atmospheric turbulence
 * 
 * Rytov variance: σ_χ² = 0.5 * C_n² * k^(7/6) * L^(11/6)
 * where k = 2π/λ is the wave number
 */
double channel_calculate_rytov_variance(double cn2, double wavelength, double distance) {
    /* Wave number k = 2π/λ */
    double k = 2.0 * FSO_PI / wavelength;
    
    /* Calculate Rytov variance: σ_χ² = 0.5 * C_n² * k^(7/6) * L^(11/6) */
    double k_power = pow(k, 7.0 / 6.0);
    double L_power = pow(distance, 11.0 / 6.0);
    double rytov_variance = 0.5 * cn2 * k_power * L_power;
    
    return rytov_variance;
}

/**
 * @brief Calculate scintillation index from Rytov variance
 * 
 * Scintillation index: σ_I² = exp(4σ_χ²) - 1
 * 
 * For weak turbulence (σ_χ² < 0.3): σ_I² ≈ 4σ_χ²
 * For moderate turbulence: use full formula
 * For strong turbulence (σ_χ² > 1): saturates
 */
double channel_calculate_scintillation_index(double rytov_variance) {
    /* Weak turbulence approximation */
    if (rytov_variance < 0.3) {
        return 4.0 * rytov_variance;
    }
    
    /* Full formula for moderate turbulence */
    double scint_index = exp(4.0 * rytov_variance) - 1.0;
    
    /* Saturation for strong turbulence */
    if (scint_index > 10.0) {
        scint_index = 10.0;
    }
    
    return scint_index;
}

/**
 * @brief Generate log-normal fading sample
 * 
 * Log-normal fading: I = exp(2X) where X ~ N(0, σ_χ²)
 * Mean of I: E[I] = exp(2σ_χ²)
 * To normalize: I_normalized = I / E[I] = exp(2X - 2σ_χ²)
 */
double channel_generate_fading(ChannelModel* channel) {
    if (channel == NULL || !channel->initialized) {
        return 1.0;  /* No fading */
    }
    
    /* For very weak turbulence, skip fading calculation */
    if (channel->rytov_variance < 1e-6) {
        return 1.0;
    }
    
    /* Generate Gaussian random variable X ~ N(0, σ_χ) */
    double sigma_chi = sqrt(channel->rytov_variance);
    double X = fso_random_gaussian(0.0, sigma_chi);
    
    /* Calculate log-normal fading: I = exp(2X - 2σ_χ²) */
    /* The subtraction of 2σ_χ² normalizes the mean to 1.0 */
    double log_amplitude = 2.0 * X - 2.0 * channel->rytov_variance;
    double fading_coefficient = exp(log_amplitude);
    
    /* Clamp to reasonable range to avoid numerical issues */
    if (fading_coefficient < 0.01) {
        fading_coefficient = 0.01;  /* -20 dB fade */
    } else if (fading_coefficient > 100.0) {
        fading_coefficient = 100.0;  /* +20 dB fade */
    }
    
    return fading_coefficient;
}

/**
 * @brief Generate temporally correlated fading sample
 * 
 * Uses AR(1) (Auto-Regressive order 1) process to generate time-correlated fading:
 * X(t) = ρ*X(t-1) + sqrt(1-ρ²)*W(t)
 * 
 * where:
 * - ρ is the correlation coefficient: ρ = exp(-Δt/τ_c)
 * - τ_c is the correlation time
 * - W(t) is white Gaussian noise
 * - Δt is the time step
 * 
 * The fading coefficient is then: I(t) = exp(2*X(t) - 2*σ_χ²)
 */
double channel_generate_correlated_fading(ChannelModel* channel, double time_step) {
    if (channel == NULL || !channel->initialized) {
        return 1.0;  /* No fading */
    }
    
    /* For very weak turbulence, skip fading calculation */
    if (channel->rytov_variance < 1e-6) {
        return 1.0;
    }
    
    /* Calculate correlation coefficient: ρ = exp(-Δt/τ_c) */
    double rho = exp(-time_step / channel->correlation_time);
    
    /* Generate white Gaussian noise W(t) ~ N(0, σ_χ) */
    double sigma_chi = sqrt(channel->rytov_variance);
    double white_noise = fso_random_gaussian(0.0, sigma_chi);
    
    /* Get previous log-amplitude value */
    /* Convert last fade value back to log-amplitude */
    double last_log_amplitude;
    if (channel->last_fade_value > 0.0) {
        last_log_amplitude = log(channel->last_fade_value) / 2.0 + channel->rytov_variance;
    } else {
        last_log_amplitude = 0.0;
    }
    
    /* AR(1) process: X(t) = ρ*X(t-1) + sqrt(1-ρ²)*W(t) */
    double current_log_amplitude = rho * last_log_amplitude + 
                                   sqrt(1.0 - rho * rho) * white_noise;
    
    /* Calculate log-normal fading: I = exp(2X - 2σ_χ²) */
    double log_amplitude = 2.0 * current_log_amplitude - 2.0 * channel->rytov_variance;
    double fading_coefficient = exp(log_amplitude);
    
    /* Clamp to reasonable range */
    if (fading_coefficient < 0.01) {
        fading_coefficient = 0.01;  /* -20 dB fade */
    } else if (fading_coefficient > 100.0) {
        fading_coefficient = 100.0;  /* +20 dB fade */
    }
    
    /* Store in history buffer (circular buffer) */
    channel->fade_history[channel->history_index] = fading_coefficient;
    channel->history_index = (channel->history_index + 1) % channel->history_length;
    
    /* Update last fade value */
    channel->last_fade_value = fading_coefficient;
    
    return fading_coefficient;
}

/* ============================================================================
 * Attenuation Model Functions
 * ============================================================================ */

/**
 * @brief Calculate fog attenuation using Kim model
 * 
 * Kim model relates attenuation to visibility:
 * α_fog = 3.91/V * (λ/550nm)^(-q)
 * 
 * where:
 * - V is visibility in km
 * - λ is wavelength in nm
 * - q is the size distribution parameter (typically 1.3 for fog)
 * 
 * @param visibility Visibility in meters
 * @param wavelength Wavelength in meters
 * @return Attenuation in dB/km
 */
static double calculate_fog_attenuation(double visibility, double wavelength) {
    /* Convert visibility to km */
    double V_km = visibility / 1000.0;
    
    /* Avoid division by zero */
    if (V_km < 0.01) {
        V_km = 0.01;  /* 10 meters minimum */
    }
    
    /* Convert wavelength to nm */
    double lambda_nm = wavelength * 1e9;
    
    /* Size distribution parameter for fog */
    double q = 1.3;
    
    /* Kim model: α = 3.91/V * (λ/550)^(-q) */
    double wavelength_factor = pow(lambda_nm / 550.0, -q);
    double attenuation = (3.91 / V_km) * wavelength_factor;
    
    return attenuation;
}

/**
 * @brief Calculate rain attenuation using Carbonneau model
 * 
 * Carbonneau model for rain attenuation:
 * α_rain = 1.076 * R^0.67
 * 
 * where R is rainfall rate in mm/hr
 * 
 * @param rainfall_rate Rainfall rate in mm/hr
 * @return Attenuation in dB/km
 */
static double calculate_rain_attenuation(double rainfall_rate) {
    if (rainfall_rate <= 0.0) {
        return 0.0;
    }
    
    /* Carbonneau model: α = 1.076 * R^0.67 */
    double attenuation = 1.076 * pow(rainfall_rate, 0.67);
    
    return attenuation;
}

/**
 * @brief Calculate snow attenuation
 * 
 * Snow attenuation model (empirical):
 * α_snow = a * S^b
 * 
 * where:
 * - S is snowfall rate in mm/hr (liquid water equivalent)
 * - a ≈ 1.023 dB/km per (mm/hr)
 * - b ≈ 0.72
 * 
 * @param snowfall_rate Snowfall rate in mm/hr
 * @return Attenuation in dB/km
 */
static double calculate_snow_attenuation(double snowfall_rate) {
    if (snowfall_rate <= 0.0) {
        return 0.0;
    }
    
    /* Empirical snow model: α = 1.023 * S^0.72 */
    double attenuation = 1.023 * pow(snowfall_rate, 0.72);
    
    return attenuation;
}

/**
 * @brief Calculate clear air attenuation
 * 
 * Clear air has minimal attenuation, primarily from molecular scattering
 * Typical value: ~0.1 dB/km for optical wavelengths
 * 
 * @return Attenuation in dB/km
 */
static double calculate_clear_air_attenuation(void) {
    return 0.1;  /* dB/km */
}

/**
 * @brief Calculate weather-based attenuation
 * 
 * Calculates total attenuation in dB/km based on weather conditions
 */
double channel_calculate_attenuation(ChannelModel* channel) {
    if (channel == NULL || !channel->initialized) {
        return 0.0;
    }
    
    double attenuation_db_per_km = 0.0;
    
    switch (channel->weather) {
        case WEATHER_CLEAR:
            attenuation_db_per_km = calculate_clear_air_attenuation();
            break;
            
        case WEATHER_FOG:
            attenuation_db_per_km = calculate_fog_attenuation(
                channel->visibility, channel->wavelength);
            break;
            
        case WEATHER_RAIN:
            attenuation_db_per_km = calculate_rain_attenuation(
                channel->rainfall_rate);
            /* Add clear air component */
            attenuation_db_per_km += calculate_clear_air_attenuation();
            break;
            
        case WEATHER_SNOW:
            attenuation_db_per_km = calculate_snow_attenuation(
                channel->snowfall_rate);
            /* Add clear air component */
            attenuation_db_per_km += calculate_clear_air_attenuation();
            break;
            
        case WEATHER_HIGH_TURBULENCE:
            /* High turbulence doesn't add attenuation, just fading */
            attenuation_db_per_km = calculate_clear_air_attenuation();
            break;
            
        default:
            attenuation_db_per_km = calculate_clear_air_attenuation();
            break;
    }
    
    return attenuation_db_per_km;
}

/* ============================================================================
 * Path Loss Functions
 * ============================================================================ */

/**
 * @brief Calculate free-space path loss
 * 
 * Free-space path loss (Friis transmission equation):
 * L_fs = (4πd/λ)²
 * 
 * In dB: L_fs(dB) = 20*log10(4πd/λ)
 *                 = 20*log10(d) + 20*log10(f) + 20*log10(4π/c)
 * 
 * @param distance Link distance in meters
 * @param wavelength Optical wavelength in meters
 * @return Path loss in dB
 */
double channel_calculate_path_loss(double distance, double wavelength) {
    /* Calculate path loss: L_fs = (4πd/λ)² */
    double ratio = (4.0 * FSO_PI * distance) / wavelength;
    
    /* Convert to dB: 10*log10(ratio²) = 20*log10(ratio) */
    double path_loss_db = 20.0 * log10(ratio);
    
    return path_loss_db;
}

/**
 * @brief Calculate geometric loss due to beam divergence
 * 
 * Geometric loss occurs when the beam spreads and only part of it
 * is captured by the receiver aperture.
 * 
 * Beam radius at distance d: r(d) = r_0 + θ*d
 * where r_0 is initial beam radius and θ is divergence angle
 * 
 * Geometric loss: L_g = (r(d) / r_rx)²
 * where r_rx is receiver aperture radius
 * 
 * @param distance Link distance in meters
 * @param divergence Beam divergence angle in radians
 * @param receiver_aperture Receiver aperture diameter in meters
 * @return Geometric loss in dB
 */
double channel_calculate_geometric_loss(double distance, double divergence,
                                        double receiver_aperture) {
    /* Initial beam radius (assume collimated beam, r_0 ≈ 0) */
    double beam_radius = divergence * distance;
    
    /* Receiver aperture radius */
    double receiver_radius = receiver_aperture / 2.0;
    
    /* If beam is smaller than receiver, no geometric loss */
    if (beam_radius <= receiver_radius) {
        return 0.0;
    }
    
    /* Geometric loss: L_g = (r_beam / r_rx)² */
    double ratio = beam_radius / receiver_radius;
    double geometric_loss_db = 20.0 * log10(ratio);
    
    return geometric_loss_db;
}

/**
 * @brief Calculate atmospheric absorption
 * 
 * Molecular absorption by atmospheric gases (primarily water vapor and CO2)
 * 
 * Simplified model for near-infrared wavelengths (1550 nm):
 * α_abs ≈ 0.05 + 0.1*h dB/km
 * 
 * where h is relative humidity (0-1)
 * 
 * For visible wavelengths, absorption is generally lower.
 * 
 * @param wavelength Optical wavelength in meters
 * @param distance Link distance in meters
 * @param humidity Relative humidity (0-1)
 * @return Absorption loss in dB
 */
double channel_calculate_atmospheric_absorption(double wavelength, double distance,
                                                double humidity) {
    /* Convert wavelength to nm */
    double lambda_nm = wavelength * 1e9;
    
    /* Absorption coefficient in dB/km */
    double alpha_abs;
    
    if (lambda_nm >= 1400.0 && lambda_nm <= 1600.0) {
        /* Near-infrared (1550 nm telecom band) */
        /* Higher absorption due to water vapor */
        alpha_abs = 0.05 + 0.1 * humidity;
    } else if (lambda_nm >= 700.0 && lambda_nm <= 1000.0) {
        /* Near-infrared (850 nm) */
        alpha_abs = 0.03 + 0.05 * humidity;
    } else {
        /* Visible wavelengths (400-700 nm) */
        /* Lower absorption */
        alpha_abs = 0.02 + 0.03 * humidity;
    }
    
    /* Total absorption loss */
    double distance_km = distance / 1000.0;
    double absorption_db = alpha_abs * distance_km;
    
    return absorption_db;
}

/* ============================================================================
 * Channel Effects Application
 * ============================================================================ */

/**
 * @brief Apply channel effects to input signal power
 * 
 * Combines all channel effects:
 * 1. Path loss (free-space propagation)
 * 2. Atmospheric attenuation (weather-dependent)
 * 3. Atmospheric absorption (molecular)
 * 4. Log-normal fading (turbulence-induced)
 * 5. AWGN (Additive White Gaussian Noise)
 * 
 * Received power calculation:
 * P_rx = P_tx * h_fade * 10^(-(L_path + L_atten + L_abs)/10) + n
 * 
 * where:
 * - P_tx is transmit power
 * - h_fade is fading coefficient (linear scale)
 * - L_path is path loss in dB
 * - L_atten is weather attenuation in dB
 * - L_abs is atmospheric absorption in dB
 * - n is AWGN noise
 * 
 * @param channel Pointer to channel model structure
 * @param input_power Input signal power in watts
 * @param noise_power Noise power in watts (0 for no noise)
 * @param time_step Time step for temporal correlation (seconds)
 * @return Received signal power in watts
 */
double channel_apply_effects(ChannelModel* channel, double input_power,
                             double noise_power, double time_step) {
    if (channel == NULL || !channel->initialized) {
        FSO_LOG_ERROR(MODULE_NAME, "Channel not initialized");
        return 0.0;
    }
    
    if (input_power < 0.0) {
        FSO_LOG_ERROR(MODULE_NAME, "Invalid input power: %.2e W", input_power);
        return 0.0;
    }
    
    /* 1. Generate fading coefficient */
    double fading_coefficient;
    if (time_step > 0.0) {
        /* Use temporally correlated fading */
        fading_coefficient = channel_generate_correlated_fading(channel, time_step);
    } else {
        /* Use uncorrelated fading */
        fading_coefficient = channel_generate_fading(channel);
    }
    
    /* 2. Calculate total loss in dB */
    /* Path loss (already calculated and cached) */
    double total_loss_db = channel->path_loss_db;
    
    /* Weather attenuation (dB/km * distance_km) */
    double distance_km = channel->link_distance / 1000.0;
    total_loss_db += channel->attenuation_db * distance_km;
    
    /* Atmospheric absorption */
    double absorption_db = channel_calculate_atmospheric_absorption(
        channel->wavelength, channel->link_distance, channel->humidity);
    total_loss_db += absorption_db;
    
    /* 3. Convert total loss to linear scale */
    double loss_linear = fso_db_to_linear(total_loss_db);
    
    /* 4. Calculate received signal power */
    /* P_rx = P_tx * h_fade / loss_linear */
    double received_power = input_power * fading_coefficient / loss_linear;
    
    /* 5. Add AWGN if specified */
    if (noise_power > 0.0) {
        /* Generate Gaussian noise with specified power */
        double noise_stddev = sqrt(noise_power);
        double noise_sample = fso_random_gaussian(0.0, noise_stddev);
        
        /* Add noise to received power (can be negative due to noise) */
        received_power += noise_sample;
        
        /* Ensure non-negative power */
        if (received_power < 0.0) {
            received_power = 0.0;
        }
    }
    
    /* Log detailed information at debug level */
    FSO_LOG_DEBUG(MODULE_NAME, 
        "Channel effects: P_in=%.2e W, fade=%.3f, loss=%.2f dB, P_out=%.2e W",
        input_power, fading_coefficient, total_loss_db, received_power);
    
    return received_power;
}
