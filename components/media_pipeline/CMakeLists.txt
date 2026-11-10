# MAPI media pipeline for the Hi3516CV610, built as a static library
# together with its logging library hi_log.
#
# The source archive next to this file is the upstream source as it is
# (directories pipeline/ and log/); the patches under patches/ are applied
# to it, in order, when it is unpacked into the build directory. To move to
# a newer upstream, replace the archive and refresh the patches.
set(_hifw_pipeline_version "557c69bd")
set(_hifw_pipeline_name "media-pipeline-${_hifw_pipeline_version}")
set(_hifw_pipeline_archive "${CMAKE_CURRENT_LIST_DIR}/${_hifw_pipeline_name}.tar.xz")
if(NOT EXISTS "${_hifw_pipeline_archive}")
    message(FATAL_ERROR "media pipeline source archive not found: ${_hifw_pipeline_archive}")
endif()
file(GLOB _hifw_pipeline_patches "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch")
list(SORT _hifw_pipeline_patches)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_hifw_pipeline_archive}" ${_hifw_pipeline_patches})

# Unpack and patch once per archive and patch content.
set(_hifw_pipeline_stage "${CMAKE_CURRENT_BINARY_DIR}/source")
set(_hifw_pipeline_source "${_hifw_pipeline_stage}/${_hifw_pipeline_name}")
set(_hifw_pipeline_hashes "")
foreach(_file IN ITEMS "${_hifw_pipeline_archive}" ${_hifw_pipeline_patches})
    file(SHA256 "${_file}" _hash)
    string(APPEND _hifw_pipeline_hashes "${_hash};")
endforeach()
string(SHA256 _hifw_pipeline_hash "${_hifw_pipeline_hashes}")
set(_hifw_pipeline_stamp "${_hifw_pipeline_source}/.hifw-${_hifw_pipeline_hash}")
if(NOT EXISTS "${_hifw_pipeline_stamp}")
    file(REMOVE_RECURSE "${_hifw_pipeline_stage}")
    file(MAKE_DIRECTORY "${_hifw_pipeline_stage}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xJf "${_hifw_pipeline_archive}"
        WORKING_DIRECTORY "${_hifw_pipeline_stage}"
        RESULT_VARIABLE _hifw_result)
    if(NOT _hifw_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract the media pipeline (${_hifw_result})")
    endif()
    foreach(_patch IN LISTS _hifw_pipeline_patches)
        execute_process(
            COMMAND patch --batch --forward -p1 -i "${_patch}"
            WORKING_DIRECTORY "${_hifw_pipeline_source}"
            RESULT_VARIABLE _hifw_result
            OUTPUT_QUIET)
        if(NOT _hifw_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${_patch} to the media pipeline")
        endif()
    endforeach()
    file(WRITE "${_hifw_pipeline_stamp}" "${_hifw_pipeline_hash}\n")
endif()

set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(CFG_SUPPORT_DISP OFF CACHE BOOL "" FORCE)
set(CFG_SUPPORT_LVGL OFF CACHE BOOL "" FORCE)
set(CFG_SUPPORT_HDMI OFF CACHE BOOL "" FORCE)
set(CFG_SUPPORT_RECORDVQE ON CACHE BOOL "" FORCE)
set(CFG_SUPPORT_TALKVQE ON CACHE BOOL "" FORCE)
set(CFG_SUPPORT_MP3 ON CACHE BOOL "" FORCE)
set(CFG_SENSOR_LIST "" CACHE STRING "" FORCE)

# MPP headers and libraries of the SDK.
set(MPP_HEADER_LINUX "${HIFW_SDK_INCLUDE_DIR}" CACHE PATH "" FORCE)
set(MPP_LIB_LINUX "${HIFW_SDK_LIB_DIR}" CACHE PATH "" FORCE)
set(MPP_CONFIG_LINUX "${HIFW_SDK_CONFIG_DIR}" CACHE PATH "" FORCE)

set(_hifw_saved_c_extensions "${CMAKE_C_EXTENSIONS}")
set(CMAKE_C_EXTENSIONS ON)

# An application may provide its own hi_log component; otherwise use the
# one shipped with the pipeline.
if(NOT TARGET hi_log)
    add_subdirectory("${_hifw_pipeline_source}/log"
        "${CMAKE_BINARY_DIR}/hi_log_build")
endif()

add_subdirectory("${_hifw_pipeline_source}/pipeline"
    "${CMAKE_BINARY_DIR}/hifw/media_pipeline" EXCLUDE_FROM_ALL)
set(CMAKE_C_EXTENSIONS "${_hifw_saved_c_extensions}")

hifw_component_register(
    LINK_LIBRARIES pipeline_config hi_mapi_static)
