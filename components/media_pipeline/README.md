# media_pipeline

Hi3516CV610 的媒体管线（MAPI）：在 MPP 之上封装视频采集（VCAP）、处理（VPSS）、编码（VENC）、
音频采集、编解码和输出等模块，编译为静态库，连同它使用的日志库 `hi_log`。

这个组件没有菜单开关，由 `media_engine` 自动选中。一般应用直接使用 `media_engine` 的接口；需要
直接调用 MAPI 时在组件里声明 `REQUIRES media_pipeline`。MPP 的头文件和库由 SDK 提供。

## 源码包和补丁

```text
media_pipeline/
├── media-pipeline-557c69bd.tar.xz   上游源码，未改动（pipeline/ 和 log/ 两个目录）
└── patches/                         构建时按文件名顺序打在源码上
```

| 补丁 | 内容 |
| --- | --- |
| `0001-cv610-audio-output-and-encoder-fixes.patch` | 针对 CV610 的音频输出、音频解码和编码器修正 |
| `0002-sensor-device-address.patch` | 采集时把配置的 I2C 设备地址设给传感器，两路摄像头用同型号传感器时需要 |
| `0003-sdk-build-integration.patch` | 构建接入：MPP 配置头文件目录可单独指定等 |

源码包在构建目录里解开并打补丁，不会修改 SDK 目录；源码包或任何一个补丁变了，下次构建会重新解包。
主机上需要有 `patch` 命令。

## 更新上游

1. 把新的上游 `pipeline/`、`log/` 两个目录打成 `media-pipeline-<版本>.tar.xz`（顶层目录同名），
   替换旧的源码包，并修改 `CMakeLists.txt` 里的版本号。
2. 构建一次。哪个补丁打不上，CMake 会报出它的文件名；对照新的上游源码修正这个补丁，已被上游
   合入的补丁直接删除。

新增修改时同样以补丁的形式加到 `patches/` 下，不要改源码包。
