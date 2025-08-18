#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

/* Log levels */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} log_level_t;

/* Predefined warning flags */
typedef enum {
    LOG_FLAG_NONE           = 0,
    LOG_FLAG_CONSOLE        = 1 << 0,  /* Output to console */
    LOG_FLAG_FILE           = 1 << 1,  /* Output to file */
    LOG_FLAG_TIMESTAMP      = 1 << 2,  /* Include timestamp */
    LOG_FLAG_THREAD_ID      = 1 << 3,  /* Include thread ID */
    LOG_FLAG_COLOR          = 1 << 4,  /* Use color output */
    LOG_FLAG_ALL            = (1 << 5) - 1
} log_flags_t;

/* Logger configuration structure */
typedef struct {
    log_level_t min_level;      /* Minimum level to log */
    log_flags_t flags;          /* Configuration flags */
    FILE *file_handle;          /* File handle for file output */
    char *log_filename;         /* Log file name */
    pthread_mutex_t mutex;      /* Thread safety mutex */
    int initialized;            /* Initialization flag */
} logger_config_t;

/* Color codes for console output */
#define COLOR_RESET   "\033[0m"
#define COLOR_DEBUG   "\033[36m"    /* Cyan */
#define COLOR_INFO    "\033[32m"    /* Green */
#define COLOR_WARN    "\033[33m"    /* Yellow */
#define COLOR_ERROR   "\033[31m"    /* Red */
#define COLOR_FATAL   "\033[35m"    /* Magenta */

/* API Functions */

/**
 * Initialize the logger with specified configuration
 * @param min_level Minimum log level to display
 * @param flags Configuration flags
 * @param filename Log file name (NULL for no file logging)
 * @return 0 on success, -1 on error
 */
int logger_init(log_level_t min_level, log_flags_t flags, const char *filename);

/**
 * Cleanup and destroy the logger
 */
void logger_destroy(void);

/**
 * Set the minimum log level
 * @param level New minimum log level
 */
void logger_set_level(log_level_t level);

/**
 * Get the current minimum log level
 * @return Current minimum log level
 */
log_level_t logger_get_level(void);

/**
 * Set logger flags
 * @param flags New configuration flags
 */
void logger_set_flags(log_flags_t flags);

/**
 * Get current logger flags
 * @return Current configuration flags
 */
log_flags_t logger_get_flags(void);

/**
 * Core logging function
 * @param level Log level
 * @param file Source file name
 * @param line Source line number
 * @param func Function name
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void logger_log(log_level_t level, const char *file, int line, const char *func, const char *format, ...);

/* Convenience macros for logging */
#define LOG_DEBUG_MSG(format, ...) logger_log(LOG_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_INFO_MSG(format, ...)  logger_log(LOG_INFO,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_WARN_MSG(format, ...)  logger_log(LOG_WARN,  __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_ERROR_MSG(format, ...) logger_log(LOG_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)
#define LOG_FATAL_MSG(format, ...) logger_log(LOG_FATAL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/* Simplified macros without file/line info */
#define LOG_DEBUG_SIMPLE(format, ...) logger_log_simple(LOG_DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO_SIMPLE(format, ...)  logger_log_simple(LOG_INFO,  format, ##__VA_ARGS__)
#define LOG_WARN_SIMPLE(format, ...)  logger_log_simple(LOG_WARN,  format, ##__VA_ARGS__)
#define LOG_ERROR_SIMPLE(format, ...) logger_log_simple(LOG_ERROR, format, ##__VA_ARGS__)
#define LOG_FATAL_SIMPLE(format, ...) logger_log_simple(LOG_FATAL, format, ##__VA_ARGS__)

/**
 * Simplified logging function without file/line information
 * @param level Log level
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void logger_log_simple(log_level_t level, const char *format, ...);

/**
 * Get string representation of log level
 * @param level Log level
 * @return String representation
 */
const char* logger_level_string(log_level_t level);

/**
 * Get color code for log level
 * @param level Log level
 * @return ANSI color code string
 */
const char* logger_level_color(log_level_t level);

#endif /* LOGGER_H */
