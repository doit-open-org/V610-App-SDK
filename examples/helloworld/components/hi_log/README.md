# hi_log 日志组件

`hi_log` 提供四个按严重程度排列的日志级别：

```text
Error < Warning < Info < Debug
```

选择某一级别会同时启用该级别及所有更严重的级别；选择 `None` 可关闭全部
输出。在下面的交互式配置菜单中通过 `Logging -> Log level` 设置：

```sh
hifw.py menuconfig
```

应用可调用：

```c
hi_log_error("message");
hi_log_warn("message");
hi_log_info("message");
hi_log_debug("message");
```

`hi_log_configured_level()` 返回已配置的 `hi_log_level_t` 值。
