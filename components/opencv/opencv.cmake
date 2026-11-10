# Build OpenCV from source for the current toolchain and hand back the
# installed package.
#
#   include("<sdk>/components/opencv/opencv.cmake")
#   hifw_opencv_provide(<result variable>)
#   find_package(OpenCV REQUIRED PATHS "${<result>}/lib/cmake/opencv4"
#       NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
#
# prebuilt/ next to this file holds the libraries compiled with the SDK's
# own compiler and is used as it is when the current compiler is that one.
# With any other compiler, module selection or source archive, the source
# archive is compiled into static libraries the first time they are needed
# and kept in a per-user cache, so the cost is paid once, not per project.
# Plain CMake projects can use this file as well as HiFW components.
#
#   OPENCV_MODULES          modules to build, comma separated; their
#                           dependencies are added automatically
#   OPENCV_SOURCE_ARCHIVE   another OpenCV source archive, e.g. a complete
#                           one when modules outside the shipped set are
#                           wanted (CMake variable or environment)
#   HIFW_CACHE_DIR          cache location (CMake variable or environment),
#                           default ~/.v610/cache
#   OPENCV_FROM_SOURCE      ON to ignore prebuilt/ (build.sh uses it)
include_guard(GLOBAL)

set(_HIFW_OPENCV_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(_HIFW_OPENCV_VERSION "4.13.0")
set(_HIFW_OPENCV_DEFAULT_MODULES "core,imgproc,imgcodecs")

# What tells one compiler from another for the purpose of linking C++
# static libraries: target and version.
function(_hifw_opencv_compiler_id out_var)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE _machine OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpfullversion
        OUTPUT_VARIABLE _version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    set(${out_var} "${_machine} ${_version}" PARENT_SCOPE)
endfunction()

function(hifw_opencv_provide out_var)
    foreach(_var IN ITEMS OPENCV_SOURCE_ARCHIVE HIFW_CACHE_DIR)
        if(NOT ${_var} AND DEFINED ENV{${_var}} AND NOT "$ENV{${_var}}" STREQUAL "")
            set(${_var} "$ENV{${_var}}")
        endif()
    endforeach()
    set(_stock TRUE)
    if(OPENCV_SOURCE_ARCHIVE OR OPENCV_FROM_SOURCE)
        set(_stock FALSE)
    endif()
    if(NOT OPENCV_SOURCE_ARCHIVE)
        set(OPENCV_SOURCE_ARCHIVE
            "${_HIFW_OPENCV_DIR}/opencv-${_HIFW_OPENCV_VERSION}-reduced.tar.xz")
    endif()
    if(NOT EXISTS "${OPENCV_SOURCE_ARCHIVE}")
        message(FATAL_ERROR "OpenCV source archive not found: ${OPENCV_SOURCE_ARCHIVE}")
    endif()
    if(NOT OPENCV_MODULES)
        set(OPENCV_MODULES "${_HIFW_OPENCV_DEFAULT_MODULES}")
    endif()

    # The shipped libraries, when they were built with this very compiler.
    _hifw_opencv_compiler_id(_compiler_id)
    set(_prebuilt "${_HIFW_OPENCV_DIR}/prebuilt")
    if(_stock AND OPENCV_MODULES STREQUAL _HIFW_OPENCV_DEFAULT_MODULES AND
       EXISTS "${_prebuilt}/COMPILER" AND
       EXISTS "${_prebuilt}/lib/cmake/opencv4/OpenCVConfig.cmake")
        file(STRINGS "${_prebuilt}/COMPILER" _prebuilt_compiler LIMIT_COUNT 1)
        if(_prebuilt_compiler STREQUAL _compiler_id)
            set(${out_var} "${_prebuilt}" PARENT_SCOPE)
            return()
        endif()
        message(STATUS
            "OpenCV: prebuilt libraries are for '${_prebuilt_compiler}', "
            "this compiler is '${_compiler_id}'; building from source")
    endif()
    if(NOT HIFW_CACHE_DIR)
        set(HIFW_CACHE_DIR "$ENV{HOME}/.v610/cache")
    endif()

    # One build per source, module selection, compiler and target flags.
    file(SHA256 "${OPENCV_SOURCE_ARCHIVE}" _archive_hash)
    file(SHA256 "${_HIFW_OPENCV_DIR}/opencv.cmake" _recipe_hash)
    string(SHA256 _key
        "${_archive_hash};${_recipe_hash};${OPENCV_MODULES};${_compiler_id};${CMAKE_C_FLAGS_INIT};${CMAKE_C_FLAGS}")
    string(SUBSTRING "${_key}" 0 16 _key)
    set(_root "${HIFW_CACHE_DIR}/opencv-${_key}")
    set(_install "${_root}/install")

    if(NOT EXISTS "${_root}/.done")
        message(STATUS
            "Building OpenCV (${OPENCV_MODULES}) for this compiler, once: ${_root}")
        file(REMOVE_RECURSE "${_root}")
        file(MAKE_DIRECTORY "${_root}/src")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xf "${OPENCV_SOURCE_ARCHIVE}"
            WORKING_DIRECTORY "${_root}/src"
            RESULT_VARIABLE _result)
        file(GLOB _source "${_root}/src/*/CMakeLists.txt")
        if(_result OR NOT _source)
            message(FATAL_ERROR "failed to unpack ${OPENCV_SOURCE_ARCHIVE}")
        endif()
        list(GET _source 0 _source)
        get_filename_component(_source "${_source}" DIRECTORY)

        separate_arguments(_flags NATIVE_COMMAND "${CMAKE_CXX_FLAGS}")
        # The toolchain file and whatever it was told on the command line.
        set(_toolchain_args)
        foreach(_var IN ITEMS CMAKE_TOOLCHAIN_FILE HIFW_PLATFORM_PATH HI_SDK_ROOT CROSS_PREFIX)
            if(${_var})
                list(APPEND _toolchain_args "-D${_var}=${${_var}}")
            endif()
        endforeach()
        if(NOT CMAKE_TOOLCHAIN_FILE)
            list(APPEND _toolchain_args
                "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
                "-DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
                "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
                "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
        endif()

        # OpenCV is built with NEON. A project whose own flags leave the
        # FPU unused (the compiler's default is soft float) still links
        # with it: softfp keeps the soft-float calling convention.
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E echo ""
            COMMAND "${CMAKE_CXX_COMPILER}" ${_flags} -dM -E -x c++ -
            OUTPUT_VARIABLE _macros ERROR_QUIET)
        if(NOT _macros MATCHES "__ARM_NEON")
            set(_fpu "-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4")
            list(APPEND _toolchain_args
                "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} ${_fpu}"
                "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} ${_fpu}")
        endif()

        # Static, position independent, with its own zlib, libjpeg and
        # libpng, so that the result depends on nothing but the C and C++
        # runtime of the board.
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -S "${_source}" -B "${_root}/build"
                ${_toolchain_args}
                -DCMAKE_BUILD_TYPE=Release
                "-DCMAKE_INSTALL_PREFIX=${_install}"
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -DBUILD_SHARED_LIBS=OFF
                "-DBUILD_LIST=${OPENCV_MODULES}"
                -DBUILD_ZLIB=ON -DBUILD_JPEG=ON -DBUILD_PNG=ON
                -DWITH_JPEG=ON -DWITH_PNG=ON
                -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF
                -DWITH_OPENEXR=OFF -DWITH_AVIF=OFF -DWITH_JPEGXL=OFF -DWITH_GIF=OFF
                -DWITH_IMGCODEC_HDR=OFF -DWITH_IMGCODEC_SUNRASTER=OFF
                -DWITH_IMGCODEC_PXM=OFF -DWITH_IMGCODEC_PFM=OFF
                -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_V4L=OFF -DWITH_GTK=OFF
                -DWITH_QT=OFF -DWITH_OPENCL=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF
                -DWITH_TBB=OFF -DWITH_OPENMP=OFF -DWITH_EIGEN=OFF -DWITH_LAPACK=OFF
                -DWITH_PROTOBUF=OFF -DWITH_FLATBUFFERS=OFF -DWITH_1394=OFF
                -DWITH_VTK=OFF -DWITH_OBSENSOR=OFF -DWITH_ADE=OFF
                -DENABLE_NEON=ON -DENABLE_VFPV3=ON
                -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF
                -DBUILD_DOCS=OFF -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF
                -DBUILD_opencv_python2=OFF -DBUILD_opencv_python3=OFF
                -DOPENCV_GENERATE_PKGCONFIG=OFF -DINSTALL_C_EXAMPLES=OFF
            RESULT_VARIABLE _result
            OUTPUT_FILE "${_root}/configure.log" ERROR_FILE "${_root}/configure.log")
        if(_result)
            message(FATAL_ERROR "OpenCV configuration failed, see ${_root}/configure.log")
        endif()
        include(ProcessorCount)
        ProcessorCount(_jobs)
        if(_jobs EQUAL 0)
            set(_jobs 1)
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --build "${_root}/build" -j ${_jobs}
            RESULT_VARIABLE _result
            OUTPUT_FILE "${_root}/build.log" ERROR_FILE "${_root}/build.log")
        if(_result)
            message(FATAL_ERROR "OpenCV build failed, see ${_root}/build.log")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" --install "${_root}/build" --strip
            RESULT_VARIABLE _result OUTPUT_QUIET ERROR_QUIET)
        if(_result OR NOT EXISTS "${_install}/lib/cmake/opencv4/OpenCVConfig.cmake")
            message(FATAL_ERROR "OpenCV installation failed in ${_root}")
        endif()
        # Only the installed package is kept.
        file(REMOVE_RECURSE "${_root}/src" "${_root}/build")
        file(WRITE "${_install}/COMPILER" "${_compiler_id}\n")
        file(WRITE "${_install}/VERSION" "${_HIFW_OPENCV_VERSION} (${OPENCV_MODULES})\n")
        file(WRITE "${_root}/.done" "")
    endif()
    set(${out_var} "${_install}" PARENT_SCOPE)
endfunction()
