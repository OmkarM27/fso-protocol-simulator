/**
 * @file main.c
 * @brief Main benchmark program
 * 
 * Runs all benchmarks and generates comprehensive reports with
 * performance summaries, speedup graphs, and scaling efficiency plots.
 */

#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External benchmark functions */
extern int benchmark_fft_comprehensive(void);
extern int benchmark_fft_quick(size_t fft_size);
extern int benchmark_filters_comprehensive(void);
extern int benchmark_moving_average(void);
extern int benchmark_adaptive_filter(void);
extern int benchmark_convolution(void);
extern int benchmark_modulation_comprehensive(void);
extern int benchmark_fec_comprehensive(void);
extern int benchmark_e2e_comprehensive(void);
extern int benchmark_e2e_quick(void);

/* ============================================================================
 * Visualization and Reporting
 * ============================================================================ */

/**
 * @brief Generate gnuplot script for speedup visualization
 */
static int generate_speedup_plot(const char* data_file, const char* output_file) {
    FILE* fp = fopen("speedup_plot.gnu", "w");
    if (!fp) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to create gnuplot script");
        return FSO_ERROR_IO;
    }
    
    fprintf(fp, "#!/usr/bin/gnuplot\n");
    fprintf(fp, "set terminal png size 1200,800\n");
    fprintf(fp, "set output '%s'\n", output_file);
    fprintf(fp, "set title 'Parallel Speedup vs Thread Count'\n");
    fprintf(fp, "set xlabel 'Number of Threads'\n");
    fprintf(fp, "set ylabel 'Speedup Factor'\n");
    fprintf(fp, "set grid\n");
    fprintf(fp, "set key left top\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Ideal speedup line\n");
    fprintf(fp, "ideal(x) = x\n");
    fprintf(fp, "\n");
    fprintf(fp, "plot '%s' using 2:11 with linespoints title 'Actual Speedup', \\\n", 
            data_file);
    fprintf(fp, "     ideal(x) with lines dashtype 2 title 'Ideal Speedup'\n");
    
    fclose(fp);
    
    FSO_LOG_INFO("BENCHMARK", "Generated gnuplot script: speedup_plot.gnu");
    return FSO_SUCCESS;
}

/**
 * @brief Generate gnuplot script for efficiency visualization
 */
static int generate_efficiency_plot(const char* data_file, const char* output_file) {
    FILE* fp = fopen("efficiency_plot.gnu", "w");
    if (!fp) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to create gnuplot script");
        return FSO_ERROR_IO;
    }
    
    fprintf(fp, "#!/usr/bin/gnuplot\n");
    fprintf(fp, "set terminal png size 1200,800\n");
    fprintf(fp, "set output '%s'\n", output_file);
    fprintf(fp, "set title 'Parallel Efficiency vs Thread Count'\n");
    fprintf(fp, "set xlabel 'Number of Threads'\n");
    fprintf(fp, "set ylabel 'Efficiency (%%)'  \n");
    fprintf(fp, "set grid\n");
    fprintf(fp, "set key right top\n");
    fprintf(fp, "set yrange [0:110]\n");
    fprintf(fp, "\n");
    fprintf(fp, "plot '%s' using 2:($12*100) with linespoints title 'Parallel Efficiency'\n",
            data_file);
    
    fclose(fp);
    
    FSO_LOG_INFO("BENCHMARK", "Generated gnuplot script: efficiency_plot.gnu");
    return FSO_SUCCESS;
}

/**
 * @brief Generate gnuplot script for throughput visualization
 */
