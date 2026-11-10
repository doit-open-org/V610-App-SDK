# V610-App-SDK

V610 应用开发 SDK，提供 CMake 组件系统、Kconfig 配置，以及媒体、推理和图形界面组件。

## 快速开始

解压 SDK 发布包后，在 SDK 根目录执行：

```sh
bash install.sh
source export.sh
hifw.py -C examples/helloworld set-target hi3516cv610
hifw.py -C examples/helloworld build -j 4
```

开发机需要 Linux x86_64、Python 3、CMake、make、tar、sha256sum 和 pkg-config；下载需要 curl 或 wget。`export.sh` 支持 Bash 和 zsh。

## 安装

发布包不含交叉编译器和模型转换工具。`install.sh` 下载并安装：

| 内容 | 默认位置 | 说明 |
| --- | --- | --- |
| 交叉编译器 GCC 10.3 / musl | `~/.v610/compiler/` | 编译应用必需 |
| CANN（模型转换工具 `atc`，约 290 MB） | `~/.v610/cann/` | 只在转换 NPU 模型时需要 |

两者都装在 SDK 目录之外，多个 SDK 版本共用一份，已安装时自动跳过。每台开发机只需执行一次。

| 参数 | 用途 |
| --- | --- |
| `--skip-cann` | 不安装 CANN |
| `--archive <文件>` | 使用已下载的编译器压缩包，离线安装 |
| `--cann-archive <文件>` | 使用已下载的 `SVP_PC.tar.gz`，离线安装 CANN |
| `--compiler-dir <目录>` | 编译器安装位置，之后使用 SDK 前需设置 `HIFW_COMPILER_ROOT=<目录>` |
| `--cann-dir <目录>` | CANN 安装位置，之后使用 SDK 前需设置 `HIFW_CANN_ROOT=<目录>` |

装有 CANN 时，`source export.sh` 会设置 `CANN_ENV` 指向 CANN 的环境脚本，模型转换脚本通过它使用 CANN。CANN 的环境会改动 `LD_LIBRARY_PATH`，所以不会自动加载；需要在当前终端直接使用 `atc` 时执行：

```sh
source "$CANN_ENV"
```

## 从源码使用

不使用发布包、直接使用源码时，克隆时需包含子模块：

```sh
git clone --recurse-submodules https://github.com/doit-open-org/V610-App-SDK.git
```

然后从同版本的 V610-Buildroot 构建结果中导入平台开发文件：

```sh
scripts/install-platform.sh /path/to/V610-Buildroot/output/host
```

## 编译应用

加载环境后，在应用目录执行：

```sh
hifw.py set-target hi3516cv610
hifw.py menuconfig
hifw.py build -j 4
```

默认产物为 `out/<项目名>`，复制到匹配的板端系统运行。也可使用 `hifw.py -C <项目目录>` 从其他目录执行。

| 命令 | 用途 |
| --- | --- |
| `hifw.py set-target hi3516cv610 --build-type Debug` | 选择 Debug 构建 |
| `hifw.py reconfigure` | 重新配置并编译 |
| `hifw.py install --destdir ./stage --prefix /usr` | 暂存安装文件 |
| `hifw.py clean` | 清理编译目标 |
| `hifw.py fullclean` | 删除构建和输出目录 |

切换 SDK 或编译环境后，执行 `fullclean` 并重新选择目标。

## 创建项目

可参考 `examples/helloworld`。最小项目包含以下三个文件。

`CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES C CXX ASM)
include("$ENV{APP_SDK_PATH}/cmake/project.cmake")
hifw_project(my_app)
```

`main/CMakeLists.txt`：

```cmake
hifw_component_register(SRCS main.c)
```

`main/main.c`：

```c
#include <stdio.h>

int main(void)
{
    puts("Hello, V610!");
    return 0;
}
```

组件使用 `hifw_component_register` 声明源文件、头文件目录和依赖；系统库通过 `PKG_CONFIG` 引入。项目组件放在 `components/`，配置放在 `Kconfig`，默认配置放在 `sdkconfig.defaults`。

组件的用法见 `components/<组件>/README.md`。示例如下，各示例目录的 README 说明硬件连接、构建和运行方法：

| 示例 | 内容 |
| --- | --- |
| `examples/helloworld` | 基础项目与项目级组件 |
| `examples/camera_record` | 摄像头录制 H.264 并抓拍 JPEG，可选 MIPI-1 或 MIPI-2 |
| `examples/opencv_resize` | OpenCV 图像缩放 |
| `examples/lvgl_widgets` | LVGL 界面 |
| `examples/audio_loopback` | 麦克风到扬声器回环 |
| `examples/ble_gatt` | BLE GATT 外设 |

## 配合 hi_aiot_solution 使用

hi_aiot_solution 以子模块的方式把本 SDK 放在 `apps/cove_610/V610-App-SDK`。在这个位置使用时，不需要导入平台开发文件，也不需要执行本 SDK 的 `install.sh`：`export.sh` 会自动使用工程编出的厂商 SDK（`_build/sdk/Hi3516CV610`），以及工程 `install.sh` 安装在 `toolchain/` 和 `CANN/` 中的编译器和 CANN。先完成一次工程的基础镜像构建，再执行：

```sh
source apps/cove_610/V610-App-SDK/export.sh
```

SDK 放在其他位置时，手动指定厂商 SDK 和编译器：

```sh
export APP_SDK_PLATFORM=solution
export HI_SDK_ROOT=/path/to/_build/sdk/Hi3516CV610
export CV610_TOOLCHAIN_PATH=/path/to/toolchain/<编译器>/bin
source export.sh
```

在 Buildroot 和 hi_aiot_solution 之间切换时，建议打开新的终端，并对应用执行 `hifw.py fullclean`。

## 生成 SDK 发布包

将本仓库克隆到 V610-Buildroot 根目录后，在 V610-Buildroot 根目录执行：

```sh
make app-sdk
```

版本号由 SDK 根目录的 `VERSION` 配置，产物为 `dist/V610-App-SDK-<版本>.tar.gz`。板端系统库更新后，应重新生成匹配的 SDK。

## 许可证

本仓库采用 [Apache License 2.0](LICENSE) 许可。OpenCV、LVGL 等第三方组件遵循各自的许可证，`platform/hi3516cv610/hisilicon/` 下的海思库遵循其中的 `licenses/LICENSE`。
