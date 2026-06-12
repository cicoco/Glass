# JuneGlass 眼镜端烧录与联调说明

> 适用对象：`Seeed XIAO ESP32S3 Sense`  
> 工程入口：[`firmware/glass/glass.ino`](../firmware/glass/glass.ino)

---

## 1. 文档目的

本文档用于说明如何使用 **Arduino IDE**：

- 打开眼镜端工程
- 配置板卡与 WiFi
- 烧录到 `XIAO ESP32S3 Sense`
- 通过串口与收包脚本完成最小联调

若需了解功能边界与配置含义，请先阅读：

- [`glass-requirement-mvp.md`](./glass-requirement-mvp.md)
- [`glass-implementation-mvp.md`](./glass-implementation-mvp.md)

---

## 2. 前置准备

### 2.1 软件

- Arduino IDE 2.x
- 已安装 `esp32 by Espressif` 开发板包

### 2.2 硬件

- Seeed XIAO ESP32S3 Sense
- USB 数据线
- 可用的 2.4GHz 热点

### 2.3 工程入口

在 Arduino IDE 中打开：

- [`firmware/glass/glass.ino`](../firmware/glass/glass.ino)

> Arduino IDE 会自动加载同目录下的 `.h/.cpp` 文件，不需要单独逐个打开。

---

## 3. Arduino IDE 配置

在 Arduino IDE 中确认以下选项：

### 3.1 开发板

- `工具 -> 开发板 -> esp32`
- 选择：`XIAO ESP32S3 Sense`

### 3.2 关键参数

- `PSRAM`: `OPI PSRAM`
- `USB CDC On Boot`: `Enabled`
- `Partition Scheme`: 默认即可
- `Port`: 选择插入开发板后出现的串口

> 如果后续固件明显变大，可尝试 `Huge APP` 分区方案。

---

## 4. 修改配置

在烧录前，先编辑：

- [`firmware/glass/config.h`](../firmware/glass/config.h)

至少需要修改以下配置：

```c
#define WIFI_SSID "你的热点名"
#define WIFI_PASSWORD "你的热点密码"
```

### 4.1 当前常用配置项

| 配置项 | 作用 |
|--------|------|
| `WIFI_SSID` / `WIFI_PASSWORD` | 眼镜连接的热点 |
| `IDLE_FPS_MS` | Idle 普通采集状态图像发送间隔 |
| `ACTIVE_FPS_MS` | 连续对话模式图像发送间隔 |
| `SILENCE_EXIT_SECONDS` | 连续静音多少秒后退出到 Idle |
| `AUDIO_MAX_DURATION_SECONDS` | 单次连续对话音频采集最长时长 |
| `SESSION_TIMEOUT_SECONDS` | 整个会话最长存活时间 |
| `AUDIO_RMS_THRESHOLD` | 声音检测阈值 |

---

## 5. 编译与烧录

### 5.1 编译

1. 点击 Arduino IDE 左上角 `验证`
2. 若看到类似输出，则说明编译成功：

```text
Sketch uses xxx bytes ...
Global variables use xxx bytes ...
```

若出现 `Compilation error`、`undefined reference` 或 `exit status 1`，则说明编译失败，需要先排查。

### 5.2 烧录

1. 点击 `上传`
2. 若上传过程卡在连接阶段，可执行以下操作：
   - 按住开发板 `BOOT` 键
   - 当 IDE 输出 `Connecting...` 时松开
3. 等待出现上传完成提示

---

## 6. 串口验证

### 6.1 打开串口监视器

- `工具 -> 串口监视器`
- 波特率设置为：`115200`

### 6.2 正常启动日志

烧录成功后，正常应看到类似日志：

```text
[BOOT] JuneGlass Glass FW ...
[WIFI] connecting...
[WIFI] IP: 192.168.x.x:8765
[CAM] idle qvga q=18
[TCP] listening
```

如果能看到这些日志，说明：

