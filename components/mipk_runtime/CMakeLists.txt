# Model inference runtime (mipk), built from source into the application.
#
# mipk and mipk_extra_module are repositories of their own, checked out
# next to this file as git submodules. MIPK_PATH and MIPK_EXTRA_MODULE_PATH
# (CMake variable or environment) can point at other checkouts. Kconfig
# switches select the modules; toolchain, vendor libraries and the device
# memory profile come from the SDK.

if(NOT MIPK_PATH AND DEFINED ENV{MIPK_PATH})
    set(MIPK_PATH "$ENV{MIPK_PATH}")
endif()
if(NOT MIPK_PATH)
    set(MIPK_PATH "${CMAKE_CURRENT_LIST_DIR}/mipk")
endif()
if(NOT EXISTS "${MIPK_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "mipk not found at ${MIPK_PATH}; run 'git submodule update --init' "
        "in the SDK, or set MIPK_PATH to a checkout")
endif()
get_filename_component(MIPK_PATH "${MIPK_PATH}" REALPATH)

function(_mipk_config_option config_name cmake_name)
    set(_enabled OFF)
    if(HIFW_CONFIG_HEADER AND EXISTS "${HIFW_CONFIG_HEADER}")
        file(STRINGS "${HIFW_CONFIG_HEADER}" _lines
            REGEX "^#define[ \t]+CONFIG_${config_name}([ \t]+1)?$")
        if(_lines)
            set(_enabled ON)
        endif()
    endif()
    set(${cmake_name} ${_enabled} CACHE BOOL "" FORCE)
endfunction()

# The string value of a Kconfig option, empty when unset.
function(_mipk_config_string config_name out_var)
    set(_value "")
    if(HIFW_CONFIG_HEADER AND EXISTS "${HIFW_CONFIG_HEADER}")
        file(STRINGS "${HIFW_CONFIG_HEADER}" _lines
            REGEX "^#define[ \t]+CONFIG_${config_name}[ \t]+\"")
        if(_lines)
            list(GET _lines 0 _line)
            string(REGEX REPLACE "^#define[ \t]+CONFIG_${config_name}[ \t]+\"(.*)\"[ \t]*$"
                "\\1" _value "${_line}")
        endif()
    endif()
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# mipk looks for the vendor SDK as a directory with soc/include,
# soc/config/gcc and soc/libs/musl. A vendor SDK tree already has that
# shape; otherwise use the ordinary build copies prepared by the platform.
if(HI_SDK_ROOT)
    set(_mipk_hisdk "${HI_SDK_ROOT}")
else()
    set(_mipk_hisdk "${HIFW_VENDOR_COMPAT_ROOT}")
endif()
set(MIPK_HI_SDK_ROOT "${_mipk_hisdk}" CACHE PATH "" FORCE)
set(MIPK_PROFILE "hi3516cv610" CACHE STRING "" FORCE)
set(MIPK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MIPK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MIPK_BACKEND_NULL OFF CACHE BOOL "" FORCE)
set(MIPK_BACKEND_ONNXRUNTIME OFF CACHE BOOL "" FORCE)
set(MIPK_BACKEND_SVP_ACL ON CACHE BOOL "" FORCE)
set(MIPK_PLATFORM_POSIX ON CACHE BOOL "" FORCE)
set(MIPK_PLATFORM_HISILICON ON CACHE BOOL "" FORCE)
foreach(_model YOLO YOLO_POSE CLASSIFICATION KEYPOINTS MICROWAKEWORD)
    _mipk_config_option(MIPK_MODEL_${_model} MIPK_MODEL_${_model})
endforeach()
# The audio operators exist only to feed the wake word decoder, so they
# follow it rather than having a switch of their own.
set(MIPK_OPS_AUDIO ${MIPK_MODEL_MICROWAKEWORD} CACHE BOOL "" FORCE)

# Out-of-tree modules from the mipk_extra_module repository, the submodule
# next to this file unless MIPK_EXTRA_MODULE_PATH names another checkout.
# Each known module has a Kconfig switch
# (CONFIG_MIPK_EXTRA_<NAME>); the string MIPK_EXTRA_MODULES adds others by
# directory name. mipk registers and archives them like its built-in modules.
if(NOT MIPK_EXTRA_MODULE_PATH AND DEFINED ENV{MIPK_EXTRA_MODULE_PATH})
    set(MIPK_EXTRA_MODULE_PATH "$ENV{MIPK_EXTRA_MODULE_PATH}")
endif()
if(NOT MIPK_EXTRA_MODULE_PATH)
    set(MIPK_EXTRA_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/mipk_extra_module")
endif()
set(_mipk_extra_names)
foreach(_extra fastestdet mediapipe_hand)
    string(TOUPPER "${_extra}" _extra_upper)
    _mipk_config_option(MIPK_EXTRA_${_extra_upper} _mipk_extra_${_extra})
    if(_mipk_extra_${_extra})
        list(APPEND _mipk_extra_names "${_extra}")
    endif()
endforeach()
_mipk_config_string(MIPK_EXTRA_MODULES _mipk_extra_others)
separate_arguments(_mipk_extra_others)
list(APPEND _mipk_extra_names ${_mipk_extra_others})
list(REMOVE_DUPLICATES _mipk_extra_names)
set(_mipk_extra_dirs)
foreach(_name IN LISTS _mipk_extra_names)
    set(_dir "${MIPK_EXTRA_MODULE_PATH}/${_name}")
    if(NOT EXISTS "${_dir}/module.json")
        message(FATAL_ERROR "mipk extra module '${_name}' not found at ${_dir}; run 'git submodule update --init' in the SDK, or set MIPK_EXTRA_MODULE_PATH")
    endif()
    list(APPEND _mipk_extra_dirs "${_dir}")
endforeach()
set(MIPK_EXTRA_MODULE_DIRS "${_mipk_extra_dirs}" CACHE STRING "" FORCE)

add_subdirectory("${MIPK_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/mipk" EXCLUDE_FROM_ALL)

hifw_component_register(LINK_LIBRARIES mipk::mipk)