static int generate_throughput_plot(const char* data_file, const char* output_file) {
    FILE* fp = fopen("throughput_plot.gnu", "w");
    if (!fp) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to create gnuplot script");
        return FSO_ERROR_IO;
    }
    
    fprintf(fp, "#!/usr/bin/gnuplot\n");
    fprintf(fp, "set terminal png size 1200,800\n");
    fprintf(fp, "set output '%s'\n", output_file);
    fprintf(fp, "set title 'Throughput vs Thread Count'\n");
    fprintf(fp, "set xlabel 'Number of Threads'\n");
    fprintf(fp, "set ylabel 'Throughput (Mbps)'\n");
    fprintf(fp, "set grid\n");
    fprintf(fp, "set key left top\n");
    fprintf(fp, "\n");
    fprintf(fp, "plot '%s' using 2:7 with linespoints title 'Throughput'\n",
            data_file);
    
    fclose(fp);
    
    FSO_LOG_INFO("BENCHMARK", "Generated gnuplot script: throughput_plot.gnu");
    return FSO_SUCCESS;
}

/**
 * @brief Generate comprehensive HTML report
 */
static int generate_html_report(void) {
    FILE* fp = fopen("benchmark_report.html", "w");
    if (!fp) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to create HTML report");
        return FSO_ERROR_IO;
    }
    
    fprintf(fp, "<!DOCTYPE html>\n");
    fprintf(fp, "<html>\n");
    fprintf(fp, "<head>\n");
    fprintf(fp, "  <title>FSO Benchmark Report</title>\n");
    fprintf(fp, "  <style>\n");
    fprintf(fp, "    body { font-family: Arial, sans-serif; margin: 20px; }\n");
    fprintf(fp, "    h1 { color: #333; }\n");
    fprintf(fp, "    h2 { color: #666; border-bottom: 2px solid #ddd; padding-bottom: 5px; }\n");
    fprintf(fp, "    table { border-collapse: collapse; width: 100%%; margin: 20px 0; }\n");
    fprintf(fp, "    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n");
    fprintf(fp, "    th { background-color: #4CAF50; color: white; }\n");
    fprintf(fp, "    tr:nth-child(even) { background-color: #f2f2f2; }\n");
    fprintf(fp, "    .metric { font-weight: bold; color: #4CAF50; }\n");
    fprintf(fp, "    .chart { margin: 20px 0; text-align: center; }\n");
    fprintf(fp, "    .chart img { max-width: 100%%; height: auto; }\n");
    fprintf(fp, "  </style>\n");
    fprintf(fp, "</head>\n");
    fprintf(fp, "<body>\n");
    fprintf(fp, "  <h1>FSO Communication Suite - Benchmark Report</h1>\n");
    fprintf(fp, "  <p>Generated: <span id='timestamp'></span></p>\n");
    fprintf(fp, "  <script>document.getElementById('timestamp').innerHTML = new Date().toLocaleString();</script>\n");
    fprintf(fp, "\n");
    fprintf(fp, "  <h2>System Information</h2>\n");
    fprintf(fp, "  <ul>\n");
    fprintf(fp, "    <li>CPU Cores: <span class='metric'>%d</span></li>\n", 
            benchmark_get_num_cores());
    fprintf(fp, "    <li>OpenMP Available: <span class='metric'>%s</span></li>\n",
#ifdef _OPENMP
            "Yes"
#else
            "No"
#endif
    );
    fprintf(fp, "  </ul>\n");
    fprintf(fp, "\n");
    fprintf(fp, "  <h2>Performance Visualizations</h2>\n");
    fprintf(fp, "  <div class='chart'>\n");
    fprintf(fp, "    <h3>Speedup Analysis</h3>\n");
    fprintf(fp, "    <img src='speedup_plot.png' alt='Speedup Plot'>\n");
    fprintf(fp, "  </div>\n");
    fprintf(fp, "  <div class='chart'>\n");
    fprintf(fp, "    <h3>Parallel Efficiency</h3>\n");
    fprintf(fp, "    <img src='efficiency_plot.png' alt='Efficiency Plot'>\n");
    fprintf(fp, "  </div>\n");
    fprintf(fp, "  <div class='chart'>\n");
    fprintf(fp, "    <h3>Throughput Scaling</h3>\n");
    fprintf(fp, "    <img src='throughput_plot.png' alt='Throughput Plot'>\n");
    fprintf(fp, "  </div>\n");
    fprintf(fp, "\n");
    fprintf(fp, "  <h2>Detailed Results</h2>\n");
    fprintf(fp, "  <p>See CSV files for detailed numerical results:</p>\n");
    fprintf(fp, "  <ul>\n");
    fprintf(fp, "    <li><a href='fft_benchmark_results.csv'>FFT Benchmarks</a></li>\n");
    fprintf(fp, "    <li><a href='filter_benchmark_results.csv'>Filter Benchmarks</a></li>\n");
    fprintf(fp, "    <li><a href='modulation_benchmark_results.csv'>Modulation Benchmarks</a></li>\n");
    fprintf(fp, "    <li><a href='fec_benchmark_results.csv'>FEC Benchmarks</a></li>\n");
    fprintf(fp, "    <li><a href='e2e_benchmark_results.csv'>End-to-End Benchmarks</a></li>\n");
    fprintf(fp, "  </ul>\n");
    fprintf(fp, "\n");
    fprintf(fp, "  <h2>Summary Statistics</h2>\n");
    fprintf(fp, "  <p>Key performance metrics from all benchmarks.</p>\n");
    fprintf(fp, "  <p><em>Note: Detailed statistics are available in the CSV and JSON files.</em></p>\n");
    fprintf(fp, "\n");
    fprintf(fp, "</body>\n");
    fprintf(fp, "</html>\n");
    
    fclose(fp);
    
    FSO_LOG_INFO("BENCHMARK", "Generated HTML report: benchmark_report.html");
    return FSO_SUCCESS;
}

