#include <stdio.h>

#include "hifw_config.h"
#include "hi_log.h"

int main(void)
{
    printf("helloworld example\n");
#ifdef NDEBUG
    printf("build mode: release\n");
#else
    printf("build mode: debug\n");
#endif
    hi_log_error("error log enabled");
    hi_log_warn("warning log enabled");
    hi_log_info(CONFIG_HELLOWORLD_MESSAGE);
    hi_log_debug("debug log enabled");
    return 0;
}
