# lvgl

LVGL 9.5，用于 Linux DRM 显示和 evdev 输入：

- RGB565 色彩格式，DRM 局部刷新
- pthread 线程支持，使用 libc 的内存管理
- Montserrat 10–24 号字体，Widgets demo
- 同时编入帧缓冲（fbdev）显示驱动

启用和依赖：

```text
CONFIG_LVGL=y
```

```cmake
hifw_component_register(
    SRCS src/main.c
    REQUIRES lvgl)
```

```c
#include "lvgl.h"

lv_display_t *disp = lv_linux_drm_create();
lv_linux_drm_set_file(disp, "/dev/dri/card0", -1);

lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event0");
lv_indev_set_display(touch, disp);
```

创建输入设备后用 `lv_indev_set_display()` 绑定显示，evdev 驱动会把触摸坐标范围映射到显示分辨率。
完整例子见 `examples/lvgl_widgets`。

## 源码包和补丁

```text
lvgl/
├── lvgl-9.5.0-reduced.tar.xz   上游 LVGL 9.5.0 的精简包
└── patches/                    构建时按文件名顺序打在源码上
```

`lvgl-9.5.0-reduced.tar.xz` 不是官方发布包：官方包约 100 MB，这里只保留库源码、CMake 支持和
Widgets demo（去掉了文档、测试、示例和其他 demo）。保留下来的文件与上游逐字节相同，没有改动。

| 补丁 | 内容 |
| --- | --- |
| `0001-drm-partial-update.patch` | DRM 驱动按脏区局部刷新，适合 SPI 屏 |
| `0002-evdev-mt-axis.patch` | 触摸设备只提供多点坐标轴（`ABS_MT_POSITION_X/Y`）时也能取得坐标范围 |

源码包在构建目录里解开并打补丁，不会修改 SDK 目录；主机上需要有 `patch` 命令。配置在
`lv_conf.h`；工程里放一个同名的 `lvgl` 组件可以整体替换它。

## libdrm

DRM 显示驱动需要 libdrm。组件自带它的精简源码 `libdrm-2.4.131-reduced.tar.xz`（核心库的五个源文件、
头文件，以及该库自己的构建系统为本平台生成的 `config.h` 和 fourcc 表头文件），编成静态库链接进应用。

所以 LVGL 程序只依赖板上的 C 运行库：镜像里有没有 libdrm 都能运行，同一个可执行文件可以用在不同
的镜像上。
