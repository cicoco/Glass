# JuneGlass 眼镜端 MVP 实现规格

> **需求**见 [`glass-requirement-mvp.md`](./glass-requirement-mvp.md)，**传输契约**见 [`transport-mvp.md`](./transport-mvp.md)。本文为 CodeDevAgent 编码落地的实现细节。

---

## 1. 文档定位

本文档将需求中的范围、参数、协议**收敛为可编码的默认值与工程结构**，解决以下问题：

- 用什么开发环境、库、板型
- 引脚与驱动怎么配
- 固件分几个任务/模块
- 会话结束、升档、发送、静音退出等行为的具体判定逻辑
- 目录结构与自检步骤

---

## 2. 开发环境

### 2.1 工具链

| 项 | 本期约定 |
|----|----------|
| IDE | Arduino IDE 2.x 或 PlatformIO |
| 板型 | **Seeed XIAO ESP32S3 Sense** |
| 核心包 | `esp32` by Espressif **≥ 3.0.0**（首选） |
| PSRAM | **OPI PSRAM Enabled**（必须） |
| USB CDC On Boot | **Enabled**（否则 Serial 可能无输出） |
| Partition Scheme | 默认即可；若固件偏大可选 *Huge APP* |

### 2.2 依赖

| 库 | 用途 | 来源 |
|----|------|------|
| `esp_camera` | OV2640 JPEG 采集 | esp32 核心包内置 |
| `ESP_I2S` | PDM 麦克风 | esp32 核心包内置（3.x） |
| `WiFi` | STA 联网 | esp32 核心包内置 |

> **esp32 2.x 兼容**：麦克风 API 为旧版 `I2S.h` + `PDM_MONO_MODE`；本期以 **3.x + ESP_I2S** 为准，2.x 仅作迁移备注。

### 2.3 参考示例（官方）

编码时优先对齐 Seeed Wiki 示例，避免引脚/dev 配置漂移：

