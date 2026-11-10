#ifndef HI_LOG_H
#define HI_LOG_H

typedef enum {
    HI_LOG_LEVEL_NONE = 0,
    HI_LOG_LEVEL_ERROR,
    HI_LOG_LEVEL_WARN,
    HI_LOG_LEVEL_INFO,
    HI_LOG_LEVEL_DEBUG,
} hi_log_level_t;

hi_log_level_t hi_log_configured_level(void);
void hi_log_error(const char *message);
void hi_log_warn(const char *message);
void hi_log_info(const char *message);
void hi_log_debug(const char *message);

#endif
