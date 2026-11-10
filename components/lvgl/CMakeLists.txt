# LVGL 9.5 for the Linux DRM display and evdev input drivers.
#
# The source archive next to this file is upstream LVGL 9.5.0 reduced to
# the library, its CMake support and the widgets demo; the files it keeps
# are unmodified. The patches under patches/ are applied to it, in order,
# when it is unpacked into the build directory.
#
# libdrm, which the DRM display driver needs, is compiled from the reduced
# source archive next to this file and linked statically. An LVGL
# application therefore needs nothing but the C runtime on the board, and
# the same binary runs on every image, whether or not it carries libdrm.
set(_hifw_lvgl_version "9.5.0")
set(_hifw_lvgl_name "lvgl-${_hifw_lvgl_version}-reduced")
set(_hifw_lvgl_archive "${CMAKE_CURRENT_LIST_DIR}/${_hifw_lvgl_name}.tar.xz")
if(NOT EXISTS "${_hifw_lvgl_archive}")
    message(FATAL_ERROR "LVGL source archive not found: ${_hifw_lvgl_archive}")
endif()
file(GLOB _hifw_lvgl_patches "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch")
list(SORT _hifw_lvgl_patches)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_hifw_lvgl_archive}"
    ${_hifw_lvgl_patches}
    "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h")

# Unpack and patch once per archive and patch content.
set(_hifw_lvgl_stage "${CMAKE_CURRENT_BINARY_DIR}/source")
set(_hifw_lvgl_source "${_hifw_lvgl_stage}/${_hifw_lvgl_name}")
set(_hifw_lvgl_hashes "")
foreach(_file IN ITEMS "${_hifw_lvgl_archive}" ${_hifw_lvgl_patches})
    file(SHA256 "${_file}" _hash)
    string(APPEND _hifw_lvgl_hashes "${_hash};")
endforeach()
string(SHA256 _hifw_lvgl_hash "${_hifw_lvgl_hashes}")
set(_hifw_lvgl_stamp "${_hifw_lvgl_source}/.hifw-${_hifw_lvgl_hash}")
if(NOT EXISTS "${_hifw_lvgl_stamp}")
    file(REMOVE_RECURSE "${_hifw_lvgl_stage}")
    file(MAKE_DIRECTORY "${_hifw_lvgl_stage}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xJf "${_hifw_lvgl_archive}"
        WORKING_DIRECTORY "${_hifw_lvgl_stage}"
        RESULT_VARIABLE _hifw_extract_result)
    if(NOT _hifw_extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract LVGL (${_hifw_extract_result})")
    endif()
    foreach(_patch IN LISTS _hifw_lvgl_patches)
        execute_process(
            COMMAND patch --batch --forward --no-backup-if-mismatch -p1 -i "${_patch}"
            WORKING_DIRECTORY "${_hifw_lvgl_source}"
            RESULT_VARIABLE _hifw_patch_result
            OUTPUT_QUIET)
        if(NOT _hifw_patch_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${_patch} to LVGL")
        endif()
    endforeach()
    file(WRITE "${_hifw_lvgl_stamp}" "${_hifw_lvgl_hash}\n")
endif()

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS ON CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${_hifw_lvgl_source}"
    "${CMAKE_CURRENT_BINARY_DIR}/lvgl-build" EXCLUDE_FROM_ALL)

target_compile_definitions(lvgl PRIVATE _POSIX_C_SOURCE=200809L)
target_link_libraries(lvgl PUBLIC pthread dl m)

set(_hifw_libdrm_version "2.4.131")
set(_hifw_libdrm_archive
    "${CMAKE_CURRENT_LIST_DIR}/libdrm-${_hifw_libdrm_version}-reduced.tar.xz")
set(_hifw_libdrm_source
    "${CMAKE_CURRENT_BINARY_DIR}/source/libdrm-${_hifw_libdrm_version}")
file(SHA256 "${_hifw_libdrm_archive}" _hifw_libdrm_hash)
set(_hifw_libdrm_stamp "${_hifw_libdrm_source}/.hifw-${_hifw_libdrm_hash}")
if(NOT EXISTS "${_hifw_libdrm_stamp}")
    file(REMOVE_RECURSE "${_hifw_libdrm_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xJf "${_hifw_libdrm_archive}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/source"
        RESULT_VARIABLE _hifw_extract_result)
    if(NOT _hifw_extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract libdrm (${_hifw_extract_result})")
    endif()
    file(WRITE "${_hifw_libdrm_stamp}" "${_hifw_libdrm_hash}\n")
endif()

add_library(hifw_lvgl_libdrm STATIC EXCLUDE_FROM_ALL
    "${_hifw_libdrm_source}/xf86drm.c"
    "${_hifw_libdrm_source}/xf86drmHash.c"
    "${_hifw_libdrm_source}/xf86drmRandom.c"
    "${_hifw_libdrm_source}/xf86drmSL.c"
    "${_hifw_libdrm_source}/xf86drmMode.c")
# config.h is the one the library's own build system generates for
# this target.
target_compile_options(hifw_lvgl_libdrm PRIVATE
    -include "${_hifw_libdrm_source}/config.h" -w)
target_include_directories(hifw_lvgl_libdrm SYSTEM PUBLIC
    "${_hifw_libdrm_source}"
    "${_hifw_libdrm_source}/include/drm")
target_link_libraries(hifw_lvgl_libdrm PUBLIC m)
target_link_libraries(lvgl PUBLIC hifw_lvgl_libdrm)
# The widgets demo is a separate library; link it only where it is used.
hifw_component_register(LINK_LIBRARIES lvgl)