- 摄像头：[XIAO ESP32S3 Camera Usage](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/)
- 麦克风：[XIAO ESP32S3 Sense Mic](https://wiki.seeedstudio.com/xiao_esp32s3_sense_mic/)
- 引脚：[XIAO ESP32S3 Pin Multiplexing](https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/)

---

## 3. 工程结构

```text
firmware/glass/
├── glass.ino          # setup/loop 入口，任务启动
├── config.h                     # WiFi、端口、帧率等常量
├── camera_pins.h                # 可直接复用 esp32-camera 的 XIAO ESP32S3 定义
├── app/
│   ├── app_state.h              # 状态机枚举与事件
│   ├── session_controller.cpp   # 唤醒/结束/超时逻辑
│   └── input_handler.cpp        # 按键 + 串口命令
├── capture/
│   ├── camera_capture.cpp       # 初始化、抓帧、升档切换
│   └── audio_capture.cpp        # PDM 采集、分块读取
├── transport/
│   ├── frame_protocol.cpp       # JG 帧封包
│   ├── tcp_server.cpp           # WiFi STA + TCP Server
│   └── stream_sender.cpp        # 发送队列、非阻塞发送
└── tools/
    └── recv_stream.py           # 从 transport-mvp.md 同步的收包脚本
```

---

## 4. 硬件引脚与约束

### 4.1 引脚表（Sense 扩展板）

| 功能 | GPIO | 说明 |
|------|------|------|
| **PDM CLK** | 42 | 麦克风时钟，勿挪作他用 |
| **PDM DATA** | 41 | 麦克风数据，勿挪作他用 |
| **Boot 按键** | 0 | 板载 Boot，本期作唤醒键；`INPUT_PULLUP`，按下为 `LOW` |
| **User LED** | 21 | 可选状态指示（Idle 慢闪 / Active 常亮） |

### 4.2 摄像头引脚

**不要手写引脚**。在工程中定义：

```c
#define CAMERA_MODEL_XIAO_ESP32S3
#include "camera_pins.h"   // 来自 esp32-camera/examples 或工程内副本
```

`camera_pins.h` 对应关系（与 Seeed / esp32-camera 一致，供查阅）：

| 信号 | GPIO |
|------|------|
| XCLK | 10 |
| SIOD / SIOC | 40 / 39 |
| Y2–Y9 | 15, 17, 18, 16, 14, 12, 11, 48 |
| VSYNC / HREF / PCLK | 38 / 47 / 13 |
| PWDN / RESET | -1 / -1（不使用） |

### 4.3 硬件约束

| 约束 | 说明 |
|------|------|
| 摄像头与 PDM 共存 | 默认 J1/J2 未切断时，麦与 D11/D12 互斥；本期用 PDM，**不要占用 GPIO 41/42** |
| SD 卡 | 本期不使用；GPIO21 作 SD CS，仅在使用 User LED 时注意冲突 |
| 供电 | 持续 WiFi + 摄像头建议 USB 供电；电池供电需关注压降与发热 |

---

## 5. 配置常量（拍板默认值）

在 `config.h` 中集中定义：

```c
// --- WiFi ---
#define WIFI_SSID           "YourPhoneHotspot"
#define WIFI_PASSWORD       "YourPassword"

// --- TCP ---
#define JG_TCP_PORT         8765

// --- Idle 档 ---
#define IDLE_FRAME_SIZE     FRAMESIZE_QVGA      // 320x240
#define IDLE_JPEG_QUALITY   18                  // 范围 15-20
#define IDLE_FPS_MS         1000                // 1 fps

// --- Active 档 ---
#define ACTIVE_FRAME_SIZE   FRAMESIZE_VGA       // 640x480
#define ACTIVE_JPEG_QUALITY 12                  // 范围 10-12
#define ACTIVE_FPS_MS       400                 // 2.5 fps
#define ACTIVE_FPS_X10      25                  // 写入 WAKE payload

// --- 音频 ---
#define AUDIO_SAMPLE_RATE            16000
#define AUDIO_BITS                   16
#define AUDIO_CHUNK_MS               320        // 每块 320ms ≈ 10240 字节
#define SILENCE_EXIT_SECONDS         3          // 连续静音 3 秒退出
#define AUDIO_MAX_DURATION_SECONDS   180        // 默认 3 分钟，可调
#define AUDIO_HIGHPASS_ALPHA         0.95f      // 一阶高通默认系数
#define AUDIO_RMS_THRESHOLD          200        // 过滤后能量阈值，需实机校准

// --- 会话 ---
#define SESSION_TIMEOUT_SECONDS      300        // 默认 5 分钟兜底超时
#define DEBOUNCE_MS         50
#define WAKE_BTN_PIN        0                   // Boot 键

// --- 相机缓冲 ---
#define CAM_FB_COUNT        2
#define CAM_XCLK_FREQ_HZ    20000000
```

---

## 6. 摄像头实现

### 6.1 初始化模板

```c
camera_config_t cfg = {};
cfg.ledc_channel = LEDC_CHANNEL_0;
cfg.ledc_timer   = LEDC_TIMER_0;
cfg.pin_d0       = Y2_GPIO_NUM;
// ... 其余引脚同 camera_pins.h ...
cfg.xclk_freq_hz = CAM_XCLK_FREQ_HZ;
cfg.frame_size   = IDLE_FRAME_SIZE;
cfg.pixel_format = PIXFORMAT_JPEG;
cfg.grab_mode    = CAMERA_GRAB_LATEST;
cfg.fb_location  = CAMERA_FB_IN_PSRAM;
cfg.jpeg_quality = IDLE_JPEG_QUALITY;
cfg.fb_count     = CAM_FB_COUNT;
esp_camera_init(&cfg);
```

### 6.2 抓帧流程

```text
esp_camera_fb_get()
  → 拷贝 JPEG 到发送缓冲（或零拷贝引用，发送完再 return）
  → esp_camera_fb_return(fb)
```

- 抓帧失败：记录计数，跳过本周期，不重启相机
- 连续失败 ≥ 5 次：执行 `camera_reinit()`（deinit → delay 100ms → init）

### 6.3 升档切换（必须按序）

```text
1. 暂停采图任务（或置 switching 标志）
2. esp_camera_deinit()
3. 修改 frame_size / jpeg_quality
4. esp_camera_init()
5. delay(100) 等待传感器稳定
6. 恢复采图；首帧 timestamp 写入 CONTROL_WAKE 之后
```

升档期间允许 0.3–0.5s 无 IMAGE，符合 [`transport-mvp.md`](./transport-mvp.md)。

### 6.4 Active 档保持策略

- 进入 `SESSION_ACTIVE` 后，图像档位保持 `ACTIVE_FRAME_SIZE + ACTIVE_FPS_MS`
- **不要** 因为短暂停顿或瞬时静音回落到 Idle 档
- 只有在整段连续对话真正结束时，才执行降档回到 Idle

---

## 7. 音频实现

### 7.1 初始化（esp32 3.x）

```c
#include "ESP_I2S.h"
I2SClass i2s;

i2s.setPinsPdmRx(42, 41);  // CLK, DATA
i2s.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
```

### 7.2 采集、过滤与分块

| 项 | 约定 |
|----|------|
| 启停 | **仅** `SESSION_ACTIVE` 期间 `i2s.begin()`；会话结束 `i2s.end()` |
| 分块大小 | `16000 * 2 * AUDIO_CHUNK_MS / 1000` = **10240 字节** |
| 发送 | 每块封装为 `AUDIO` 帧；末块 `flags \|= AUDIO_LAST` |
| 缓冲 | 块缓冲放 PSRAM 或堆，单块 ~10KB |
| 过滤 | 原始 PCM 先做基础高通/降噪，再进入能量检测与上送链路 |

### 7.3 静音退出策略（本期必须实现）

- 过滤后音频按 chunk 计算 RMS 或等效能量
- 当 chunk 能量高于 `AUDIO_RMS_THRESHOLD` 时，视为“检测到有效声音”
- 当能量低于阈值时，累计静音时长
- 连续静音达到 `SILENCE_EXIT_SECONDS` 后，触发退出会话
- `AUDIO_MAX_DURATION_SECONDS` 和 `SESSION_TIMEOUT_SECONDS` 仅作安全保护，不作为主退出路径

### 7.4 音频上限与会话超时

- `AUDIO_MAX_DURATION_SECONDS`：音频采集允许的最长时长，分钟级可配置
- `SESSION_TIMEOUT_SECONDS`：整个连续对话会话的兜底超时，分钟级可配置
- 两者统一用“秒”配置，内部实现时再转换为毫秒
- 推荐关系：`SESSION_TIMEOUT_SECONDS >= AUDIO_MAX_DURATION_SECONDS`

---

## 8. 任务架构

### 8.1 三任务模型（FreeRTOS）

```text
┌─────────────────┐     队列      ┌─────────────────┐
│  TaskCapture    │ ────────────► │  TaskStream     │ ──► TCP
│  相机 + 音频采集  │   FrameJob    │  封包 + 发送     │
└─────────────────┘               └─────────────────┘
         ▲                                   ▲
         │ 状态/档位                           │ client_fd
┌─────────────────┐                           │
│  TaskSession    │ ──────────────────────────┘
│  按键/串口/超时  │
└─────────────────┘
```

| 任务 | 优先级 | 栈 | 周期/触发 | 职责 |
|------|--------|-----|-----------|------|
| `TaskCapture` | 2 | 8KB | Idle 1000ms / Active 400ms | 抓 JPEG；Active 时读音频块 |
| `TaskStream` | 2 | 6KB | 队列事件驱动 | `send_frame()`；发送失败丢帧 |
| `TaskSession` | 1 | 4KB | 20ms 轮询 | 按键消抖、串口、静音退出、`SESSION_TIMEOUT` |
| `loop()` | — | — | 非阻塞 | 仅 `vTaskDelay` 或空；逻辑不进 loop |

主线程 `setup()`：Serial → WiFi → Camera → TCP Server → 创建三任务 → 进入 Idle。

### 8.2 任务间通信

| 机制 | 用途 |
|------|------|
| `QueueHandle_t frame_queue` | Capture → Stream，元素为 `FrameJob{type, buf, len, ts, sid, flags}` |
| `volatile AppState` | Session 任务写，Capture/Stream 读 |
| `EventGroup`（可选） | `SWITCHING_CAMERA` 标志，升档时 Capture 暂停 |
| `silence_accumulator_ms` | Audio/Session 共享，记录连续静音时长 |

### 8.3 发送非阻塞

```c
// send_all: 循环 write；若 EAGAIN/超时，返回 false
// Stream 任务：false → 释放本帧，drop_count++，不阻塞 Capture
```

---

## 9. 状态机实现

### 9.1 状态定义

```c
typedef enum {
  APP_IDLE_STREAM,
  APP_SESSION_ACTIVE,
  APP_CAMERA_SWITCHING   // 升档/降档中间态
} AppState;
```

### 9.2 事件与转移

| 事件 | 当前状态 | 动作 | 下一状态 |
|------|----------|------|----------|
| `EVT_WAKE`（按键/串口 w） | IDLE | `session_id++`；发 `CONTROL_WAKE`；升档相机；`i2s.begin()`；记录 `session_start_ms` | ACTIVE |
| `EVT_SILENCE_EXIT` | ACTIVE | 发 `CONTROL_SESSION_END`；`i2s.end()`；降档相机 | IDLE |
| `EVT_TIMEOUT` | ACTIVE | 发 `CONTROL_SESSION_END`；`i2s.end()`；降档相机 | IDLE |
| `EVT_FORCE_END`（串口 e） | ACTIVE | 同 TIMEOUT | IDLE |
| `EVT_AUDIO_MAX` | ACTIVE | 标记最后一包音频 → 发 END | IDLE |

### 9.3 会话结束优先级（拍板）

```text
1. 串口 e / 按键长按（可选）              → 立即 EVT_FORCE_END
2. 连续静音达到 SILENCE_EXIT_SECONDS      → EVT_SILENCE_EXIT
3. AUDIO_MAX_DURATION_SECONDS 达上限      → EVT_AUDIO_MAX
4. SESSION_TIMEOUT_SECONDS 达上限         → EVT_TIMEOUT
```

收到 `EVT_WAKE` 后**只处理一次**，防抖期间忽略重复触发。

### 9.4 session_id 与帧发送

| 状态 | session_id | 发送内容 |
|------|------------|----------|
| IDLE | 0 | 仅 IMAGE，`flags=STREAM_IDLE` |
| ACTIVE | 递增后的 sid | WAKE → IMAGE + AUDIO → END → IMAGE(sid=0) |

`CONTROL_WAKE` 的 `timestamp_ms` 取 **`millis()` 在发送前的时刻**，与需求一致。

---

## 10. 传输层实现要点

实现须严格遵循 [`transport-mvp.md`](./transport-mvp.md)，眼镜端额外约定：

| 项 | 实现 |
|----|------|
| TCP Server | `WiFiServer`，`listen(8765)`；新 Client 接入时 `stop()` 旧 Client |
| 无 Client | Capture 照常；Stream 从队列取帧但丢弃发送（或不入队，推荐后者省 CPU） |
| 发送顺序 | 同 session：`WAKE` →（IMAGE/AUDIO 持续交错）→ `AUDIO_LAST` → `SESSION_END` |
| `send_frame` | 小端写入 14 字节头；`payload_len==0` 时只发头 |
| WAKE payload | 8 字节：`{1, 0, 640, 480, 25}` 按小端写入 |

---

## 11. 输入与调试

### 11.1 按键

```c
// Boot GPIO0, INPUT_PULLUP
// 下降沿触发 EVT_WAKE；仅在 APP_IDLE_STREAM 有效
// 50ms debounce
```

### 11.2 串口（115200）

| 命令 | 动作 |
|------|------|
| `w` | 触发 `EVT_WAKE` |
| `e` | 触发 `EVT_FORCE_END` |
| `s` | 打印状态：AppState、sid、堆/PSRAM、drop_count、WiFi RSSI |
| `i` | 打印 IP 与端口 |

### 11.3 日志规范

上电必打：

```text
[BOOT] JuneGlass Glass FW x.y.z
[WIFI] connecting...
[WIFI] IP: 192.168.x.x:8765
[CAM] idle qvga q=18
[TCP] listening
```

会话关键节点：`[WAKE] sid=1 ts=...`、`[VOICE] rms=... silence_ms=...`、`[END] sid=1 reason=silence`、`[DROP] image count=...`

---

## 12. 烧录与自检

### 12.1 烧录步骤

1. 选择板型 **XIAO ESP32S3 Sense**
2. 打开 **PSRAM**、**USB CDC On Boot**
3. 修改 `config.h` 中 `WIFI_SSID` / `WIFI_PASSWORD`
4. 编译上传；打开 Serial Monitor **115200**
5. 确认打印 IP；手机开热点

### 12.2 单机验收（对照 glass-requirement-mvp §11.1）

| # | 步骤 | 通过标准 |
|---|------|----------|
| 1 | 看串口 | WiFi 成功、IP、PSRAM free > 2MB |
| 2 | `python3 tools/recv_stream.py <ip>` | 持续收到 IMAGE，≥5 分钟 |
| 3 | 按 Boot 或串口 `w` | 顺序：WAKE → IMAGE(大) + AUDIO → END → IMAGE(sid=0) |
| 4 | 串口 `e` 中断会话 | 能强制 END 并回落 Idle |
| 5 | 连续唤醒 10 次 | 无重启；`ESP.getFreeHeap()` 无单调下降 |

### 12.3 常见问题

| 现象 | 排查 |
|------|------|
| 摄像头初始化失败 | PSRAM 未开；引脚 config 错误 |
| 无 Serial | USB CDC On Boot 未开 |
| 麦克风无数据 | GPIO 41/42 被占用；J1/J2 被切断 |
| WiFi 连不上 | 热点 2.4GHz；SSID/密码；地区信道 |
| JPEG 花屏 | 升档未 deinit；降低 Active 至 SVGA 试验 |
| 频繁重启 | 看栈水位；增大 Task 栈；查 WDT |

---

## 13. 模块接口草案

便于 AI 分文件生成时对齐：

```c
// app_state.h
AppState app_get_state();
uint16_t app_get_session_id();
void app_post_event(AppEvent evt);

// camera_capture.h
bool camera_init_idle();
bool camera_switch_active();
bool camera_switch_idle();
bool camera_capture_jpeg(uint8_t **out, size_t *len, uint32_t *ts);

// audio_capture.h
bool audio_start();
bool audio_read_chunk(uint8_t *buf, size_t *len, bool *is_last);
void audio_stop();

// frame_protocol.h
bool frame_send(int fd, uint8_t type, uint8_t flags,
                uint32_t ts, uint16_t sid,
                const uint8_t *payload, uint32_t len);

// tcp_server.h
bool net_init_sta();
int  net_accept_client_blocking();  // 无 client 返回 -1
```

---

## 14. 扩展预留（本期不实现）

| 扩展点 | 预留方式 |
|--------|----------|
| 本地唤醒词 | `input_handler` 增加 `EVT_WAKE` 源 |
| BLE 传输 | `stream_sender` 抽象 `TransportSink` 接口 |
| ESP-IDF | 模块边界与本文档任务划分保持一致 |
| mDNS | `net_init_sta()` 后 `MDNS.addService("jg", "tcp", 8765)` |

---

## 15. 文档索引

```text
glass-implementation-mvp.md   ← 本文档（编码落地）
glass-requirement-mvp.md      需求与验收
glass-flash-and-debug.md      Arduino IDE 烧录与联调步骤
transport-mvp.md              传输契约
requirement-mvp.md            系统总览
```
