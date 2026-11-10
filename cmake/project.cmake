include_guard(GLOBAL)

get_filename_component(HIFW_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/component.cmake")

macro(_hifw_initialize_chip)
    if(NOT HIFW_CHIP)
        message(FATAL_ERROR "HIFW_CHIP is not set; run hifw.py set-target first")
    endif()
    set(_hifw_chip_file "${HIFW_ROOT}/cmake/chips/${HIFW_CHIP}.cmake")
    if(NOT EXISTS "${_hifw_chip_file}")
        message(FATAL_ERROR
            "HiFW chip definition not found for '${HIFW_CHIP}': ${_hifw_chip_file}")
    endif()
    include("${_hifw_chip_file}")
    if(NOT COMMAND hifw_chip_initialize)
        message(FATAL_ERROR
            "HiFW chip definition '${HIFW_CHIP}' must provide hifw_chip_initialize()")
    endif()
    hifw_chip_initialize()
endmacro()

function(_hifw_overlay_component_dirs _names_var _dirs_var)
    set(_names ${${_names_var}})
    set(_dirs ${${_dirs_var}})

    foreach(_dir IN LISTS ARGN)
        if(NOT EXISTS "${_dir}/CMakeLists.txt")
            continue()
        endif()
        get_filename_component(_name "${_dir}" NAME)
        list(FIND _names "${_name}" _index)
        if(NOT _index EQUAL -1)
            list(GET _dirs ${_index} _previous_dir)
            list(REMOVE_AT _names ${_index})
            list(REMOVE_AT _dirs ${_index})
            message(STATUS
                "[hifw] component '${_name}' overridden: ${_previous_dir} -> ${_dir}")
        endif()
        list(APPEND _names "${_name}")
        list(APPEND _dirs "${_dir}")
    endforeach()

    set(${_names_var} "${_names}" PARENT_SCOPE)
    set(${_dirs_var} "${_dirs}" PARENT_SCOPE)
endfunction()

macro(hifw_project _project_name)
    option(HIFW_ENABLE_SDK "Enable target SDK integration" ON)
    set(HIFW_BOARD "cove_610" CACHE STRING "HiFW board profile")
    add_compile_definitions(HIFW_PROJECT_NAME="${_project_name}")

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_SKIP_RPATH TRUE)

    if(HIFW_ENABLE_SDK)
        _hifw_initialize_chip()
    endif()

    if(NOT HIFW_OUTPUT_DIR)
        set(HIFW_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/out")
    endif()
    get_filename_component(HIFW_OUTPUT_DIR "${HIFW_OUTPUT_DIR}" ABSOLUTE)

    set_property(GLOBAL PROPERTY HIFW_COMPONENT_NAMES "")
    set(_hifw_component_names)
    set(_hifw_component_dirs)

    file(GLOB _hifw_components CONFIGURE_DEPENDS LIST_DIRECTORIES true
        "${HIFW_ROOT}/components/*")
    _hifw_overlay_component_dirs(_hifw_component_names _hifw_component_dirs
        ${_hifw_components})

    if(EXTRA_COMPONENT_DIRS)
        _hifw_overlay_component_dirs(_hifw_component_names _hifw_component_dirs
            ${EXTRA_COMPONENT_DIRS})
    endif()

    file(GLOB _hifw_project_components CONFIGURE_DEPENDS LIST_DIRECTORIES true
        "${CMAKE_CURRENT_SOURCE_DIR}/components/*")
    _hifw_overlay_component_dirs(_hifw_component_names _hifw_component_dirs
        ${_hifw_project_components})

    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/main")
        _hifw_overlay_component_dirs(_hifw_component_names _hifw_component_dirs
            "${CMAKE_CURRENT_SOURCE_DIR}/main")
    endif()

    list(FIND _hifw_component_names main _hifw_main_index)
    if(_hifw_main_index EQUAL -1)
        message(FATAL_ERROR
            "Project '${_project_name}' must provide a main component")
    endif()

    set(_hifw_requested_components ${HIFW_COMPONENTS} ${HIFW_EXTRA_COMPONENTS})
    list(REMOVE_DUPLICATES _hifw_requested_components)
    foreach(_hifw_requested_component IN LISTS _hifw_requested_components)
        list(FIND _hifw_component_names
            "${_hifw_requested_component}" _hifw_requested_index)
        if(_hifw_requested_index EQUAL -1)
            message(FATAL_ERROR
                "Enabled component '${_hifw_requested_component}' was not found")
        endif()
    endforeach()

    set(_hifw_selected_components)
    foreach(_hifw_component_name IN LISTS _hifw_component_names)
        list(FIND _hifw_requested_components
            "${_hifw_component_name}" _hifw_enabled_index)
        if(NOT _hifw_component_name STREQUAL "main" AND
           _hifw_enabled_index EQUAL -1)
            continue()
        endif()
        list(FIND _hifw_component_names
            "${_hifw_component_name}" _hifw_component_index)
        list(GET _hifw_component_dirs
            ${_hifw_component_index} _hifw_component_dir)
        list(APPEND _hifw_selected_components "${_hifw_component_name}")
        add_subdirectory("${_hifw_component_dir}"
            "${CMAKE_CURRENT_BINARY_DIR}/components/${_hifw_component_name}"
            EXCLUDE_FROM_ALL)
    endforeach()

    if(NOT TARGET __hifw_component_main)
        message(FATAL_ERROR
            "Project '${_project_name}' main component did not register a target")
    endif()

    hifw_finalize_components()

    if(HIFW_CONFIG_HEADER)
        get_filename_component(HIFW_CONFIG_HEADER "${HIFW_CONFIG_HEADER}" ABSOLUTE)
        if(NOT EXISTS "${HIFW_CONFIG_HEADER}")
            message(FATAL_ERROR "Configuration header not found: ${HIFW_CONFIG_HEADER}")
        endif()
        get_filename_component(_hifw_config_dir "${HIFW_CONFIG_HEADER}" DIRECTORY)
        add_library(__hifw_project_config INTERFACE)
        add_library(hifw::config ALIAS __hifw_project_config)
        target_include_directories(__hifw_project_config INTERFACE
            "${_hifw_config_dir}")

        get_property(_hifw_registered_names GLOBAL PROPERTY HIFW_COMPONENT_NAMES)
        foreach(_hifw_registered_name IN LISTS _hifw_registered_names)
            set(_hifw_registered_target "__hifw_component_${_hifw_registered_name}")
            get_target_property(_hifw_registered_type
                "${_hifw_registered_target}" TYPE)
            if(_hifw_registered_type STREQUAL "INTERFACE_LIBRARY")
                target_link_libraries("${_hifw_registered_target}" INTERFACE
                    __hifw_project_config)
            else()
                target_link_libraries("${_hifw_registered_target}" PRIVATE
                    __hifw_project_config)
            endif()
        endforeach()
    endif()

    add_executable("${_project_name}")
    target_link_libraries("${_project_name}" PRIVATE __hifw_component_main)
    set_target_properties("${_project_name}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${HIFW_OUTPUT_DIR}")

    install(TARGETS "${_project_name}" RUNTIME DESTINATION bin)

    set(_hifw_available_components ${_hifw_component_names})
    list(JOIN _hifw_available_components ", " _hifw_available_summary)
    list(JOIN _hifw_selected_components ", " _hifw_selected_summary)
    message(STATUS "[hifw] project: ${_project_name}")
    message(STATUS "[hifw] board: ${HIFW_BOARD}")
    if(HIFW_CHIP)
        message(STATUS "[hifw] chip: ${HIFW_CHIP}")
    endif()
    message(STATUS "[hifw] available components: ${_hifw_available_summary}")
    message(STATUS "[hifw] selected components: ${_hifw_selected_summary}")
    message(STATUS "[hifw] output: ${HIFW_OUTPUT_DIR}/${_project_name}")
endmacro()
