# helloworld

最小的应用项目，演示项目结构、Kconfig 配置和项目级组件。

```text
helloworld/
├── CMakeLists.txt        项目入口
├── Kconfig               项目配置项 HELLOWORLD_MESSAGE
├── sdkconfig.defaults    配置默认值
├── main/                 应用入口组件
└── components/hi_log/    项目级组件：分级日志
```

## 构建和运行

```sh
source <sdk>/export.sh
hifw.py set-target hi3516cv610
hifw.py build
```

把 `out/helloworld` 拷到板子上运行。默认日志级别为 Info，输出：

```text
helloworld example
build mode: release
[error] error log enabled
[warn] warning log enabled
[info] configured by hifw
```

## 修改配置

```sh
hifw.py menuconfig
hifw.py build
```

在菜单中可以修改打印的消息，以及 `hi_log` 的日志级别。级别的含义见 `components/hi_log/README.md`。选择 Debug 构建（`hifw.py set-target hi3516cv610 --build-type Debug`）时，`build mode` 一行输出 `debug`。
