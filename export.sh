#!/usr/bin/env bash
# Source from Bash or zsh. The compiler is independent of the platform files.
if [ -n "${BASH_VERSION:-}" ]; then
    if [ "${BASH_SOURCE[0]}" = "$0" ]; then
        echo 'ERROR: use source export.sh' >&2
        exit 1
    fi
    _v610_export_file="${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION:-}" ]; then
    case "${ZSH_EVAL_CONTEXT:-}" in
        *:file) ;;
        *) echo 'ERROR: use source export.sh' >&2; exit 1 ;;
    esac
    _v610_export_file="${(%):-%x}"
else
    echo 'ERROR: source export.sh from Bash or zsh' >&2
    return 1 2>/dev/null || exit 1
fi

_v610_setup() {
    local sdk platform compiler_dir compiler installed required solution solution_root
    sdk=$(cd "$(dirname "$_v610_export_file")" && pwd) || return 1
    . "$sdk/compiler.conf"
    platform=${APP_SDK_PLATFORM:-}
    solution=${HI_SDK_ROOT:-}
    if [ -z "$solution" ] && [ "$(basename "$(dirname "$(dirname "$sdk")")")" = apps ]; then
        solution="$(dirname "$(dirname "$(dirname "$sdk")")")/_build/sdk/Hi3516CV610"
        [ -d "$solution/soc/include" ] || solution=
    fi
    if [ -z "$platform" ]; then
        if [ -n "$solution" ]; then platform=solution; else platform=buildroot; fi
    fi
    case "$platform" in
        buildroot)
            local platform_path
            platform_path=${HIFW_PLATFORM_PATH:-$sdk/platform/hi3516cv610}
            if [ ! -f "$platform_path/manifest.json" ]; then
                echo "[hifw] ERROR: platform missing: $platform_path" >&2
                echo "  run scripts/install-platform.sh <Buildroot SDK archive or output/host>" >&2
                return 1
            fi
            platform_path=$(cd "$platform_path" && pwd -P) || return 1
            ;;
        solution)
            if [ ! -d "$solution/soc/include" ]; then
                echo '[hifw] ERROR: solution mode needs a built HI_SDK_ROOT' >&2
                return 1
            fi
            ;;
        *) echo '[hifw] ERROR: APP_SDK_PLATFORM must be buildroot or solution' >&2; return 1 ;;
    esac
    # hi_aiot_solution root (<root>/_build/sdk/Hi3516CV610): its install.sh
    # puts the compiler in toolchain/ and CANN in CANN/.
    solution_root=
    if [ -n "$solution" ] && [ -d "$solution/../../../toolchain" ]; then
        solution_root=$(cd "$solution/../../.." && pwd)
    fi

    compiler_dir=
    if [ "$platform" = solution ]; then
        # Respect the solution shell's compiler before consulting standalone
        # installations. Export the choice so CMake uses the same compiler.
        if [ -z "${CROSS_PREFIX:-}" ]; then
            # The solution shell's CV610_TOOLCHAIN_PATH, then the toolchain/ of
            # the tree: <name>/bin of the current or the older openEuler build.
            local prefix toolchain_path
            for toolchain_path in "${CV610_TOOLCHAIN_PATH:-}" \
                "${solution_root:+$solution_root/toolchain/$COMPILER_NAME/bin}" \
                "${solution_root:+$solution_root/toolchain/openeuler_gcc_arm32le-musl/bin}"; do
                [ -n "$toolchain_path" ] || continue
                for prefix in "$COMPILER_PREFIX" arm-openeuler-linux-musleabi-; do
                    if [ -x "$toolchain_path/${prefix}gcc" ]; then
                        export CROSS_PREFIX="$prefix"
                        compiler_dir=$(cd "$toolchain_path" && pwd -P)
                        break 2
                    fi
                done
            done
            for prefix in "$COMPILER_PREFIX" arm-openeuler-linux-musleabi-; do
                [ -z "${CROSS_PREFIX:-}" ] || break
                if command -v "${prefix}gcc" >/dev/null 2>&1; then
                    export CROSS_PREFIX="$prefix"
                    break
                fi
            done
        fi
    fi
    # For solution mode a compiler already on PATH wins; otherwise use the
    # independent installation or the original image compiler.
    for compiler in \
        "${HIFW_COMPILER_ROOT:-${HOME}/.v610/compiler}/${COMPILER_PACKAGE}/${COMPILER_NAME}/bin" \
        "$sdk/../output/host/opt/ext-toolchain/${COMPILER_NAME}/bin"; do
        if [ "$platform" = solution ] && { [ -n "$compiler_dir" ] || command -v "${CROSS_PREFIX:-$COMPILER_PREFIX}gcc" >/dev/null 2>&1; }; then
            break
        fi
        if [ -x "$compiler/${CROSS_PREFIX:-$COMPILER_PREFIX}gcc" ]; then
            compiler_dir=$(cd "$compiler" && pwd -P)
            break
        fi
    done
    compiler=${CROSS_PREFIX:-$COMPILER_PREFIX}gcc
    if [ -n "$compiler_dir" ]; then compiler="$compiler_dir/$compiler"; fi
    if ! command -v "$compiler" >/dev/null 2>&1; then
        echo "[hifw] ERROR: compiler missing; run bash $sdk/install.sh" >&2
        return 1
    fi
    if [ "$platform" = buildroot ]; then
        required=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["compiler"]["description"])' "$platform_path/manifest.json") || return 1
        installed=$("$compiler" --version | head -n 1) || return 1
        if [ "$required" != "$installed" ]; then
            echo "[hifw] ERROR: compiler version mismatch" >&2
            echo "  platform: $required" >&2
            echo "  installed: $installed" >&2
            return 1
        fi
    fi
    [ -z "$compiler_dir" ] || export PATH="$compiler_dir:$PATH"
    export APP_SDK_PATH="$sdk" HIFW_PATH="$sdk" APP_SDK_PLATFORM="$platform"
    export PATH="$sdk:$PATH"
    unset HIFW_TOOLCHAIN_PATH
    if [ "$platform" = buildroot ]; then
        export HIFW_PLATFORM_PATH="$platform_path"
        unset HI_SDK_ROOT
        echo "[hifw] platform files: $HIFW_PLATFORM_PATH"
    else
        export HI_SDK_ROOT="$solution"
        unset HIFW_PLATFORM_PATH
        echo "[hifw] vendor SDK: $HI_SDK_ROOT"
    fi
    echo "[hifw] APP_SDK_PATH=$APP_SDK_PATH"
    echo "[hifw] compiler: $(command -v "${CROSS_PREFIX:-$COMPILER_PREFIX}gcc")"
    # CANN is optional and only needed to convert models. Its environment
    # changes LD_LIBRARY_PATH, so it is not loaded here: model scripts read
    # CANN_ENV, and "source \"$CANN_ENV\"" puts atc on PATH.
    # Prefer an explicit location, then the environment of a hi_aiot_solution
    # shell or tree, then the standalone installation.
    local cann_env
    for cann_env in "${HIFW_CANN_ROOT:+$HIFW_CANN_ROOT/export_act.sh}" "${CANN_ENV:-}" \
        "${solution_root:+$solution_root/CANN/export_act.sh}" "$HOME/.v610/cann/export_act.sh"; do
        if [ -n "$cann_env" ] && [ -f "$cann_env" ]; then
            export CANN_ENV="$cann_env"
            echo "[hifw] CANN_ENV=$CANN_ENV"
            break
        fi
    done
}
_v610_setup
_v610_export_status=$?
unset -f _v610_setup
unset _v610_export_file
unset COMPILER_PACKAGE COMPILER_NAME COMPILER_PREFIX COMPILER_URL COMPILER_SHA256
if [ "$_v610_export_status" -ne 0 ]; then
    unset _v610_export_status
    return 1
fi
unset _v610_export_status
