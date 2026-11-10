include_guard(GLOBAL)

# Hi3516CV610. The MPP headers and libraries come from one of two places:
# the grouped platform files (generated with the
# board image), or a built vendor SDK tree when HI_SDK_ROOT is set.
#
#   HIFW_SDK_INCLUDE_DIR         MPP headers
#   HIFW_SDK_LIB_DIR             MPP shared and static libraries
#   HIFW_SDK_CONFIG_DIR          directory of the SDK's autoconf.h
#   HIFW_SDK_SENSOR_COMMON_DIR   headers shared by the sensor drivers
macro(hifw_chip_initialize)
    if(HI_SDK_ROOT)
        if(NOT EXISTS "${HI_SDK_ROOT}/soc/include/ot_type.h" OR
           NOT EXISTS "${HI_SDK_ROOT}/soc/libs/musl")
            message(FATAL_ERROR
                "Hi3516CV610 MPP headers and libraries not found in the "
                "vendor SDK '${HI_SDK_ROOT}'; build the SDK first")
        endif()
        set(HIFW_SDK_INCLUDE_DIR "${HI_SDK_ROOT}/soc/include"
            CACHE PATH "Hi3516CV610 MPP headers" FORCE)
        set(HIFW_SDK_LIB_DIR "${HI_SDK_ROOT}/soc/libs/musl"
            CACHE PATH "Hi3516CV610 MPP libraries" FORCE)
        set(HIFW_SDK_CONFIG_DIR "${HI_SDK_ROOT}/soc/config/gcc"
            CACHE PATH "Hi3516CV610 SDK configuration header" FORCE)
        set(HIFW_SDK_SENSOR_COMMON_DIR
            "${HI_SDK_ROOT}/soc/drivers/mpp/cbb/isp/user/sensor/hi3516cv610/common"
            CACHE PATH "Hi3516CV610 sensor driver common headers" FORCE)
    else()
        if(NOT HIFW_PLATFORM_PATH)
            if(DEFINED ENV{HIFW_PLATFORM_PATH})
                set(HIFW_PLATFORM_PATH "$ENV{HIFW_PLATFORM_PATH}")
            else()
                set(HIFW_PLATFORM_PATH "${HIFW_ROOT}/platform/hi3516cv610")
            endif()
        endif()
        if(NOT EXISTS "${HIFW_PLATFORM_PATH}/manifest.json")
            message(FATAL_ERROR "Platform missing at ${HIFW_PLATFORM_PATH}; import it with scripts/install-platform.sh")
        endif()
        include("${HIFW_ROOT}/cmake/platform.cmake")
    endif()
endmacro()
