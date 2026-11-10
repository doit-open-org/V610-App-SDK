# ble_gatt

通过 BlueZ 的 D-Bus 接口，把板载 AIC8800D80 作为 BLE 外设使用：以 `COVE610` 为名广播，提供一个服务，服务下有一个可读特征，读出的值是设备名。手机连接和断开时打印日志。

| 项目 | UUID |
| --- | --- |
| 服务 | `a6f1c000-04f1-4d9b-a002-c06100000001` |
| 特征（只读） | `a6f1c000-04f1-4d9b-a002-c06100000002` |

## 构建和运行

```sh
source <sdk>/export.sh
hifw.py set-target hi3516cv610
hifw.py build
```

把 `out/ble_gatt` 拷到板子上。先启动 BlueZ，再运行程序：

```sh
ble-service start
./ble_gatt
```

看到 `advertising as COVE610` 后，在手机上用 nRF Connect 或 LightBlue 扫描 `COVE610` 并连接，板端会打印 `/org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX connected`。nRF Connect 显示 `CONNECTED / NOT BONDED` 属于正常情况。系统蓝牙设置一般看不到这类 BLE 外设，要用上面的 App 连接。

按 `Ctrl-C` 退出，BlueZ 会自动撤销广播和服务。不再使用蓝牙时执行 `ble-service stop`。
