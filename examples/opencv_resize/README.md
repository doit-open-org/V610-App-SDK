# opencv_resize

使用 SDK 的 `opencv` 组件：绘制一张 640×480 的测试图，缩小到 160×120，分别保存为 JPEG。不需要外接硬件。

## 构建和运行

```sh
source <sdk>/export.sh
hifw.py set-target hi3516cv610
hifw.py build
```

把 `out/opencv_resize` 拷到板子上运行，参数为输出目录，默认 `/tmp`：

```sh
./opencv_resize          # 输出 /tmp/opencv_full.jpg 和 /tmp/opencv_small.jpg
./opencv_resize /root
```

运行时打印 OpenCV 版本和两张图片的尺寸。
