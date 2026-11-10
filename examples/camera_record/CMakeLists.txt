cmake_minimum_required(VERSION 3.16)
project(camera_record LANGUAGES C CXX ASM)

# APP_SDK_PATH is exported by "source <sdk>/export.sh".
if(DEFINED ENV{APP_SDK_PATH} AND NOT "$ENV{APP_SDK_PATH}" STREQUAL "")
    get_filename_component(APP_SDK_PATH "$ENV{APP_SDK_PATH}" ABSOLUTE)
else()
    get_filename_component(APP_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

include("${APP_SDK_PATH}/cmake/project.cmake")
hifw_project(camera_record)
