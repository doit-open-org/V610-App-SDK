include_guard(GLOBAL)

include(CMakeParseArguments)

function(hifw_component_register)
    set(_multi_value_args
        SRCS
        INCLUDE_DIRS
        PRIV_INCLUDE_DIRS
        REQUIRES
        PRIV_REQUIRES
        LINK_LIBRARIES
        PRIV_LINK_LIBRARIES
        COMPILE_DEFINITIONS
        PRIV_COMPILE_DEFINITIONS
        PUBLIC_COMPILE_OPTIONS
        COMPILE_OPTIONS
        PUBLIC_LINK_OPTIONS
        PRIV_LINK_OPTIONS
        LINK_OPTIONS
        PKG_CONFIG
        PRIV_PKG_CONFIG)
    cmake_parse_arguments(_COMPONENT "" "" "${_multi_value_args}" ${ARGN})
    if(_COMPONENT_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unknown hifw_component_register arguments: ${_COMPONENT_UNPARSED_ARGUMENTS}")
    endif()

    get_filename_component(_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
    if(NOT _name MATCHES "^[A-Za-z0-9_]+$")
        message(FATAL_ERROR
            "Invalid component directory name '${_name}': use letters, numbers and underscores")
    endif()

    set(_target "__hifw_component_${_name}")
    if(TARGET "${_target}")
        message(FATAL_ERROR "Duplicate component name: ${_name}")
    endif()

    # Libraries of the board image, found in the toolchain sysroot through
    # pkg-config. They become imported targets linked like any other
    # library, so their include directories and flags follow the dependency.
    foreach(_scope IN ITEMS "" PRIV_)
        foreach(_module IN LISTS _COMPONENT_${_scope}PKG_CONFIG)
            if(TARGET platform::common AND _module MATCHES "^hisi-(mpp|mpp-media|svp)$")
                if(_module STREQUAL "hisi-mpp")
                    list(APPEND _COMPONENT_${_scope}LINK_LIBRARIES platform::common)
                elseif(_module STREQUAL "hisi-mpp-media")
                    list(APPEND _COMPONENT_${_scope}LINK_LIBRARIES platform::isp)
                else()
                    list(APPEND _COMPONENT_${_scope}LINK_LIBRARIES platform::svp)
                endif()
                continue()
            endif()
            string(MAKE_C_IDENTIFIER "${_module}" _module_id)
            if(NOT TARGET "PkgConfig::HIFW_${_module_id}")
                if(COMMAND hifw_import_system_package)
                    hifw_import_system_package("${_module}" "PkgConfig::HIFW_${_module_id}")
                else()
                    find_package(PkgConfig REQUIRED)
                    pkg_check_modules("HIFW_${_module_id}" IMPORTED_TARGET GLOBAL "${_module}")
                endif()
                if(NOT TARGET "PkgConfig::HIFW_${_module_id}")
                    message(FATAL_ERROR
                        "Component '${_name}' needs '${_module}', which pkg-config "
                        "cannot find. With the SDK's platform files it has to be "
                        "enabled in the board image (a Buildroot package) and the "
                        "SDK released again; with HI_SDK_ROOT, provide the library "
                        "and add its .pc directory to PKG_CONFIG_PATH.")
                endif()
            endif()
            list(APPEND _COMPONENT_${_scope}LINK_LIBRARIES "PkgConfig::HIFW_${_module_id}")
        endforeach()
    endforeach()

    if(_COMPONENT_SRCS)
        if(_name STREQUAL "main")
            add_library("${_target}" OBJECT ${_COMPONENT_SRCS})
        else()
            add_library("${_target}" STATIC ${_COMPONENT_SRCS})
        endif()
        set(_public_scope PUBLIC)
        set(_private_scope PRIVATE)
    else()
        if(_COMPONENT_PRIV_INCLUDE_DIRS OR _COMPONENT_PRIV_REQUIRES
           OR _COMPONENT_PRIV_LINK_LIBRARIES
           OR _COMPONENT_PRIV_COMPILE_DEFINITIONS
           OR _COMPONENT_COMPILE_OPTIONS OR _COMPONENT_LINK_OPTIONS
           OR _COMPONENT_PRIV_LINK_OPTIONS)
            message(FATAL_ERROR
                "Header-only component '${_name}' cannot have private properties")
        endif()
        add_library("${_target}" INTERFACE)
        set(_public_scope INTERFACE)
        set(_private_scope INTERFACE)
    endif()

    add_library("hifw::${_name}" ALIAS "${_target}")
    set(HIFW_COMPONENT_LIB "${_target}" PARENT_SCOPE)

    if(_COMPONENT_INCLUDE_DIRS)
        target_include_directories("${_target}" ${_public_scope}
            ${_COMPONENT_INCLUDE_DIRS})
    endif()
    if(_COMPONENT_PRIV_INCLUDE_DIRS)
        target_include_directories("${_target}" ${_private_scope}
            ${_COMPONENT_PRIV_INCLUDE_DIRS})
    endif()
    if(_COMPONENT_COMPILE_DEFINITIONS)
        target_compile_definitions("${_target}" ${_public_scope}
            ${_COMPONENT_COMPILE_DEFINITIONS})
    endif()
    if(_COMPONENT_PRIV_COMPILE_DEFINITIONS)
        target_compile_definitions("${_target}" ${_private_scope}
            ${_COMPONENT_PRIV_COMPILE_DEFINITIONS})
    endif()
    if(_COMPONENT_COMPILE_OPTIONS)
        target_compile_options("${_target}" ${_private_scope}
            ${_COMPONENT_COMPILE_OPTIONS})
    endif()
    if(_COMPONENT_PUBLIC_COMPILE_OPTIONS)
        target_compile_options("${_target}" ${_public_scope}
            ${_COMPONENT_PUBLIC_COMPILE_OPTIONS})
    endif()
    if(_COMPONENT_LINK_OPTIONS)
        target_link_options("${_target}" ${_private_scope}
            ${_COMPONENT_LINK_OPTIONS})
    endif()
    if(_COMPONENT_PUBLIC_LINK_OPTIONS)
        target_link_options("${_target}" ${_public_scope}
            ${_COMPONENT_PUBLIC_LINK_OPTIONS})
    endif()
    if(_COMPONENT_PRIV_LINK_OPTIONS)
        target_link_options("${_target}" ${_private_scope}
            ${_COMPONENT_PRIV_LINK_OPTIONS})
    endif()
    if(_COMPONENT_LINK_LIBRARIES)
        target_link_libraries("${_target}" ${_public_scope}
            ${_COMPONENT_LINK_LIBRARIES})
    endif()
    if(_COMPONENT_PRIV_LINK_LIBRARIES)
        target_link_libraries("${_target}" ${_private_scope}
            ${_COMPONENT_PRIV_LINK_LIBRARIES})
    endif()

    set_property(TARGET "${_target}" PROPERTY
        HIFW_PUBLIC_REQUIREMENTS "${_COMPONENT_REQUIRES}")
    set_property(TARGET "${_target}" PROPERTY
        HIFW_PRIVATE_REQUIREMENTS "${_COMPONENT_PRIV_REQUIRES}")
    set_property(GLOBAL APPEND PROPERTY HIFW_COMPONENT_NAMES "${_name}")
endfunction()

function(_hifw_check_component_cycle _name _path)
    set(_target "__hifw_component_${_name}")
    get_property(_state TARGET "${_target}" PROPERTY HIFW_VISIT_STATE)
    if(_state STREQUAL "visiting")
        list(APPEND _path "${_name}")
        list(JOIN _path " -> " _cycle)
        message(FATAL_ERROR "Component dependency cycle: ${_cycle}")
    endif()
    if(_state STREQUAL "visited")
        return()
    endif()

    set_property(TARGET "${_target}" PROPERTY HIFW_VISIT_STATE "visiting")
    list(APPEND _path "${_name}")
    get_target_property(_public_requirements "${_target}"
        HIFW_PUBLIC_REQUIREMENTS)
    get_target_property(_private_requirements "${_target}"
        HIFW_PRIVATE_REQUIREMENTS)
    foreach(_requirement IN LISTS
            _public_requirements _private_requirements)
        if(TARGET "__hifw_component_${_requirement}")
            _hifw_check_component_cycle("${_requirement}" "${_path}")
        endif()
    endforeach()
    set_property(TARGET "${_target}" PROPERTY HIFW_VISIT_STATE "visited")
endfunction()

function(hifw_finalize_components)
    get_property(_names GLOBAL PROPERTY HIFW_COMPONENT_NAMES)
    foreach(_name IN LISTS _names)
        set_property(TARGET "__hifw_component_${_name}"
            PROPERTY HIFW_VISIT_STATE "")
    endforeach()
    foreach(_name IN LISTS _names)
        _hifw_check_component_cycle("${_name}" "")
    endforeach()

    foreach(_name IN LISTS _names)
        set(_target "__hifw_component_${_name}")
        get_target_property(_type "${_target}" TYPE)
        if(_type STREQUAL "INTERFACE_LIBRARY")
            set(_public_scope INTERFACE)
            set(_private_scope INTERFACE)
        else()
            set(_public_scope PUBLIC)
            set(_private_scope PRIVATE)
        endif()

        get_target_property(_public_requirements "${_target}"
            HIFW_PUBLIC_REQUIREMENTS)
        get_target_property(_private_requirements "${_target}"
            HIFW_PRIVATE_REQUIREMENTS)

        foreach(_requirement IN LISTS _public_requirements)
            if(NOT TARGET "__hifw_component_${_requirement}")
                message(FATAL_ERROR
                    "Component '${_name}' requires missing component '${_requirement}'")
            endif()
            target_link_libraries("${_target}" ${_public_scope}
                "__hifw_component_${_requirement}")
        endforeach()
        foreach(_requirement IN LISTS _private_requirements)
            if(NOT TARGET "__hifw_component_${_requirement}")
                message(FATAL_ERROR
                    "Component '${_name}' privately requires missing component '${_requirement}'")
            endif()
            target_link_libraries("${_target}" ${_private_scope}
                "__hifw_component_${_requirement}")
        endforeach()
    endforeach()
endfunction()