/**
 * @brief Generate summary report
 */
static int generate_summary_report(void) {
    FILE* fp = fopen("benchmark_summary.txt", "w");
    if (!fp) {
        FSO_LOG_ERROR("BENCHMARK", "Failed to create summary report");
        return FSO_ERROR_IO;
    }
    
    fprintf(fp, "================================================================================\n");
    fprintf(fp, "  FSO Communication Suite - Benchmark Summary\n");
    fprintf(fp, "================================================================================\n");
    fprintf(fp, "\n");
    fprintf(fp, "System Information:\n");
    fprintf(fp, "  CPU Cores: %d\n", benchmark_get_num_cores());
    fprintf(fp, "  OpenMP: %s\n",
#ifdef _OPENMP
            "Available"
#else
            "Not Available"
#endif
    );
    fprintf(fp, "\n");
    fprintf(fp, "Benchmark Categories:\n");
    fprintf(fp, "  1. FFT Performance\n");
    fprintf(fp, "  2. Filter Performance\n");
    fprintf(fp, "  3. Modulation Throughput\n");
    fprintf(fp, "  4. FEC Throughput\n");
    fprintf(fp, "  5. End-to-End Latency\n");
    fprintf(fp, "\n");
    fprintf(fp, "Output Files:\n");
    fprintf(fp, "  - CSV Results: *_benchmark_results.csv\n");
    fprintf(fp, "  - JSON Results: *_benchmark_results.json\n");
    fprintf(fp, "  - Visualization Scripts: *.gnu\n");
    fprintf(fp, "  - HTML Report: benchmark_report.html\n");
    fprintf(fp, "\n");
    fprintf(fp, "To generate plots, run:\n");
    fprintf(fp, "  gnuplot speedup_plot.gnu\n");
    fprintf(fp, "  gnuplot efficiency_plot.gnu\n");
    fprintf(fp, "  gnuplot throughput_plot.gnu\n");
    fprintf(fp, "\n");
    fprintf(fp, "================================================================================\n");
    
    fclose(fp);
    
    FSO_LOG_INFO("BENCHMARK", "Generated summary report: benchmark_summary.txt");
    return FSO_SUCCESS;
}

/* ============================================================================
 * Main Benchmark Program
 * ============================================================================ */

static void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -a, --all               Run all benchmarks (default)\n");
    printf("  -f, --fft               Run FFT benchmarks only\n");
    printf("  -F, --filters           Run filter benchmarks only\n");
    printf("  -m, --modulation        Run modulation benchmarks only\n");
    printf("  -e, --fec               Run FEC benchmarks only\n");
    printf("  -E, --e2e               Run end-to-end benchmarks only\n");
    printf("  -q, --quick             Run quick benchmarks (faster)\n");
    printf("  -r, --report            Generate reports and visualizations\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --all                # Run all benchmarks\n", program_name);
    printf("  %s --fft --quick        # Quick FFT benchmark\n", program_name);
    printf("  %s --e2e                # End-to-end latency only\n", program_name);
    printf("\n");
}

