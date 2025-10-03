/**
 * @file logging.c
 * @brief Implementation of logging system for FSO Communication Suite
 */

#include "../fso.h"

/**
 * @brief Global log level - can be set by user to filter messages
 * Default is LOG_INFO to show informational messages and above
 */
LogLevel g_log_level = LOG_INFO;

/**
 * @brief Set the global log level
 * @param level New log level to set
 */
void fso_set_log_level(LogLevel level) {
    g_log_level = level;
}

/**
 * @brief Get the current global log level
 * @return Current log level
 */
LogLevel fso_get_log_level(void) {
    return g_log_level;
}

/**
 * @brief Get string representation of error code
 * @param error_code Error code to convert
 * @return String representation of error code
 */
const char* fso_error_string(FSOErrorCode error_code) {
    switch (error_code) {
        case FSO_SUCCESS:
            return "Success";
        case FSO_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case FSO_ERROR_MEMORY:
            return "Memory allocation failed";
        case FSO_ERROR_NOT_INITIALIZED:
            return "Component not initialized";
        case FSO_ERROR_CONVERGENCE:
            return "Algorithm failed to converge";
        case FSO_ERROR_UNSUPPORTED:
            return "Unsupported operation";
        case FSO_ERROR_IO:
            return "Input/output error";
        default:
            return "Unknown error";
    }
}
