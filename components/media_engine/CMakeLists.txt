# Media engine: video capture and encoding, audio capture and playback on
# top of the media pipeline.
#
# The GC4023 sensor driver is built into the component so that the I2C
# address can be chosen at run time: sensor/galaxycore_gc4023 is the
# upstream driver as it is, and the patches under sensor/patches are
# applied to a copy of it in the build directory. The code shared by the
# sensor drivers comes from the SDK's libsns_gc4023.a, or is compiled from
# the SDK's sensor_common.c when the SDK has not built that library.
set(_sensor_upstream "${CMAKE_CURRENT_LIST_DIR}/sensor/galaxycore_gc4023")
file(GLOB _sensor_patches "${CMAKE_CURRENT_LIST_DIR}/sensor/patches/*.patch")
list(SORT _sensor_patches)
file(GLOB _sensor_inputs "${_sensor_upstream}/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${_sensor_inputs} ${_sensor_patches})

# Copy and patch once per driver and patch content.
set(_sensor_dir "${CMAKE_CURRENT_BINARY_DIR}/galaxycore_gc4023")
set(_sensor_hashes "")
foreach(_file IN LISTS _sensor_inputs _sensor_patches)
    file(SHA256 "${_file}" _hash)
    string(APPEND _sensor_hashes "${_hash};")
endforeach()
string(SHA256 _sensor_hash "${_sensor_hashes}")
if(NOT EXISTS "${_sensor_dir}/.hifw-${_sensor_hash}")
    file(REMOVE_RECURSE "${_sensor_dir}")
    # The SDK directory may be read-only; the copy has to be writable.
    file(COPY "${_sensor_upstream}/" DESTINATION "${_sensor_dir}"
        NO_SOURCE_PERMISSIONS)
    foreach(_patch IN LISTS _sensor_patches)
        execute_process(
            COMMAND patch --batch --forward -p1 -i "${_patch}"
            WORKING_DIRECTORY "${_sensor_dir}"
            RESULT_VARIABLE _sensor_patch_result
            OUTPUT_QUIET)
        if(NOT _sensor_patch_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${_patch} to the GC4023 driver")
        endif()
    endforeach()
    file(WRITE "${_sensor_dir}/.hifw-${_sensor_hash}" "")
endif()

set(_media_engine_sources
    src/media_context.c
    src/media_video.c
    src/media_overlay.c
    src/media_label.c
    src/media_audio.c
    src/media_ogg.c
    "${_sensor_dir}/gc4023_cmos.c"
    "${_sensor_dir}/gc4023_sensor_ctrl.c")

set(_sdk_lib_dir "${HIFW_SDK_LIB_DIR}")
set(_sensor_common_dir "${HIFW_SDK_SENSOR_COMMON_DIR}")
set(_sensor_common_lib "${_sdk_lib_dir}/libsns_gc4023.a")
if(NOT EXISTS "${_sensor_common_lib}")
    set(_sensor_common_lib "")
    if(NOT EXISTS "${_sensor_common_dir}/sensor_common.c")
        message(FATAL_ERROR
            "media_engine: neither ${_sdk_lib_dir}/libsns_gc4023.a nor "
            "${_sensor_common_dir}/sensor_common.c exists")
    endif()
    list(APPEND _media_engine_sources "${_sensor_common_dir}/sensor_common.c")
    # Vendor code, built the way the SDK builds it rather than with the
    # component's warning level.
    set_source_files_properties("${_sensor_common_dir}/sensor_common.c"
        PROPERTIES COMPILE_OPTIONS "-D_GNU_SOURCE;-w")
endif()

hifw_component_register(
    SRCS ${_media_engine_sources}
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS
        include/media_engine
        "${_sensor_dir}"
        "${_sensor_common_dir}"
    REQUIRES media_pipeline
    LINK_LIBRARIES
        -Wl,--start-group
        "${_sdk_lib_dir}/libsecurec.so"
        "${_sdk_lib_dir}/libss_mpi.so"
        "${_sdk_lib_dir}/libss_mpi_sysbind.so"
        "${_sdk_lib_dir}/libss_mpi_sysmem.so"
        "${_sdk_lib_dir}/libss_mpi_audio.so"
        "${_sdk_lib_dir}/libss_mpi_audio_adp.a"
        "${_sdk_lib_dir}/libss_mpi_ive.so"
        "${_sdk_lib_dir}/libss_mpi_isp.so"
        "${_sdk_lib_dir}/libot_mpi_isp.so"
        "${_sdk_lib_dir}/libss_mpi_awb.so"
        "${_sdk_lib_dir}/libss_mpi_ae.so"
        "${_sdk_lib_dir}/libot_osal.so"
        "${_sdk_lib_dir}/libot_base.so"
        "${_sdk_lib_dir}/libot_irq.so"
        "${_sdk_lib_dir}/libot_sys.so"
        "${_sdk_lib_dir}/libot_vb.so"
        "${_sdk_lib_dir}/libbnr.so"
        "${_sdk_lib_dir}/libdehaze.so"
        "${_sdk_lib_dir}/libacs.so"
        "${_sdk_lib_dir}/libcalcflicker.so"
        "${_sdk_lib_dir}/libir_auto.so"
        "${_sdk_lib_dir}/libldci.so"
        "${_sdk_lib_dir}/libdrc.so"
        "${_sdk_lib_dir}/libextend_stats.so"
        "${_sdk_lib_dir}/libvoice_engine.so"
        "${_sdk_lib_dir}/libdnvqe.so"
        "${_sdk_lib_dir}/libupvqe.so"
        "${_sdk_lib_dir}/libvqe_aec.so"
        "${_sdk_lib_dir}/libvqe_agc.so"
        "${_sdk_lib_dir}/libvqe_anr.so"
        "${_sdk_lib_dir}/libvqe_eq.so"
        "${_sdk_lib_dir}/libvqe_hpf.so"
        "${_sdk_lib_dir}/libvqe_hs.so"
        "${_sdk_lib_dir}/libvqe_record.so"
        "${_sdk_lib_dir}/libvqe_res.so"
        "${_sdk_lib_dir}/libvqe_talkv2.so"
        "${_sdk_lib_dir}/libaac_comm.so"
        "${_sdk_lib_dir}/libaac_dec.so"
        "${_sdk_lib_dir}/libaac_enc.so"
        "${_sdk_lib_dir}/libaac_sbr_dec.so"
        "${_sdk_lib_dir}/libaac_sbr_enc.so"
        "${_sdk_lib_dir}/libmp3_dec.so"
        "${_sdk_lib_dir}/libmp3_enc.so"
        "${_sdk_lib_dir}/libmp3_lame.so"
        "${_sdk_lib_dir}/libopus.so"
        ${_sensor_common_lib}
        -Wl,--end-group
        pthread m dl
    PRIV_COMPILE_DEFINITIONS _POSIX_C_SOURCE=200809L __HI3516CV610__
    COMPILE_OPTIONS -Wall -Wextra -Werror)