int main(int argc, char* argv[]) {
    // Set log level
    fso_set_log_level(LOG_INFO);
    
    // Initialize random number generator
    fso_random_init(0);
    
    // Parse command line arguments
    int run_all = 1;
    int run_fft = 0;
    int run_filters = 0;
    int run_modulation = 0;
    int run_fec = 0;
    int run_e2e = 0;
    int quick_mode = 0;
    int generate_reports = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            run_all = 1;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fft") == 0) {
            run_fft = 1;
            run_all = 0;
        } else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--filters") == 0) {
            run_filters = 1;
            run_all = 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--modulation") == 0) {
            run_modulation = 1;
            run_all = 0;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--fec") == 0) {
            run_fec = 1;
            run_all = 0;
        } else if (strcmp(argv[i], "-E") == 0 || strcmp(argv[i], "--e2e") == 0) {
            run_e2e = 1;
            run_all = 0;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quick") == 0) {
            quick_mode = 1;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--report") == 0) {
            generate_reports = 1;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Print banner
    printf("\n");
    printf("================================================================================\n");
    printf("  FSO Communication Suite - Performance Benchmarks\n");
    printf("================================================================================\n");
    printf("\n");
    printf("System: %d CPU cores, OpenMP %s\n",
           benchmark_get_num_cores(),
#ifdef _OPENMP
           "enabled"
#else
           "disabled"
#endif
    );
    printf("Mode: %s\n", quick_mode ? "Quick" : "Comprehensive");
    printf("\n");
    
    int result = FSO_SUCCESS;
    
    // Run benchmarks
    if (run_all || run_fft) {
        if (quick_mode) {
            result = benchmark_fft_quick(16384);
        } else {
            result = benchmark_fft_comprehensive();
        }
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("MAIN", "FFT benchmark failed");
        }
    }
    
    if (run_all || run_filters) {
        result = benchmark_filters_comprehensive();
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("MAIN", "Filter benchmark failed");
        }
    }
    
    if (run_all || run_modulation) {
        result = benchmark_modulation_comprehensive();
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("MAIN", "Modulation benchmark failed");
        }
    }
    
    if (run_all || run_fec) {
        result = benchmark_fec_comprehensive();
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("MAIN", "FEC benchmark failed");
        }
    }
    
    if (run_all || run_e2e) {
        if (quick_mode) {
            result = benchmark_e2e_quick();
        } else {
            result = benchmark_e2e_comprehensive();
        }
        if (result != FSO_SUCCESS) {
            FSO_LOG_ERROR("MAIN", "End-to-end benchmark failed");
        }
    }
    
    // Generate reports and visualizations
    if (generate_reports || run_all) {
        printf("\n");
        printf("================================================================================\n");
        printf("  Generating Reports and Visualizations\n");
        printf("================================================================================\n");
        printf("\n");
        
        generate_speedup_plot("fft_benchmark_results.csv", "speedup_plot.png");
        generate_efficiency_plot("fft_benchmark_results.csv", "efficiency_plot.png");
        generate_throughput_plot("fft_benchmark_results.csv", "throughput_plot.png");
        generate_html_report();
        generate_summary_report();
        
        printf("\nReports generated successfully!\n");
        printf("  - benchmark_summary.txt\n");
        printf("  - benchmark_report.html\n");
        printf("  - *.gnu (gnuplot scripts)\n");
        printf("\n");
        printf("To generate plots, run: gnuplot *.gnu\n");
    }
    
    printf("\n");
    printf("================================================================================\n");
    printf("  Benchmarks Complete!\n");
    printf("================================================================================\n");
    printf("\n");
    
    return result == FSO_SUCCESS ? 0 : 1;
}
