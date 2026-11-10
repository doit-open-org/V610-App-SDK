# audio_loopback

麦克风到扬声器的软件回环：用 `media_engine` 采集 16 kHz 单声道 PCM，直接写到音频输出。回环时关闭 AEC，否则回放的声音会被消掉。

## 硬件连接

断电后把麦克风接到 MIC 接口、扬声器接到 SPK 接口，再上电。扬声器两端都不要接地。

## 构建和运行

```sh
source <sdk>/export.sh
hifw.py set-target hi3516cv610
hifw.py build
```

把 `out/audio_loopback` 拷到板子上，在登录 shell 中运行：

```sh
./audio_loopback          # 回环 10 秒
./audio_loopback 30 1     # 回环 30 秒，使用麦克风通道 1
```

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| 第 1 个 | 运行秒数 | `10` |
| 第 2 个 | 麦克风通道，`0` 为 ADCL，`1` 为 ADCR | `0` |

程序每秒打印一次帧数和输入峰值。对着麦克风说话时，峰值应明显升高，同时扬声器能听到回放；峰值一直很低时换另一个通道试试。按 `Ctrl-C` 提前结束。
