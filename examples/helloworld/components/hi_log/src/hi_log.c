#include "hi_log.h"

#include <stdio.h>

#include "hifw_config.h"

#if defined(CONFIG_HI_LOG_LEVEL_NONE)
#define HI_LOG_CONFIGURED_LEVEL HI_LOG_LEVEL_NONE
#elif defined(CONFIG_HI_LOG_LEVEL_ERROR)
#define HI_LOG_CONFIGURED_LEVEL HI_LOG_LEVEL_ERROR
#elif defined(CONFIG_HI_LOG_LEVEL_WARN)
#define HI_LOG_CONFIGURED_LEVEL HI_LOG_LEVEL_WARN
#elif defined(CONFIG_HI_LOG_LEVEL_DEBUG)
#define HI_LOG_CONFIGURED_LEVEL HI_LOG_LEVEL_DEBUG
#else
#define HI_LOG_CONFIGURED_LEVEL HI_LOG_LEVEL_INFO
#endif

static void hi_log_write(hi_log_level_t level, const char *name,
                         const char *message)
{
    if (level > HI_LOG_CONFIGURED_LEVEL || message == NULL) {
        return;
    }
    printf("[%s] %s\n", name, message);
}

hi_log_level_t hi_log_configured_level(void)
{
    return HI_LOG_CONFIGURED_LEVEL;
}

void hi_log_error(const char *message)
{
    hi_log_write(HI_LOG_LEVEL_ERROR, "error", message);
}

void hi_log_warn(const char *message)
{
    hi_log_write(HI_LOG_LEVEL_WARN, "warn", message);
}

void hi_log_info(const char *message)
{
    hi_log_write(HI_LOG_LEVEL_INFO, "info", message);
}

void hi_log_debug(const char *message)
{
    hi_log_write(HI_LOG_LEVEL_DEBUG, "debug", message);
}
