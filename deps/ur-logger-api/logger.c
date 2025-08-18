#include "logger.h"

/* Global logger configuration */
static logger_config_t g_logger = {
    .min_level = LOG_INFO,
    .flags = LOG_FLAG_CONSOLE | LOG_FLAG_TIMESTAMP,
    .file_handle = NULL,
    .log_filename = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = 0
};

/* Level strings */
static const char* level_strings[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

/* Level colors */
static const char* level_colors[] = {
    COLOR_DEBUG, COLOR_INFO, COLOR_WARN, COLOR_ERROR, COLOR_FATAL
};

int logger_init(log_level_t min_level, log_flags_t flags, const char *filename) {
    pthread_mutex_lock(&g_logger.mutex);
    
    /* Clean up previous initialization */
    if (g_logger.initialized) {
        if (g_logger.file_handle && g_logger.file_handle != stdout && g_logger.file_handle != stderr) {
            fclose(g_logger.file_handle);
            g_logger.file_handle = NULL;
        }
        if (g_logger.log_filename) {
            free(g_logger.log_filename);
            g_logger.log_filename = NULL;
        }
    }
    
    /* Set configuration */
    g_logger.min_level = min_level;
    g_logger.flags = flags;
    
    /* Initialize file logging if requested */
    if ((flags & LOG_FLAG_FILE) && filename) {
        g_logger.file_handle = fopen(filename, "a");
        if (!g_logger.file_handle) {
            pthread_mutex_unlock(&g_logger.mutex);
            return -1;
        }
        
        /* Store filename */
        g_logger.log_filename = malloc(strlen(filename) + 1);
        if (g_logger.log_filename) {
            strcpy(g_logger.log_filename, filename);
        }
    }
    
    g_logger.initialized = 1;
    pthread_mutex_unlock(&g_logger.mutex);
    
    return 0;
}

void logger_destroy(void) {
    pthread_mutex_lock(&g_logger.mutex);
    
    if (g_logger.initialized) {
        if (g_logger.file_handle && g_logger.file_handle != stdout && g_logger.file_handle != stderr) {
            fclose(g_logger.file_handle);
            g_logger.file_handle = NULL;
        }
        
        if (g_logger.log_filename) {
            free(g_logger.log_filename);
            g_logger.log_filename = NULL;
        }
        
        g_logger.initialized = 0;
    }
    
    pthread_mutex_unlock(&g_logger.mutex);
}

void logger_set_level(log_level_t level) {
    pthread_mutex_lock(&g_logger.mutex);
    g_logger.min_level = level;
    pthread_mutex_unlock(&g_logger.mutex);
}

log_level_t logger_get_level(void) {
    pthread_mutex_lock(&g_logger.mutex);
    log_level_t level = g_logger.min_level;
    pthread_mutex_unlock(&g_logger.mutex);
    return level;
}

void logger_set_flags(log_flags_t flags) {
    pthread_mutex_lock(&g_logger.mutex);
    g_logger.flags = flags;
    pthread_mutex_unlock(&g_logger.mutex);
}

log_flags_t logger_get_flags(void) {
    pthread_mutex_lock(&g_logger.mutex);
    log_flags_t flags = g_logger.flags;
    pthread_mutex_unlock(&g_logger.mutex);
    return flags;
}

const char* logger_level_string(log_level_t level) {
    if (level >= LOG_DEBUG && level <= LOG_FATAL) {
        return level_strings[level];
    }
    return "UNKNOWN";
}

const char* logger_level_color(log_level_t level) {
    if (level >= LOG_DEBUG && level <= LOG_FATAL) {
        return level_colors[level];
    }
    return COLOR_RESET;
}

static void format_timestamp(char *buffer, size_t buffer_size) {
    time_t raw_time;
    struct tm *time_info;
    
    time(&raw_time);
    time_info = localtime(&raw_time);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info);
}

static void logger_output(log_level_t level, const char *message) {
    char timestamp[32] = {0};
    char thread_id_str[32] = {0};
    
    /* Format timestamp if requested */
    if (g_logger.flags & LOG_FLAG_TIMESTAMP) {
        format_timestamp(timestamp, sizeof(timestamp));
    }
    
    /* Format thread ID if requested */
    if (g_logger.flags & LOG_FLAG_THREAD_ID) {
        snprintf(thread_id_str, sizeof(thread_id_str), "[%lu] ", 
                (unsigned long)pthread_self());
    }
    
    /* Output to console */
    if (g_logger.flags & LOG_FLAG_CONSOLE) {
        FILE *output = (level >= LOG_ERROR) ? stderr : stdout;
        
        if (g_logger.flags & LOG_FLAG_COLOR) {
            fprintf(output, "%s", logger_level_color(level));
        }
        
        if (g_logger.flags & LOG_FLAG_TIMESTAMP) {
            fprintf(output, "[%s] ", timestamp);
        }
        
        if (g_logger.flags & LOG_FLAG_THREAD_ID) {
            fprintf(output, "%s", thread_id_str);
        }
        
        fprintf(output, "[%5s] %s", logger_level_string(level), message);
        
        if (g_logger.flags & LOG_FLAG_COLOR) {
            fprintf(output, "%s", COLOR_RESET);
        }
        
        fprintf(output, "\n");
        fflush(output);
    }
    
    /* Output to file */
    if ((g_logger.flags & LOG_FLAG_FILE) && g_logger.file_handle) {
        if (g_logger.flags & LOG_FLAG_TIMESTAMP) {
            fprintf(g_logger.file_handle, "[%s] ", timestamp);
        }
        
        if (g_logger.flags & LOG_FLAG_THREAD_ID) {
            fprintf(g_logger.file_handle, "%s", thread_id_str);
        }
        
        fprintf(g_logger.file_handle, "[%5s] %s\n", 
                logger_level_string(level), message);
        fflush(g_logger.file_handle);
    }
}

void logger_log(log_level_t level, const char *file, int line, const char *func, const char *format, ...) {
    /* Check if we should log this level */
    if (level < g_logger.min_level) {
        return;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    
    /* If not initialized, use default console output */
    if (!g_logger.initialized) {
        g_logger.flags = LOG_FLAG_CONSOLE | LOG_FLAG_TIMESTAMP;
    }
    
    char message[2048];
    char formatted_message[2560];
    va_list args;
    
    /* Format the user message */
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Add file/line/function information */
    const char *basename = strrchr(file, '/');
    basename = basename ? basename + 1 : file;
    
    snprintf(formatted_message, sizeof(formatted_message), 
             "%s (%s:%d in %s())", message, basename, line, func);
    
    logger_output(level, formatted_message);
    
    pthread_mutex_unlock(&g_logger.mutex);
}

void logger_log_simple(log_level_t level, const char *format, ...) {
    /* Check if we should log this level */
    if (level < g_logger.min_level) {
        return;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    
    /* If not initialized, use default console output */
    if (!g_logger.initialized) {
        g_logger.flags = LOG_FLAG_CONSOLE | LOG_FLAG_TIMESTAMP;
    }
    
    char message[2048];
    va_list args;
    
    /* Format the user message */
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    logger_output(level, message);
    
    pthread_mutex_unlock(&g_logger.mutex);
}
