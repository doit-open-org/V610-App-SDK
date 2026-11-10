#!/usr/bin/env bash
# Rebuild prebuilt/, the OpenCV libraries shipped for the SDK's own compiler.
#
#   source <sdk>/export.sh
#   components/opencv/build.sh
#
# Run it after changing the source archive, the module list or the build
# options in opencv.cmake. Applications never need it: with another
# compiler OpenCV is built from source automatically.
set -euo pipefail

COMPONENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if [[ -z "${APP_SDK_PATH:-}" ]]; then
    echo "error: source the SDK's export.sh first" >&2
    exit 1
fi
if [[ -n "${HI_SDK_ROOT:-}" ]]; then
    toolchain="${APP_SDK_PATH}/cmake/toolchains/vendor_sdk.cmake"
else
    toolchain="${APP_SDK_PATH}/cmake/toolchains/hi3516cv610.cmake"
fi

work=$(mktemp -d)
trap 'rm -rf "${work}"' EXIT
cat > "${work}/CMakeLists.txt" <<PROJECT
cmake_minimum_required(VERSION 3.16)
project(opencv_prebuilt LANGUAGES C CXX)
include("${COMPONENT_DIR}/opencv.cmake")
set(OPENCV_FROM_SOURCE ON)
set(HIFW_CACHE_DIR "${work}/cache")
hifw_opencv_provide(_dir)
file(REMOVE_RECURSE "${COMPONENT_DIR}/prebuilt")
file(COPY "\${_dir}/" DESTINATION "${COMPONENT_DIR}/prebuilt")
file(REMOVE_RECURSE "${COMPONENT_DIR}/prebuilt/bin" "${COMPONENT_DIR}/prebuilt/share")
PROJECT
cmake -S "${work}" -B "${work}/build" -DCMAKE_TOOLCHAIN_FILE="${toolchain}" >/dev/null

echo "OpenCV $(cat "${COMPONENT_DIR}/prebuilt/VERSION") for $(cat "${COMPONENT_DIR}/prebuilt/COMPILER")"
du -sh "${COMPONENT_DIR}/prebuilt" | cut -f1
