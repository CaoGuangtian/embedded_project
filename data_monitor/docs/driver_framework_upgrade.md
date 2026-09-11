# Linux 驱动框架升级说明

## 1. 升级背景

`data_monitor` 初版主要通过自定义字符设备向用户空间提供硬件接口。这种方式便于快速验证功能，但每个外设都需要重复实现设备节点、数据读取、事件分发和资源管理逻辑，用户空间程序也会依赖项目私有的 `/dev/dm_*` 接口。

本次升级遵循“优先复用 Linux 标准设备子系统”的原则：

- LED 使用 `gpio-leds` 和 LED class；
- 按键使用 `gpio-keys` 和 Input 子系统；
- AP3216C、ICM20608 传感器接入 IIO 子系统；
- 蜂鸣器暂时保留自定义 GPIO 字符设备，因为当前硬件只作为简单 GPIO 开关使用。

这样既保留了原有的数据采集、告警和网络上报功能，也将硬件访问接口逐步迁移到 Linux 通用框架中。

## 2. 升级前后对比

| 外设 | 升级前 | 升级后 | 升级收益 |
| --- | --- | --- | --- |
| LED | 自定义 `platform_driver`、`cdev` 和 `/dev/dm_led` | `gpio-leds`、LED class、`/sys/class/leds/datamon:green/brightness` | 减少字符设备模板代码，可直接使用 Linux LED 接口 |
| 按键 | 自定义 GPIO 中断字符设备和 `/dev/dm_key` | `gpio-keys`、Input 子系统、`/dev/input/eventX` | 统一按键事件格式，便于复用 `evdev` 工具 |
| AP3216C | 自定义 I2C `miscdevice`、二进制 `read()` | I2C 驱动注册为 IIO 设备 | 统一传感器通道接口，便于扩展采样和触发功能 |
| ICM20608 | 自定义 SPI 字符设备和二进制 `read()` | SPI 驱动注册为 IIO 设备 | 统一加速度、温度和角速度访问方式 |
| 蜂鸣器 | 自定义 GPIO 字符设备和 `/dev/dm_beep` | 接口保持不变，后续可考虑 `pwm-beeper` | 维持当前硬件兼容性，避免不必要的迁移 |

## 3. LED 与按键标准化

LED 通过设备树绑定到 `gpio-leds`，用户空间使用以下接口控制：

```text
/sys/class/leds/datamon:green/brightness
```

升级后不再需要维护 LED 专用的字符设备注册、主次设备号分配和 `ioctl` 接口。后续还可以直接使用内核 LED trigger 实现心跳、定时或磁盘活动指示。

按键通过 `gpio-keys` 注册为输入设备，用户空间读取标准的 `struct input_event`，并根据 `EV_KEY` 事件切换工作模式。这样按键不再依赖私有的 `/dev/dm_key`，新增其他按键时也可以复用 Linux Input 子系统的去抖、中断和事件分发机制。

## 4. 传感器接入 IIO 子系统

AP3216C 通过 I2C 驱动注册为 IIO 设备，提供以下属性：

```text
/sys/bus/iio/devices/iio:deviceN/in_intensity0_raw
/sys/bus/iio/devices/iio:deviceN/in_illuminance0_raw
/sys/bus/iio/devices/iio:deviceN/in_proximity0_raw
```

ICM20608 通过 SPI 驱动注册为 IIO 设备，提供以下属性：

```text
/sys/bus/iio/devices/iio:deviceN/in_accel_x_raw
/sys/bus/iio/devices/iio:deviceN/in_accel_y_raw
/sys/bus/iio/devices/iio:deviceN/in_accel_z_raw
/sys/bus/iio/devices/iio:deviceN/in_temp0_raw
/sys/bus/iio/devices/iio:deviceN/in_anglvel_x_raw
/sys/bus/iio/devices/iio:deviceN/in_anglvel_y_raw
/sys/bus/iio/devices/iio:deviceN/in_anglvel_z_raw
```

IIO 升级带来的主要好处是：

- 统一传感器通道命名和访问方式；
- 用户空间不再依赖私有二进制结构体布局；
- 后续可以支持 `scan_elements`、buffer、触发器和批量采样；
- 便于直接使用 sysfs 进行现场调试；
- 更容易替换同类传感器或迁移到其他 ARM 平台。

用户空间通过 IIO 设备的 `name` 文件匹配 `dm-ap3216c` 和 `dm-icm20608`，避免依赖不稳定的 `iio:deviceN` 编号。

## 5. 用户空间改动

`dm_collector` 保留原有的网络协议、滤波算法、告警策略、日志记录和运行模式切换逻辑，本次只替换底层硬件访问方式：

- LED：写入 LED class 的 `brightness` 属性；
- 按键：读取 Linux `struct input_event`，处理 `EV_KEY` 按键事件；
- AP3216C：读取 IIO 的红外、光照和接近度 raw 属性；
- ICM20608：读取 IIO 的三轴加速度、温度和三轴角速度 raw 属性；
- 蜂鸣器：继续向 `/dev/dm_beep` 写入 `0` 或 `1`。

这种分层方式保持了上层业务行为基本不变，同时将硬件差异集中在适配层，降低了驱动框架迁移对采集、告警和网络功能的影响。

## 6. 内核配置与构建变化

当前 4.1.15 BSP 需要确认以下配置已经启用：

```text
CONFIG_IIO
CONFIG_LEDS_GPIO
CONFIG_KEYBOARD_GPIO
CONFIG_INPUT_EVDEV
```

本次模块构建目标调整为：

```text
dm_beep.ko
dm_ap3216c_iio.ko
dm_icm20608_iio.ko
```

LED 和按键由设备树节点直接绑定到内核通用驱动，不再编译项目自定义的 `dm_led.ko` 和 `dm_key.ko`。这样可以减少需要维护的模块数量，使构建、部署和后续平台迁移更加清晰。

## 7. 验证方法

启动使用升级后设备树的开发板，并加载传感器模块：

```sh
insmod dm_beep.ko
insmod dm_ap3216c_iio.ko
insmod dm_icm20608_iio.ko
```

检查 LED 和按键：

```sh
cat /sys/class/leds/datamon:green/brightness
echo 1 > /sys/class/leds/datamon:green/brightness
cat /proc/bus/input/devices
./bin/dm_keyread
```

检查传感器：

```sh
./bin/dm_ap3216c_read
./bin/dm_icm20608_read
```

如果开发板的 GPIO、输入设备名称或 IIO 设备名称不同，需要同步修改设备树和用户空间的匹配条件。

## 8. 当前边界与后续方向

本次升级已经完成 LED、按键和两类传感器的标准子系统接入，但仍有以下后续空间：

1. 为 IIO 传感器增加 buffer 和触发器，减少逐个读取 sysfs 属性的开销；
2. 根据实际硬件连接情况，将蜂鸣器迁移到 `pwm-beeper`；
3. 为设备树节点补充更完整的电源管理和 suspend/resume 支持；
4. 完善 IIO scale、采样频率和批量采集属性；
5. 增加统一设备发现逻辑，减少对固定设备路径的依赖。

总体上，本次升级将项目从“每个外设一套私有字符设备接口”推进到“总线驱动 + Linux 标准设备子系统 + 用户空间适配层”的结构，为扩展传感器种类、提高采样能力和迁移硬件平台打下基础。
