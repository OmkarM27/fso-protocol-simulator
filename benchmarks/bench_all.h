/**
 * @file bench_all.h
 * @brief Header file for all benchmark functions
 */

#ifndef BENCH_ALL_H
#define BENCH_ALL_H

#include "benchmark.h"

/* FFT Benchmarks */
int benchmark_fft_comprehensive(void);
int benchmark_fft_quick(size_t fft_size);

/* Filter Benchmarks */
int benchmark_filters_comprehensive(void);
int benchmark_moving_average(void);
int benchmark_adaptive_filter(void);
int benchmark_convolution(void);

/* Modulation and FEC Benchmarks */
int benchmark_modulation_comprehensive(void);
int benchmark_fec_comprehensive(void);

/* End-to-End Benchmarks */
int benchmark_e2e_comprehensive(void);
int benchmark_e2e_quick(void);

#endif /* BENCH_ALL_H */
