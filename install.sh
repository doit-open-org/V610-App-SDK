#!/usr/bin/env bash
# 安装交叉编译器和 CANN 模型转换工具。每台开发机只需执行一次；之后更新 SDK 不必重装。
#
#   bash install.sh [--compiler-dir <目录>] [--archive <文件>]
#                   [--cann-dir <目录>] [--cann-archive <文件>] [--skip-cann]

set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# shellcheck source=compiler.conf
. "${SCRIPT_DIR}/compiler.conf"

INSTALL_ROOT=${HIFW_COMPILER_ROOT:-"${HOME}/.v610/compiler"}
ARCHIVE=
TEMP_DIR=
# CANN 来自海思的 SVP 主机工具包，其中的 atc 用于把模型转换为 NPU 格式。
CANN_URL=https://gitcode.com/HiSpark/Hi3516CV610/releases/download/Tools_V1.0.1/SVP_PC.tar.gz
CANN_SHA256=9cf9ddd17193c21fd94e7cddb158942fd32a4f45d388235b3d8ab0b09b49332a
CANN_DIR=${HIFW_CANN_ROOT:-"${HOME}/.v610/cann"}
CANN_ARCHIVE=
SKIP_CANN=

usage()
{
    cat <<USAGE
用法: bash install.sh [--compiler-dir <目录>] [--archive <文件>]
                     [--cann-dir <目录>] [--cann-archive <文件>] [--skip-cann]

下载并安装 Cove-610 应用开发所需的交叉编译器 ${COMPILER_PACKAGE}，
以及模型转换用的 CANN（atc）。

  --compiler-dir <目录>  编译器安装位置，默认 ${INSTALL_ROOT}
                         （也可用环境变量 HIFW_COMPILER_ROOT 指定）
  --archive <文件>       使用已下载的编译器压缩包，不联网
  --cann-dir <目录>      CANN 安装位置，默认 ${CANN_DIR}
                         （也可用环境变量 HIFW_CANN_ROOT 指定）
  --cann-archive <文件>  使用已下载的 SVP_PC.tar.gz，不联网
  --skip-cann            不安装 CANN。只编译应用、不转换模型时不需要 CANN

编译器和 CANN 都装在 SDK 目录之外，多个 SDK 版本共用同一份；已安装时直接跳过。
安装到非默认位置时，使用 SDK 前需要设置同一个 HIFW_COMPILER_ROOT / HIFW_CANN_ROOT。
USAGE
}

cleanup()
{
    if [[ -n $TEMP_DIR && -d $TEMP_DIR ]]; then
        chmod -R u+w -- "$TEMP_DIR" 2>/dev/null || true
        rm -rf -- "$TEMP_DIR"
    fi
}
trap cleanup EXIT