- 固件已正常启动
- WiFi 已连接
- 相机已进入 Idle 状态
- TCP Server 已开始监听

---

## 7. 串口调试命令

在串口监视器输入以下字符可触发调试行为：

| 命令 | 作用 |
|------|------|
| `w` | 触发唤醒，进入连续对话模式 |
| `e` | 强制结束当前会话，回到 Idle |
| `s` | 打印当前状态、堆内存、RSSI、丢帧数 |
| `i` | 打印当前 IP 与端口 |

---

## 8. 收包联调

### 8.1 电脑侧收包脚本

项目自带调试脚本：

- [`firmware/glass/tools/recv_stream.py`](../firmware/glass/tools/recv_stream.py)

它的作用是：

- 作为 TCP Client 连接眼镜端 `8765`
- 按 `JG` 协议拆包
- 打印 `IMAGE / AUDIO / CONTROL_WAKE / CONTROL_SESSION_END`
- 可选保存收到的 JPEG 图片

### 8.2 运行方式

在项目根目录执行：

```bash
python3 firmware/glass/tools/recv_stream.py <眼镜IP>
```

例如：

```bash
python3 firmware/glass/tools/recv_stream.py 192.168.31.120
```

若要保存图片：

```bash
python3 firmware/glass/tools/recv_stream.py 192.168.31.120 --save-images out_frames
```

---

## 9. 连续对话模式验证

### 9.1 预期行为

1. 默认处于 `IDLE_STREAM`
2. 串口输入 `w` 或按键唤醒后：
   - 发送 `CONTROL_WAKE`
   - 图像切到 Active 档
   - 开始持续发送音频
3. 说话期间：
   - 串口持续打印 `[VOICE] rms=... silence_ms=...`
   - 有声音时 `silence_ms` 应反复清零
4. 停止说话后：
   - 连续静音达到 `SILENCE_EXIT_SECONDS`
   - 发送 `CONTROL_SESSION_END`
   - 自动回到 Idle

### 9.2 当前默认退出条件

在当前配置下，会话退出条件为：

1. 连续静音达到 `SILENCE_EXIT_SECONDS`
2. 达到 `AUDIO_MAX_DURATION_SECONDS`
3. 达到 `SESSION_TIMEOUT_SECONDS`
4. 串口输入 `e`

---

## 10. 常见问题

### 10.1 没有串口输出

检查：

- `USB CDC On Boot` 是否为 `Enabled`
- 串口波特率是否为 `115200`
- USB 线是否为数据线

### 10.2 摄像头初始化失败

检查：

- `PSRAM` 是否启用
- 是否选择了正确的板型 `XIAO ESP32S3 Sense`

### 10.3 WiFi 连不上

检查：

- 热点是否为 `2.4GHz`
- `WIFI_SSID` / `WIFI_PASSWORD` 是否填写正确

### 10.4 收不到流

检查：

- 串口是否打印了 `IP: ...:8765`
- 电脑和眼镜是否在同一网络
- `recv_stream.py` 连接的 IP 是否正确

### 10.5 唤醒后一直不退出

检查：

- 当前环境噪声是否太大，导致始终被判定为“有声音”
- 可适当提高 `AUDIO_RMS_THRESHOLD`
- 或延长 `SILENCE_EXIT_SECONDS`

---

## 11. 建议联调顺序

建议按以下顺序验证：

1. 编译通过
2. 烧录成功
3. 串口看到启动日志
4. `recv_stream.py` 能收到 Idle 图片
5. 串口 `w` 触发连续对话
6. 串口和收包脚本都看到 `WAKE`
7. 说话并观察 `[VOICE]` 日志
8. 停止说话后自动回到 Idle

---

## 12. 相关文档

- [`glass-requirement-mvp.md`](./glass-requirement-mvp.md)
- [`glass-implementation-mvp.md`](./glass-implementation-mvp.md)
- [`transport-mvp.md`](./transport-mvp.md)
