#!/usr/bin/env bash
# Import only platform development files, without a compiler or host tools.
set -euo pipefail
SDK_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
exec python3 "${SDK_DIR}/scripts/install-platform.py" "$@"
