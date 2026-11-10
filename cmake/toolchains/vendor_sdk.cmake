# CMake toolchain file for HiFW projects built against a vendor SDK tree.
#
# Used instead of hi3516cv610.cmake when HI_SDK_ROOT (CMake variable or
# environment) names a built Hi3516CV610 SDK: the MPP headers and libraries
# are then taken from that tree, and the compiler is the plain cross
# compiler found on PATH. CROSS_PREFIX selects it; without it the compilers
# the vendor has shipped for this chip are tried in turn.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT HI_SDK_ROOT AND DEFINED ENV{HI_SDK_ROOT})
    set(HI_SDK_ROOT "$ENV{HI_SDK_ROOT}")
endif()
if(NOT HI_SDK_ROOT OR NOT EXISTS "${HI_SDK_ROOT}/soc/include")
    message(FATAL_ERROR
        "vendor SDK not found (HI_SDK_ROOT='${HI_SDK_ROOT}'); it must be a "
        "built Hi3516CV610 SDK with soc/include and soc/libs")
endif()
get_filename_component(HI_SDK_ROOT "${HI_SDK_ROOT}" ABSOLUTE)
set(HI_SDK_ROOT "${HI_SDK_ROOT}" CACHE PATH "Hi3516CV610 SDK root" FORCE)

if(NOT CROSS_PREFIX AND DEFINED ENV{CROSS_PREFIX})
    set(CROSS_PREFIX "$ENV{CROSS_PREFIX}")
endif()
if(NOT CROSS_PREFIX)
    foreach(_hifw_prefix IN ITEMS
            "arm-v01c02-linux-musleabi-" "arm-openeuler-linux-musleabi-")
        find_program(_HIFW_PROBE_${_hifw_prefix} NAMES "${_hifw_prefix}gcc")
        if(_HIFW_PROBE_${_hifw_prefix})
            set(CROSS_PREFIX "${_hifw_prefix}")
            break()
        endif()
    endforeach()
endif()
set(CROSS_PREFIX "${CROSS_PREFIX}" CACHE STRING "Cross compiler prefix" FORCE)
# Keep the choices in the compiler checks CMake runs in sub-projects.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES HI_SDK_ROOT CROSS_PREFIX)

find_program(_HIFW_C_COMPILER NAMES "${CROSS_PREFIX}gcc")
find_program(_HIFW_CXX_COMPILER NAMES "${CROSS_PREFIX}g++")
find_program(_HIFW_AR NAMES "${CROSS_PREFIX}ar")
find_program(_HIFW_STRIP NAMES "${CROSS_PREFIX}strip")
if(NOT CROSS_PREFIX OR NOT _HIFW_C_COMPILER OR NOT _HIFW_CXX_COMPILER OR NOT _HIFW_AR)
    message(FATAL_ERROR
        "cross compiler '${CROSS_PREFIX}gcc' was not found in PATH; "
        "source export.sh, or set CROSS_PREFIX")
endif()

# The compiler defaults to -mfloat-abi=soft, which emulates every float
# operation in software although the Cortex-A7 has VFPv4 and NEON. softfp
# keeps the soft-float calling convention, so objects still link with the
# SDK libraries, but uses the FPU inside functions.
set(HIFW_ARM_ABI_FLAGS "-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4")
set(CMAKE_C_FLAGS_INIT "${HIFW_ARM_ABI_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${HIFW_ARM_ABI_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${HIFW_ARM_ABI_FLAGS}")

set(CMAKE_C_COMPILER "${_HIFW_C_COMPILER}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_HIFW_CXX_COMPILER}" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${_HIFW_C_COMPILER}" CACHE FILEPATH "" FORCE)
set(CMAKE_AR "${_HIFW_AR}" CACHE FILEPATH "" FORCE)
if(_HIFW_STRIP)
    set(CMAKE_STRIP "${_HIFW_STRIP}" CACHE FILEPATH "" FORCE)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
