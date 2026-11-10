# opencv

OpenCV 4.13 的静态库，默认模块 `core`、`imgproc`、`imgcodecs`（JPEG、PNG）。应用链接时只带走用到的部分，
程序本身自足：板端镜像里不需要有 OpenCV，也不用往板上拷库。

```text
CONFIG_OPENCV=y
```

```cmake
hifw_component_register(
    SRCS src/main.cpp
    REQUIRES opencv)
```

```cpp
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
```

完整例子见 `examples/opencv_resize`，链接后的程序约 2.3 MB（strip 之后）。

## 预编译库和源码编译

组件里同时带着预编译库（`prebuilt/`）和精简过的源码包（`opencv-4.13.0-reduced.tar.xz`），构建时自动二选一：

| 情况 | 用什么 |
| --- | --- |
| 当前编译器与 `prebuilt/COMPILER` 记录的一致，模块为默认的三个 | 直接用 `prebuilt/` |
| 编译器不同（目标或版本不一样） | 从源码编译 |
| `CONFIG_OPENCV_MODULES` 改过，或指定了 `OPENCV_SOURCE_ARCHIVE` | 从源码编译 |

从源码编译只在第一次发生，约一分钟，结果按编译器、模块和编译选项缓存在 `~/.v610/cache`（可用
`HIFW_CACHE_DIR` 改位置），之后所有工程共用。C++ 静态库不能跨编译器版本链接，所以按编译器区分，
而不是一律使用预编译库。

编译选项：静态、位置无关、开启 NEON，zlib、libjpeg-turbo、libpng 用 OpenCV 自带的版本一并编入，
所以除了板上的 C/C++ 运行库外没有其他依赖。

## 更多模块

自带的源码包只含默认的三个模块。需要其他模块时，下载完整的 OpenCV 源码包并指给组件：

```sh
export OPENCV_SOURCE_ARCHIVE=/path/to/opencv-4.13.0.tar.gz
```

```text
CONFIG_OPENCV_MODULES="core,imgproc,imgcodecs,features2d"
```

## 在普通 CMake 工程里使用

不是 HiFW 工程也可以用同一套逻辑：

```cmake
include("$ENV{APP_SDK_PATH}/components/opencv/opencv.cmake")
hifw_opencv_provide(OPENCV_ROOT)
find_package(OpenCV REQUIRED PATHS "${OPENCV_ROOT}/lib/cmake/opencv4"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
target_link_libraries(app PRIVATE ${OpenCV_LIBS})
```

## 更新预编译库

改了源码包、默认模块或 `opencv.cmake` 里的编译选项后，用 SDK 自己的编译器重新生成 `prebuilt/`：

```sh
source <sdk>/export.sh
components/opencv/build.sh
```
