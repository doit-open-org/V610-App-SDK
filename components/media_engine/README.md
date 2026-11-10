# media_engine

`media_engine` 是 HiFW 面向 Hi3516CV610 的媒体组件，统一管理 MAPI/MPP 资源，并提供相互独立的视频、录音和音频播放接口。

## 能力

- 最多接入两路 GC4023，每路最多配置三个 NV12、H264 或 H265 输出；
- 获取原始视频帧和编码包，保存 JPEG、绘制检测框、请求 IDR；
- 采集 16 kHz 单声道 S16LE PCM，默认开启 AEC；
- 将同一份录音同时输出为 PCM 和 Opus；
- 播放 PCM、WAV、MP3、AAC/ADTS、Opus 和 Ogg Opus 文件或内存数据；
- 流式播放 PCM、MP3、AAC/ADTS 和 Opus；
- 提示音可中断普通播放，并在结束后恢复普通播放。

## 集成

在配置中启用组件：

```text
CONFIG_MEDIA_ENGINE=y
```

`MEDIA_ENGINE` 会自动选择 `MEDIA_PIPELINE`。应用组件声明依赖：

```cmake
hifw_component_register(
    SRCS src/main.c
    REQUIRES media_engine)
```

业务代码包含：

```c
#include "media_engine/media.h"
```

## 初始化

`media_config_init()` 默认关闭所有子系统。应用按需初始化配置，`media_engine_open()` 只初始化共享资源，各子系统需要独立打开：

```c
media_config config;
media_config_init(&config);
media_video_config_init(&config.video);
media_audio_input_config_init(&config.audio_input);

media_context *media = NULL;
media_status result = media_engine_open(&config, &media);
if (result != MEDIA_OK) {
    return result;
}

result = media_video_open(media);
if (result != MEDIA_OK) {
    media_engine_close(media);
    return result;
}

result = media_audio_input_open(media);
if (result != MEDIA_OK) {
    media_video_close(media);
    media_engine_close(media);
    return result;
}

media_audio_input_close(media);
media_video_close(media);
media_engine_close(media);
```

视频、音频输入和音频输出可以分别配置、打开和关闭，不要求同时启用。

## 视频

`media_video_config.cameras[]` 是连续的逻辑输入。每个输入通过 `device_id` 选择 Cove-610
物理摄像头接口（0 或 1），通过 `sensor_type` 选择传感器型号，默认型号为
`MEDIA_VIDEO_SENSOR_GC4023`；通过 `i2c.bus_id` 选择 I2C 控制器（0 或 1）。
`i2c.address` 使用传感器 SDK 的 8 位地址格式；为 0 时不覆盖传感器驱动的默认地址，
非零值会通过传感器对象的地址设置回调显式覆盖。默认配置中 camera0 使用 I2C0 和
`0x52`，camera1 使用 I2C1 和 `0x42`。

`media_video_config.streams[]` 由应用定义，每个流通过 `camera_id` 选择逻辑输入，并通过
`output` 选择原始帧、H264 或 H265。流 ID 是配置数组中的索引，不绑定固定 VPSS 通道用途。
`media_video_config_init()` 默认配置一路 1920x1080 H.264 流；需要识别流等额外输出时，
由应用显式增加并配置。

原始帧由 `media_video_acquire_frame()` 获取，使用后必须调用 `media_video_release_frame()`。编码包由 `media_video_read_packet()` 获取，使用后必须调用 `media_video_packet_release()`。

## 音频

音频输入和音频输出使用独立的 open/close 接口。音频输入默认开启 AEC，不依赖音频输出；需要原始录音时将 `enable_aec` 设为 `false`。

`media_audio_codec` 描述 AAC、MP3 和 Opus 编解码类型。`media_audio_format` 描述文件、内存和提示音数据的格式，例如 PCM、WAV、ADTS 和 Ogg Opus。

### 音频输出

实时 PCM 通过 `media_audio_output_write()` 写入。最后一块 PCM 写入后调用 `media_audio_output_finish()`，输出缓冲区和 AO 队列排空后进入 `MEDIA_AUDIO_PLAY_IDLE`：

```c
media_audio_output_write(media, samples, sample_count, 100);
media_audio_output_finish(media);
```

### 编码器

编码器独立于音频输入打开，当前支持将 16 kHz 单声道 PCM 编码为 Opus：

```c
media_audio_encoder_config encoder_config;
media_audio_encoder_config_init(&encoder_config);

media_audio_encoder *encoder = NULL;
media_audio_encoder_open(media, &encoder_config, &encoder);

uint8_t packet[1500];
size_t packet_size;
uint64_t pts;
uint32_t sequence;
media_audio_encoder_read_packet(encoder, packet, sizeof(packet), 100,
    &packet_size, &pts, &sequence);

media_audio_encoder_close(encoder);
```

PCM 和 Opus 使用独立队列。`media_audio_input_read()` 可持续向语音唤醒提供 PCM，同时 `media_audio_encoder_read_packet()` 向 AI 对话提供 Opus 包。编码器必须在音频输入之后打开、之前关闭。

### 解码器

AAC、MP3 和 Opus 等编码响应通过 `media_audio_decoder` 输入音频输出。AAC 和 MP3 使用 `media_audio_decoder_write()` 写入字节流，Opus 使用 `media_audio_decoder_write_packet()` 写入完整的 RFC 7587 包：

```c
media_audio_decoder_config decoder_config;
media_audio_decoder_config_init(&decoder_config);

media_audio_decoder *decoder = NULL;
media_audio_decoder_open(media, &decoder_config, &decoder);
media_audio_decoder_write_packet(decoder, packet, packet_size, 100);
media_audio_decoder_finish(decoder);
media_audio_decoder_close(decoder);
```

文件和内存 Opus 播放支持：

- `MEDIA_AUDIO_FORMAT_OGG_OPUS`：标准 Ogg Opus 文件或内存数据；
- `MEDIA_AUDIO_FORMAT_OPUS`：每个 RFC 7587 Opus 包前添加 4 字节大端长度的文件或内存数据；

CV610 的 Opus 播放当前支持 16 kHz、单声道。
Opus 包自身携带帧时长，decoder 无需单独配置 `frame_samples`。解码后的
PCM 会按 `audio_output.frame_samples` 重新分帧：较短的 Opus 包跨包拼接，
较长的包拆成多个 AO 帧，仅在输入结束时对最后一个不足帧补零。

提示音使用 `MEDIA_PROMPT_QUEUE` 排队，或使用 `MEDIA_PROMPT_REPLACE` 替换正在播放和等待中的提示音。提示音结束后，普通播放从原位置继续。

## 约束

- 关闭视频前必须释放所有已获取的帧；
- 关闭录音前必须关闭对应的音频编码器；
- PCM 主输出结束后调用 `media_audio_output_finish()`；
- 音频解码器关闭不能与写入或结束操作并发；
- `media_engine_close()` 不能与其他媒体 API 并发；
- 一个进程只能创建一个占用 CV610 媒体硬件的 `media_context`。

## GC4023 驱动

传感器驱动编进组件，而不是用 SDK 里编好的库，这样 I2C 设备地址可以在运行时指定：

```text
sensor/
├── galaxycore_gc4023/    上游驱动源码，未改动
└── patches/              构建时按文件名顺序打在驱动的副本上
    └── 0001-gc4023-configurable-device-address.patch
```

驱动在构建目录里复制并打补丁，不会修改 SDK 目录。更新驱动时替换 `galaxycore_gc4023/` 下的文件，
补丁打不上时对照新源码修正补丁。
