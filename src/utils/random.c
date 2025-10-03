/**
 * @file random.c
 * @brief Thread-safe random number generation utilities
 * 
 * Implements thread-safe random number generators including uniform
 * and Gaussian distributions using Box-Muller transform.
 */

#include "../fso.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/**
 * @brief Random number generator state
 */
typedef struct {
    unsigned int seed;           /**< Current seed value */
    int has_spare;              /**< Flag for Box-Muller spare value */
    double spare;               /**< Spare Gaussian value from Box-Muller */
} RandomState;

/**
 * @brief Thread-local random state
 */
#ifdef _OPENMP
#pragma omp threadprivate(tls_random_state)
#endif
static RandomState tls_random_state = {0, 0, 0.0};

/**
 * @brief Initialize random number generator with seed
 * @param seed Seed value (use 0 for time-based seed)
 */
void fso_random_init(unsigned int seed) {
    if (seed == 0) {
        seed = (unsigned int)time(NULL);
#ifdef _OPENMP
        seed += omp_get_thread_num();
#endif
    }
    
    tls_random_state.seed = seed;
    tls_random_state.has_spare = 0;
    tls_random_state.spare = 0.0;
}

/**
 * @brief Set seed for current thread
 * @param seed Seed value
 */
void fso_random_set_seed(unsigned int seed) {
    tls_random_state.seed = seed;
    tls_random_state.has_spare = 0;
}

/**
 * @brief Get current seed for current thread
 * @return Current seed value
 */
unsigned int fso_random_get_seed(void) {
    return tls_random_state.seed;
}

/**
 * @brief Generate uniform random number in [0, 1)
 * @return Random number in [0, 1)
 */
double fso_random_uniform(void) {
    // Thread-safe random number generation
    // Use a simple LCG (Linear Congruential Generator) for portability
    tls_random_state.seed = tls_random_state.seed * 1103515245 + 12345;
    unsigned int r = (tls_random_state.seed / 65536) % 32768;
    return (double)r / 32768.0;
}

/**
 * @brief Generate uniform random number in [min, max)
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 * @return Random number in [min, max)
 */
double fso_random_uniform_range(double min, double max) {
    return min + (max - min) * fso_random_uniform();
}

/**
 * @brief Generate Gaussian (normal) random number using Box-Muller transform
 * @param mean Mean of the distribution
 * @param stddev Standard deviation of the distribution
 * @return Random number from N(mean, stddev^2)
 */
double fso_random_gaussian(double mean, double stddev) {
    // Box-Muller transform generates two independent Gaussian values
    // We save one for the next call to improve efficiency
    
    if (tls_random_state.has_spare) {
        tls_random_state.has_spare = 0;
        return mean + stddev * tls_random_state.spare;
    }
    
    // Generate two uniform random numbers
    double u1, u2;
    do {
        u1 = fso_random_uniform();
    } while (u1 == 0.0);  // Avoid log(0)
    
    u2 = fso_random_uniform();
    
    // Box-Muller transform
    double r = sqrt(-2.0 * log(u1));
    double theta = 2.0 * FSO_PI * u2;
    
    // Generate two independent Gaussian values
    double z0 = r * cos(theta);
    double z1 = r * sin(theta);
    
    // Save one for next call
    tls_random_state.spare = z1;
    tls_random_state.has_spare = 1;
    
    return mean + stddev * z0;
}

/**
 * @brief Generate standard normal random number (mean=0, stddev=1)
 * @return Random number from N(0, 1)
 */
double fso_random_normal(void) {
    return fso_random_gaussian(0.0, 1.0);
}

/**
 * @brief Generate random integer in [min, max]
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return Random integer in [min, max]
 */
int fso_random_int(int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    
    int range = max - min + 1;
    tls_random_state.seed = tls_random_state.seed * 1103515245 + 12345;
    unsigned int r = (tls_random_state.seed / 65536) % 32768;
    return min + (r % range);
}