while (($# > 0)); do
    case $1 in
        --compiler-dir)
            (($# >= 2)) || { printf '错误: --compiler-dir 缺少目录参数\n' >&2; exit 1; }
            INSTALL_ROOT=$2
            shift 2
            ;;
        --archive)
            (($# >= 2)) || { printf '错误: --archive 缺少文件参数\n' >&2; exit 1; }
            ARCHIVE=$2
            shift 2
            ;;
        --cann-dir)
            (($# >= 2)) || { printf '错误: --cann-dir 缺少目录参数\n' >&2; exit 1; }
            CANN_DIR=$2
            shift 2
            ;;
        --cann-archive)
            (($# >= 2)) || { printf '错误: --cann-archive 缺少文件参数\n' >&2; exit 1; }
            CANN_ARCHIVE=$2
            shift 2
            ;;
        --skip-cann)
            SKIP_CANN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf '错误: 未知参数: %s\n' "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

case $INSTALL_ROOT in
    "~") INSTALL_ROOT=$HOME ;;
    "~/"*) INSTALL_ROOT="${HOME}/${INSTALL_ROOT#\~/}" ;;
    /*) ;;
    *) INSTALL_ROOT="$(pwd -P)/${INSTALL_ROOT}" ;;
esac
case $CANN_DIR in
    "~") CANN_DIR=$HOME ;;
    "~/"*) CANN_DIR="${HOME}/${CANN_DIR#\~/}" ;;
    /*) ;;
    *) CANN_DIR="$(pwd -P)/${CANN_DIR}" ;;
esac

# 应用开发用到的主机工具；只检查，不代为安装。
MISSING=()
for tool in python3 cmake make tar sha256sum pkg-config; do
    command -v "$tool" >/dev/null 2>&1 || MISSING+=("$tool")
done
if ((${#MISSING[@]} > 0)); then
    printf '错误: 缺少主机工具: %s\n' "${MISSING[*]}" >&2
    printf '  Debian/Ubuntu: sudo apt-get install python3 cmake make tar coreutils pkg-config\n' >&2
    exit 1
fi

# 在 hi_aiot_solution 工程中（<工程>/apps/cove_610/V610-App-SDK），编译器和 CANN 由工程的
# install.sh 安装，并生成工程根目录的 export.sh；本 SDK 的 export.sh 会从工程中查找它们。
SOLUTION_ROOT=$(dirname -- "$(dirname -- "$(dirname -- "$SCRIPT_DIR")")")
if [[ $(basename -- "$(dirname -- "$(dirname -- "$SCRIPT_DIR")")") == apps && -f $SOLUTION_ROOT/export.sh ]]; then
    printf '编译器和 CANN 由 hi_aiot_solution 提供: %s/export.sh\n' "$SOLUTION_ROOT"
    printf '\n接下来: source %s/export.sh\n' "$SCRIPT_DIR"
    exit 0
fi

INSTALL_DIR="${INSTALL_ROOT}/${COMPILER_PACKAGE}"
COMPILER_PATH="${INSTALL_DIR}/${COMPILER_NAME}/bin/${COMPILER_PREFIX}gcc"

if [[ -x $COMPILER_PATH ]]; then
    printf '编译器已安装: %s\n' "$INSTALL_DIR"
else
    if [[ -e $INSTALL_DIR ]]; then
        printf '错误: 安装目标已存在但编译器不完整: %s\n' "$INSTALL_DIR" >&2
        printf '请移走该目录后重试。\n' >&2
        exit 1
    fi

    mkdir -p -- "$INSTALL_ROOT"
    TEMP_DIR=$(mktemp -d "${INSTALL_ROOT}/.${COMPILER_PACKAGE}.XXXXXX")

    if [[ -z $ARCHIVE ]]; then
        ARCHIVE="${TEMP_DIR}/${COMPILER_PACKAGE}.tgz"
        printf '下载编译器: %s\n' "$COMPILER_URL"
        if command -v curl >/dev/null 2>&1; then
            curl --fail --location --retry 3 --retry-all-errors \
                --continue-at - --output "$ARCHIVE" "$COMPILER_URL"
        elif command -v wget >/dev/null 2>&1; then
            wget --continue --tries=3 --output-document="$ARCHIVE" "$COMPILER_URL"
        else
            printf '错误: 未找到 curl 或 wget，无法下载编译器\n' >&2
            printf '可以手工下载后用 --archive <文件> 安装: %s\n' "$COMPILER_URL" >&2
            exit 1
        fi
    elif [[ ! -f $ARCHIVE ]]; then
        printf '错误: 找不到压缩包: %s\n' "$ARCHIVE" >&2
        exit 1
    fi

    if ! printf '%s  %s\n' "$COMPILER_SHA256" "$ARCHIVE" | sha256sum --check --status; then
        printf '错误: 编译器压缩包校验失败: %s\n' "$ARCHIVE" >&2
        exit 1
    fi

    printf '解压编译器...\n'
    tar -xzf "$ARCHIVE" -C "$TEMP_DIR"
    EXTRACTED_DIR="${TEMP_DIR}/${COMPILER_PACKAGE}"
    EXTRACTED_COMPILER="${EXTRACTED_DIR}/${COMPILER_NAME}/bin/${COMPILER_PREFIX}gcc"
    if [[ ! -x $EXTRACTED_COMPILER ]]; then
        printf '错误: 压缩包中未找到编译器: %s\n' "${COMPILER_PREFIX}gcc" >&2
        exit 1
    fi
    if ! "$EXTRACTED_COMPILER" --version >/dev/null; then
        printf '错误: 编译器无法在本机运行: %s\n' "$EXTRACTED_COMPILER" >&2
        exit 1
    fi

    chmod -R u+w -- "$EXTRACTED_DIR"
    mv -T -- "$EXTRACTED_DIR" "$INSTALL_DIR"
    rm -rf -- "$TEMP_DIR"
    TEMP_DIR=
    printf '编译器已安装: %s\n' "$INSTALL_DIR"
    "$COMPILER_PATH" --version | head -n 1
fi

CANN_ENV="${CANN_DIR}/export_act.sh"
if [[ -n $SKIP_CANN ]]; then
    printf '已跳过 CANN\n'
elif [[ -f $CANN_ENV ]]; then
    printf 'CANN 已安装: %s\n' "$CANN_DIR"
elif [[ $(uname -m) != x86_64 ]]; then
    printf '警告: CANN 只支持 Linux x86_64 主机，已跳过\n' >&2
else
    if [[ -e $CANN_DIR ]]; then
        printf '错误: 安装目标已存在但 CANN 不完整: %s\n' "$CANN_DIR" >&2
        printf '请移走该目录后重试。\n' >&2
        exit 1
    fi
    mkdir -p -- "$(dirname -- "$CANN_DIR")"
    TEMP_DIR=$(mktemp -d "$(dirname -- "$CANN_DIR")/.cann.XXXXXX")

    if [[ -z $CANN_ARCHIVE ]]; then
        CANN_ARCHIVE="${TEMP_DIR}/SVP_PC.tar.gz"
        printf '下载 CANN: %s\n' "$CANN_URL"
        if command -v curl >/dev/null 2>&1; then
            curl --fail --location --retry 3 --retry-all-errors \
                --continue-at - --output "$CANN_ARCHIVE" "$CANN_URL"
        elif command -v wget >/dev/null 2>&1; then
            wget --continue --tries=3 --output-document="$CANN_ARCHIVE" "$CANN_URL"
        else
            printf '错误: 未找到 curl 或 wget，无法下载 CANN\n' >&2
            printf '可以手工下载后用 --cann-archive <文件> 安装: %s\n' "$CANN_URL" >&2
            exit 1
        fi
    elif [[ ! -f $CANN_ARCHIVE ]]; then
        printf '错误: 找不到压缩包: %s\n' "$CANN_ARCHIVE" >&2
        exit 1
    fi
    if ! printf '%s  %s\n' "$CANN_SHA256" "$CANN_ARCHIVE" | sha256sum --check --status; then
        printf '错误: CANN 压缩包校验失败: %s\n' "$CANN_ARCHIVE" >&2
        exit 1
    fi

    # SVP_PC.tar.gz 内是 SVP_NNN_PC_*.tgz，其中的 CANN/Ascend-cann-toolkit_*.run 才是安装器。
    printf '解压 CANN...\n'
    INNER=$(tar -tzf "$CANN_ARCHIVE" | grep -E '(^|/)SVP_NNN_PC_[^/]*\.tgz$')
    tar -xOzf "$CANN_ARCHIVE" -- "$INNER" >"${TEMP_DIR}/svp_nnn_pc.tgz"
    INSTALLER=$(tar -tzf "${TEMP_DIR}/svp_nnn_pc.tgz" |
        grep -E '/CANN/Ascend-cann-toolkit_[^/]*_linux\.x86_64\.run$')
    tar -xzf "${TEMP_DIR}/svp_nnn_pc.tgz" -C "$TEMP_DIR" -- "$INSTALLER"
    rm -f -- "${TEMP_DIR}/svp_nnn_pc.tgz"

    printf '安装 CANN: %s\n' "$CANN_DIR"
    chmod +x -- "${TEMP_DIR}/${INSTALLER}"
    DISPLAY= SHELL=/bin/bash "${TEMP_DIR}/${INSTALLER}" \
        --install --quiet --nox11 --install-path="${TEMP_DIR}/cann"
    if [[ ! -x ${TEMP_DIR}/cann/ascend-toolkit/svp_latest/atc/bin/atc ]]; then
        printf '错误: CANN 安装后未找到 atc\n' >&2
        exit 1
    fi
    mv -T -- "${TEMP_DIR}/cann" "$CANN_DIR"

    # 与 mipk_model 等模型转换脚本约定的环境脚本，通过 CANN_ENV 使用。
    {
        printf '#!/bin/sh\n\nexport CANN_ROOT=%s\n' "${CANN_DIR}/ascend-toolkit/svp_latest"
        cat <<'ENV'
export DDK_PATH="$CANN_ROOT/x86_64-linux"
export NPU_HOST_LIB="$DDK_PATH/acllib/lib32/stub"
case ":$PATH:" in
    *":$CANN_ROOT/atc/bin:"*) ;;
    *) export PATH="$CANN_ROOT/atc/bin:$CANN_ROOT/toolkit/bin:$PATH" ;;
esac
case ":${LD_LIBRARY_PATH:-}:" in
    *":$CANN_ROOT/atc/lib64:"*) ;;
    *) export LD_LIBRARY_PATH="$CANN_ROOT/atc/lib64:$CANN_ROOT/atc/third_party_lib:$CANN_ROOT/toolkit/tools/sim/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ;;
esac
ENV
    } >"$CANN_ENV"

    # atc 会在当前目录写临时文件，在临时目录中验证。
    if ! (cd "$TEMP_DIR" && bash -c '. "$1" && atc --version' _ "$CANN_ENV" >/dev/null); then
        printf '错误: atc 无法运行，请检查 CANN 安装: %s\n' "$CANN_DIR" >&2
        exit 1
    fi
    printf 'CANN 已安装: %s\n' "$CANN_DIR"
fi

printf '\n接下来: source %s/export.sh\n' "$SCRIPT_DIR"
