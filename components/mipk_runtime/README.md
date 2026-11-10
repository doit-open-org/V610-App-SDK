# mipk_runtime

模型推理运行时 mipk，以源码方式编入应用，推理后端是芯片的 NPU（SVP ACL）。

mipk 和它的扩展模块 mipk_extra_module 是独立的仓库，作为子模块检出在本目录下：

```text
components/mipk_runtime/
├── mipk/                 模型运行时
└── mipk_extra_module/    扩展解码器（FastestDet、MediaPipe 手部等）
```

从 git 克隆 SDK 时要带上子模块（`git clone --recurse-submodules`，或之后
`git submodule update --init`）；SDK 发布包里已经包含。要用别处的检出时，设置 `MIPK_PATH`、
`MIPK_EXTRA_MODULE_PATH`（环境变量或 CMake 变量）。

```cmake
# 工程顶层 CMakeLists.txt
set(HIFW_EXTRA_COMPONENTS mipk_runtime)
```

```text
# sdkconfig.defaults
CONFIG_MIPK_RUNTIME=y
CONFIG_MIPK_MODEL_YOLO=y
```

组件用 `REQUIRES mipk_runtime` 依赖它，源码里 `#include "mipk.h"`。`hifw.py menuconfig`
里选择编入哪些模型解码器和算子，只编入用到的可以明显减小可执行文件。

`mipk_write_capabilities(TARGET <应用> OUTPUT <文件>)` 把应用实际编入的模块写成清单，打包模型时
交给 `mipk_tool.py --target-capabilities`，可提前确认应用能打开这个模型包。

完整的应用见独立的应用仓库 V610-Apps。
