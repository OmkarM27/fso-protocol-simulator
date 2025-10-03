/**
 * @file sim_results.c
 * @brief Simulation results management implementation
 */

#include "simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Results Initialization and Cleanup
 * ============================================================================ */

int sim_results_init(SimResults* results, size_t history_capacity, int num_packets) {
    if (results == NULL) {
        FSO_LOG_ERROR("SimResults", "NULL results pointer");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (history_capacity == 0 || num_packets <= 0) {
        FSO_LOG_ERROR("SimResults", "Invalid capacity or num_packets");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Initialize all fields to zero
    memset(results, 0, sizeof(SimResults));
    
    // Allocate time-series history
    results->history = (TimeSeriesPoint*)calloc(history_capacity, sizeof(TimeSeriesPoint));
    if (results->history == NULL) {
        FSO_LOG_ERROR("SimResults", "Failed to allocate history array");
        return FSO_ERROR_MEMORY;
    }
    results->history_capacity = history_capacity;
    results->history_length = 0;
    
    // Allocate packet statistics
    results->packet_stats = (PacketStats*)calloc(num_packets, sizeof(PacketStats));
    if (results->packet_stats == NULL) {
        FSO_LOG_ERROR("SimResults", "Failed to allocate packet stats array");
        free(results->history);
        results->history = NULL;
        return FSO_ERROR_MEMORY;
    }
    results->num_packet_stats = 0;
    
    // Initialize min/max values
    results->min_snr = INFINITY;
    results->max_snr = -INFINITY;
    results->min_ber = INFINITY;
    results->max_ber = -INFINITY;
    
    FSO_LOG_INFO("SimResults", "Initialized with capacity %zu history points, %d packets",
                 history_capacity, num_packets);
    return FSO_SUCCESS;
}

void sim_results_free(SimResults* results) {
    if (results == NULL) {
        return;
    }
    
    if (results->history != NULL) {
        free(results->history);
        results->history = NULL;
    }
    
    if (results->packet_stats != NULL) {
        free(results->packet_stats);
        results->packet_stats = NULL;
    }
    
    results->history_capacity = 0;
    results->history_length = 0;
    results->num_packet_stats = 0;
    
    FSO_LOG_DEBUG("SimResults", "Freed results resources");
}

/* ============================================================================
 * Data Collection
 * ============================================================================ */

int sim_results_add_point(SimResults* results, const TimeSeriesPoint* point) {
    if (results == NULL || point == NULL) {
        FSO_LOG_ERROR("SimResults", "NULL pointer in add_point");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    // Check if we need to reallocate
    if (results->history_length >= results->history_capacity) {
        size_t new_capacity = results->history_capacity * 2;
        TimeSeriesPoint* new_history = (TimeSeriesPoint*)realloc(
            results->history, new_capacity * sizeof(TimeSeriesPoint));
        
        if (new_history == NULL) {
            FSO_LOG_ERROR("SimResults", "Failed to reallocate history array");
            return FSO_ERROR_MEMORY;
        }
        
        results->history = new_history;
        results->history_capacity = new_capacity;
        FSO_LOG_DEBUG("SimResults", "Reallocated history to capacity %zu", new_capacity);
    }
    
    // Add the point
    results->history[results->history_length] = *point;
    results->history_length++;
    
    return FSO_SUCCESS;
}

int sim_results_add_packet(SimResults* results, const PacketStats* stats) {
    if (results == NULL || stats == NULL) {
        FSO_LOG_ERROR("SimResults", "NULL pointer in add_packet");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (results->packet_stats == NULL) {
        FSO_LOG_ERROR("SimResults", "Packet stats array not initialized");
        return FSO_ERROR_NOT_INITIALIZED;
    }
    
    // Add packet stats
    size_t index = results->num_packet_stats;
    results->packet_stats[index] = *stats;
    results->num_packet_stats++;
    
    // Update counters
    results->total_packets++;
    results->total_bits += stats->bits_transmitted;
    results->total_bit_errors += stats->bit_errors;
    
    if (stats->fec_uncorrectable) {
        results->packets_lost++;
    } else {
        results->packets_received++;
    }
    
    results->fec_corrected_errors += stats->fec_corrected_errors;
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Metrics Calculation
 * ============================================================================ */

int sim_results_calculate_metrics(SimResults* results) {
    if (results == NULL) {
        FSO_LOG_ERROR("SimResults", "NULL results pointer");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    if (results->total_packets == 0) {
        FSO_LOG_WARNING("SimResults", "No packets to calculate metrics from");
        return FSO_SUCCESS;
    }
    
    // Calculate packet loss rate
    results->packet_loss_rate = (double)results->packets_lost / (double)results->total_packets;
    
    // Calculate average BER
    if (results->total_bits > 0) {
        results->avg_ber = (double)results->total_bit_errors / (double)results->total_bits;
    }
    
    // Calculate averages from time-series data
    if (results->history_length > 0) {
        double sum_snr = 0.0;
        double sum_throughput = 0.0;
        double sum_beam_az = 0.0;
        double sum_beam_el = 0.0;
        
        for (size_t i = 0; i < results->history_length; i++) {
            TimeSeriesPoint* point = &results->history[i];
            
            sum_snr += point->snr_db;
            sum_throughput += point->throughput;
            
            if (results->tracking_enabled) {
                sum_beam_az += point->beam_azimuth;
                sum_beam_el += point->beam_elevation;
            }
            
            // Update min/max
            if (point->snr_db < results->min_snr) {
                results->min_snr = point->snr_db;
            }
            if (point->snr_db > results->max_snr) {
                results->max_snr = point->snr_db;
            }
            
            if (point->ber < results->min_ber) {
                results->min_ber = point->ber;
            }
            if (point->ber > results->max_ber) {
                results->max_ber = point->ber;
            }
        }
        
        results->avg_snr = sum_snr / results->history_length;
        results->avg_throughput = sum_throughput / results->history_length;
        
        if (results->tracking_enabled) {
            results->avg_beam_azimuth = sum_beam_az / results->history_length;
            results->avg_beam_elevation = sum_beam_el / results->history_length;
        }
    }
    
    // Calculate simulation duration
    if (results->history_length > 0) {
        results->simulation_duration = 
            results->history[results->history_length - 1].timestamp - 
            results->history[0].timestamp;
    }
    
    FSO_LOG_INFO("SimResults", "Calculated metrics: BER=%.3e, SNR=%.2f dB, PLR=%.3f",
                 results->avg_ber, results->avg_snr, results->packet_loss_rate);
    
    return FSO_SUCCESS;
}

/* ============================================================================
 * Results Output
 * ============================================================================ */

void sim_results_print(const SimResults* results) {
    if (results == NULL) {
        printf("NULL results\n");
        return;
    }
    
    printf("\n");
    printf("=== Simulation Results ===\n");
    printf("\n");
    
    printf("Overall Statistics:\n");
    printf("  Total Packets:        %d\n", results->total_packets);
    printf("  Packets Received:     %d\n", results->packets_received);
    printf("  Packets Lost:         %d\n", results->packets_lost);
    printf("  Packet Loss Rate:     %.3f%% (%.3e)\n", 
           results->packet_loss_rate * 100.0, results->packet_loss_rate);
    printf("\n");
    
    printf("Bit Error Statistics:\n");
    printf("  Total Bits:           %lld\n", results->total_bits);
    printf("  Total Bit Errors:     %lld\n", results->total_bit_errors);
    printf("  Average BER:          %.3e\n", results->avg_ber);
    printf("  Min BER:              %.3e\n", results->min_ber);
    printf("  Max BER:              %.3e\n", results->max_ber);
    printf("\n");
    
    printf("FEC Statistics:\n");
    printf("  Errors Corrected:     %lld\n", results->fec_corrected_errors);
    printf("  Correction Rate:      %.1f%%\n",
           results->total_bit_errors > 0 ? 
           (100.0 * results->fec_corrected_errors / results->total_bit_errors) : 0.0);
    printf("\n");
    
    printf("Signal Quality:\n");
    printf("  Average SNR:          %.2f dB\n", results->avg_snr);
    printf("  Min SNR:              %.2f dB\n", results->min_snr);
    printf("  Max SNR:              %.2f dB\n", results->max_snr);
    printf("  Average Throughput:   %.3e bits/s\n", results->avg_throughput);
    printf("\n");
    
    if (results->tracking_enabled) {
        printf("Beam Tracking:\n");
        printf("  Average Azimuth:      %.3f rad (%.2f deg)\n",
               results->avg_beam_azimuth, results->avg_beam_azimuth * 180.0 / FSO_PI);
        printf("  Average Elevation:    %.3f rad (%.2f deg)\n",
               results->avg_beam_elevation, results->avg_beam_elevation * 180.0 / FSO_PI);
        printf("  Tracking Updates:     %d\n", results->tracking_updates);
        printf("  Reacquisitions:       %d\n", results->reacquisitions);
        printf("\n");
    }
    
    printf("Timing:\n");
    printf("  Simulation Duration:  %.3f s\n", results->simulation_duration);
    printf("  History Points:       %zu\n", results->history_length);
    printf("\n");
}

/* ============================================================================
 * CSV Export
 * ============================================================================ */

int sim_results_export_csv(const SimResults* results, const char* filename) {
    if (results == NULL || filename == NULL) {
        FSO_LOG_ERROR("SimResults", "NULL pointer in export_csv");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        FSO_LOG_ERROR("SimResults", "Failed to open file: %s", filename);
        return FSO_ERROR_IO;
    }
    
    // Write header
    fprintf(fp, "timestamp,ber,snr_db,received_power,throughput");
    if (results->tracking_enabled) {
        fprintf(fp, ",beam_azimuth,beam_elevation,signal_strength");
    }
    fprintf(fp, "\n");
    
    // Write data points
    for (size_t i = 0; i < results->history_length; i++) {
        const TimeSeriesPoint* point = &results->history[i];
        
        fprintf(fp, "%.6f,%.6e,%.3f,%.6e,%.3e",
                point->timestamp,
                point->ber,
                point->snr_db,
                point->received_power,
                point->throughput);
        
        if (results->tracking_enabled) {
            fprintf(fp, ",%.6f,%.6f,%.6f",
                    point->beam_azimuth,
                    point->beam_elevation,
                    point->signal_strength);
        }
        
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    FSO_LOG_INFO("SimResults", "Exported %zu time-series points to %s",
                 results->history_length, filename);
    
    return FSO_SUCCESS;
}

int sim_results_export_packets_csv(const SimResults* results, const char* filename) {
    if (results == NULL || filename == NULL) {
        FSO_LOG_ERROR("SimResults", "NULL pointer in export_packets_csv");
        return FSO_ERROR_INVALID_PARAM;
    }
    
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        FSO_LOG_ERROR("SimResults", "Failed to open file: %s", filename);
        return FSO_ERROR_IO;
    }
    
    // Write header
    fprintf(fp, "packet_id,bits_transmitted,bits_received,bit_errors,ber,snr_db,");
    fprintf(fp, "received_power,fec_corrected_errors,fec_uncorrectable\n");
    
    // Write packet data
    for (size_t i = 0; i < results->num_packet_stats; i++) {
        const PacketStats* stats = &results->packet_stats[i];
        
        fprintf(fp, "%d,%d,%d,%d,%.6e,%.3f,%.6e,%d,%d\n",
                stats->packet_id,
                stats->bits_transmitted,
                stats->bits_received,
                stats->bit_errors,
                stats->ber,
                stats->snr_db,
                stats->received_power,
                stats->fec_corrected_errors,
                stats->fec_uncorrectable);
    }
    
    fclose(fp);
    FSO_LOG_INFO("SimResults", "Exported %zu packet statistics to %s",
                 results->num_packet_stats, filename);
    
    return FSO_SUCCESS;
}
