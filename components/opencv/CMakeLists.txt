# OpenCV as static libraries built from the source archive next to this
# file (see opencv.cmake).
#
# The application links what it uses and carries it along: nothing has to
# be installed on the board, and the board image does not need OpenCV.
include("${CMAKE_CURRENT_LIST_DIR}/opencv.cmake")

# CONFIG_OPENCV_MODULES selects the modules.
if(HIFW_CONFIG_HEADER AND EXISTS "${HIFW_CONFIG_HEADER}")
    file(STRINGS "${HIFW_CONFIG_HEADER}" _hifw_opencv_lines
        REGEX "^#define[ \t]+CONFIG_OPENCV_MODULES[ \t]+\"")
    if(_hifw_opencv_lines)
        list(GET _hifw_opencv_lines 0 _hifw_opencv_line)
        string(REGEX REPLACE "^#define[ \t]+CONFIG_OPENCV_MODULES[ \t]+\"(.*)\"[ \t]*$"
            "\\1" OPENCV_MODULES "${_hifw_opencv_line}")
        string(REPLACE " " "" OPENCV_MODULES "${OPENCV_MODULES}")
    endif()
endif()

hifw_opencv_provide(_hifw_opencv_dir)

# The package is outside the sysroot, where packages are otherwise looked up.
find_package(OpenCV REQUIRED
    PATHS "${_hifw_opencv_dir}/lib/cmake/opencv4"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

# Unused code of the static libraries is dropped at link time.
hifw_component_register(
    LINK_LIBRARIES ${OpenCV_LIBS}
    PUBLIC_LINK_OPTIONS -Wl,--gc-sections)
