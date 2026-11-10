# lvgl_widgets

运行 LVGL 自带的 `lv_demo_widgets()`：通过 DRM 显示，输入在启动时自动选择。

| 顺序 | 输入 | 默认设备 |
| --- | --- | --- |
| 1 | evdev 触摸 | `/dev/input/event0` |
| 2 | 当前终端，作为编码器 | `/dev/tty` |

触摸设备不存在或打不开时回退到当前终端。触摸坐标按控制器上报的范围映射到实际显示分辨率，程序里
不写死屏幕尺寸。

用终端操作时的按键：

| 按键 | 操作 |
| --- | --- |
| `PgUp` 或方向键上 | 向左旋转 |
| `PgDn` 或方向键下 | 向右旋转 |
| `Enter` 或空格 | 按下 |

## 硬件连接

断电后把 LCD 屏的 FPC 排线接到 LCD 连接器，再上电。LCD 与 SD 卡共用引脚，板子需要处于 LCD 模式，这是默认模式；用 `sdio1-pins` 查看当前模式，切换后重启生效。没有接触摸屏时，可以用终端按键操作。

## 构建和运行

```sh
source <sdk>/export.sh
hifw.py set-target hi3516cv610
hifw.py build
```

把 `out/lvgl_widgets` 拷到板子上运行，`Ctrl-C` 退出，退出时恢复终端设置。显示设备默认
`/dev/dri/card0`，三个设备路径都可以在 `hifw.py menuconfig` 里修改。触摸不是 `event0` 时，先用
`cat /proc/bus/input/devices` 确认。
