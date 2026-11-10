# camera_record

用 `media_engine` 从 GC4023 摄像头录制 1280×720、25 fps、2 Mbit/s 的 H.264 码流，录制结束后再抓拍一张 JPEG。

## 硬件连接

断电后把 GC4023 摄像头接到 MIPI-1 或 MIPI-2 接口，再上电。MIPI-1 使用 I2C0，MIPI-2 使用 I2C1。

## 构建和运行

```sh
source <sdk>/export.sh
hifw.py set-target hi3516cv610
hifw.py build
```

把 `out/camera_record` 拷到板子上，在登录 shell 中运行：

```sh
./camera_record                          # MIPI-1 录制 5 秒，输出 /tmp/camera.h264
./camera_record 10 /tmp/test.h264 2      # MIPI-2 录制 10 秒
```

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| 第 1 个 | 录制秒数 | `5` |
| 第 2 个 | H.264 输出文件，JPEG 保存为同名加 `.jpg` | `/tmp/camera.h264` |
| 第 3 个 | 摄像头接口，`1` 为 MIPI-1，`2` 为 MIPI-2 | `1` |

结束时打印包数、关键帧数、帧率和码率，例如：

```text
124 packets (3 key frames), 1220928 bytes in 5.0 s: 24.6 fps, 1939 kbit/s
snapshot /tmp/camera.h264.jpg: ok
```

输出的是 H.264 裸码流，可用 `ffplay /tmp/camera.h264` 或 VLC 播放。按 `Ctrl-C` 提前结束录制。摄像头同一时间只能被一个程序使用，运行前先结束其他使用摄像头的程序。
