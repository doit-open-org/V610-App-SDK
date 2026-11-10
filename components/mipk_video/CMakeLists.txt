hifw_component_register(
    SRCS src/mipk_video.c
    INCLUDE_DIRS include
    REQUIRES media_engine mipk_runtime
    COMPILE_OPTIONS -Wall -Wextra -Werror)
