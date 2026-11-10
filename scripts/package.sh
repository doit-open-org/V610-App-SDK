#!/usr/bin/env bash
# Build the distributable SDK archive: the framework, its components and
# examples together with the platform files of the current image build.
# The compiler is not part of it; developers install that once with
# install.sh.
#
#   scripts/package.sh [output directory]
#
# Produces <output>/V610-App-SDK-<version>.tar.gz.
set -euo pipefail

SDK_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT_DIR=${1:-${SDK_DIR}/dist}
VERSION=$(cat "${SDK_DIR}/VERSION")
NAME="V610-App-SDK-${VERSION}"

if [[ ! -f "${SDK_DIR}/platform/hi3516cv610/manifest.json" ]]; then
    echo "error: no platform files in ${SDK_DIR}/platform; run scripts/install-platform.sh first" >&2
    exit 1
fi

mkdir -p "${OUT_DIR}"
tar -czf "${OUT_DIR}/${NAME}.tar.gz.tmp" -C "${SDK_DIR}" \
    --transform "flags=rh;s,^\.,${NAME}," \
    --exclude=./dist --exclude=.git --exclude='./.platform.*' \
    --exclude='__pycache__' \
    --exclude='./examples/*/build' --exclude='./examples/*/out' \
    --exclude='./examples/*/.config' --exclude='./examples/*/.config.old' \
    .
mv "${OUT_DIR}/${NAME}.tar.gz.tmp" "${OUT_DIR}/${NAME}.tar.gz"
ls -l "${OUT_DIR}/${NAME}.tar.gz"
