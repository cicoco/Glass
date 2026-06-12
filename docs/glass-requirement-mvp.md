# AI 眼镜端 MVP 需求与方案：实时图流版

> **系统总览**见 [`requirement-mvp.md`](./requirement-mvp.md)。本文为眼镜端详细需求展开。

## 1. 文档定位

本文档定义 `JuneGlass` 当前阶段 **眼镜端** 的 MVP 需求边界与实现方案。

关联文档：

- [`requirement-mvp.md`](./requirement-mvp.md) — 系统一体化 MVP 总览（首选入口）
- [`phone-requirement-mvp.md`](./phone-requirement-mvp.md) — 手机端详细需求
- [`transport-mvp.md`](./transport-mvp.md) — 传输层双端规格
- [`glass-implementation-mvp.md`](./glass-implementation-mvp.md) — 编码实现规格（引脚、任务、默认值）

本文档聚焦：

- 眼镜端持续采集与推流
- 唤醒后会话升档（图 + 音双流）
- WiFi 传输与状态机

本文档不覆盖：

- 手机端 STT、选帧、MiniCPM、TTS
- 本地唤醒词（本期用按键/串口模拟）
- BLE 日常传输与 BLE/WiFi 切换
- 找物、历史记忆等第二阶段能力

---

## 2. MVP 核心目标

眼镜端第一版只解决一个问题：

**在稳定、低功耗的前提下，持续向手机回传 JPEG 图流；用户唤醒后，升档同时回传图片与音频，供手机完成单轮实时图分析。**

### 2.1 成功标准

- Idle 档可持续推图，连续运行 10 分钟无明显掉流、重启、内存泄漏
- 唤醒后会话可完成一次完整交互：升档图流 + 音频推流 + 正常回落 Idle
- 手机端（或调试脚本）可按协议正确拆包 IMAGE / AUDIO / CONTROL
- 采集链路不在眼镜端执行任何深度学习推理

---

## 3. 硬件与平台

| 项 | 选型 |
|----|------|
| 开发板 | Seeed XIAO ESP32-S3 Sense |
| 摄像头 | 板载 OV2640 |
| 麦克风 | 板载 PDM 数字麦 |
| 无线 | WiFi（本期唯一传输通道） |
| 框架 | Arduino（本期）；验证通过后迁移 ESP-IDF |
| 内存 | 启用 PSRAM，`fb_count = 2` |

---

## 4. 总体架构

### 4.1 职责边界

| 端 | 职责 |
|----|------|
| 眼镜端 | 采图、采音、本地唤醒门控（本期模拟）、编码、推流、会话状态机 |
| 手机端 | 收流、帧缓存、STT、有效提问检测、选帧、MiniCPM、TTS |

### 4.2 产品闭环（系统级）

```text
眼镜持续采图 → 用户唤醒 → 眼镜升档传图+音 → 手机 STT/选帧/MiniCPM/TTS
```

### 4.3 传输分层（本期 vs 下期）

| 阶段 | 日常（Idle） | 唤醒后（Active） |
|------|--------------|------------------|
| **本期** | WiFi，仅 IMAGE 低档 | WiFi，IMAGE 升档 + AUDIO |
| **下期** | BLE，仅 IMAGE 低档 | 切换 WiFi，IMAGE 升档 + AUDIO |

本期固定 WiFi，协议与状态机按「最终双通道」设计，避免后期推翻重来。

### 4.4 状态机

```text
IDLE_STREAM ──(唤醒触发)──► SESSION_ACTIVE ──(结束/超时)──► IDLE_STREAM
```

| 状态 | 行为 |
|------|------|
| `IDLE_STREAM` | QVGA 1fps 持续推 JPEG，不传音频 |
| `SESSION_ACTIVE` | VGA 2.5fps 推 JPEG + 16kHz PCM；会话结束后发 `SESSION_END` |

升档流程：**先停采 → 修改分辨率/质量 → 再开采**。允许约 0.3–0.5s 图流间隙，手机端用唤醒前缓存帧兜底。

---

## 5. 采集参数

### 5.1 档位定义

| 档位 | 分辨率 | JPEG quality¹ | 帧率 | 音频 |
|------|--------|---------------|------|------|
| **Idle** | QVGA 320×240 | 15–20 | 1 fps | 不传 |
| **Active** | VGA 640×480 | 10–12 | 2.5 fps（2–3 可调） | 16 kHz / 16 bit / mono PCM |

¹ OV2640 `quality` 数值越小画质越高。

### 5.2 编码与缓冲

- `fb_count = 2`（依赖 PSRAM）
- `grab_mode = LATEST`：丢弃积压帧，避免延迟滚雪球
- 帧率由定时器/Task 控制，禁止无节流连续抓拍

### 5.3 音频（Active 会话）

- 格式：PCM signed 16-bit little-endian，单声道，16 kHz
- 时长：默认推流 3–5 秒，或简单能量 VAD 检测到句末后停止
- 会话超时：8 秒无有效语音活动则自动结束（实现可微调 ±2s）

---

## 6. 唤醒与触发（本期）

### 6.1 本期实现

| 方式 | 用途 |
|------|------|
| 板载用户按键 | 模拟唤醒，边沿触发 + 50ms 消抖 |
| 串口命令 | `w` 触发唤醒，`e` 强制结束会话；与按键共用同一入口 |

### 6.2 下期实现

- 眼镜端本地唤醒词检测（ESP-SR / microWakeWord 等）
- 按键与串口保留为调试兜底

### 6.3 与手机端的协作

- 眼镜端只负责「开门」：发 `CONTROL_WAKE` 并升档推流
- **有效提问检测**仍在手机端（规则匹配「这是什么」「帮我看看」等）
- `CONTROL_WAKE` 必须携带 `timestamp_ms`，供手机在唤醒窗口内优先选帧

---

## 7. 传输方案

本期传输层见 [`transport-mvp.md`](./transport-mvp.md)。

| 项 | 约定 |
|----|------|
| 角色 | 眼镜 = TCP Server，手机/脚本 = TCP Client |
| 端口 | `8765` |
| 连接 | 单 TCP 连接混传 IMAGE / AUDIO / CONTROL |
| WiFi 模式 | STA，连接手机热点 |
| 发现 | 串口打印 IP；mDNS（`juneglass.local`）可选 |

---

## 8. 本期必须实现（In Scope）

1. OV2640 初始化与 JPEG 采集（Idle / Active 两档）
2. PDM 麦克风采集与 Active 会话音频推流
3. WiFi STA 联网与 TCP Server
4. 二进制帧协议封包与发送
5. `IDLE_STREAM` ↔ `SESSION_ACTIVE` 状态机
6. 板载按键 + 串口命令触发唤醒/结束
7. 串口日志：WiFi 状态、IP、内存、当前档位、会话 ID

---

## 9. 本期明确不做（Out of Scope）

| 能力 | 归属 |
|------|------|
| 本地唤醒词 | 眼镜端下期 |
| BLE 传输 | 眼镜端下期 |
| BLE → WiFi 切换 | 眼镜端下期 |
| STT | 手机端 |
| 有效提问语义判断 | 手机端 |
| 最近帧缓存 / 选帧 / 清晰度 | 手机端 |
| MiniCPM-V 推理 | 手机端 |
| TTS | 手机端 |
| 找物 / 历史 / 记忆 | 第二阶段 |

---

## 10. 交付形态

| 项 | 说明 |
|----|------|
| 代码 | 一个 Arduino 工程，一期交付完整 Idle + Active 能力 |
| 文档 | 本文档 + [`transport-mvp.md`](./transport-mvp.md) + [`glass-implementation-mvp.md`](./glass-implementation-mvp.md) |
| 配置 | 关键参数（WiFi SSID/密码、端口、帧率）可通过头文件或编译宏调整 |

---

## 11. 验收标准

### 11.1 单机自检（不依赖手机 App）

1. 串口可见 WiFi 连接成功、IP、剩余堆/PSRAM
2. TCP Client 可持续收到 Idle 档 `IMAGE` 帧 ≥ 5 分钟
3. 按键或串口 `w` 后：收到 `CONTROL_WAKE` → 升档 `IMAGE` + `AUDIO` → `CONTROL_SESSION_END`
4. 会话结束后自动回落 Idle 档，图流恢复
5. 连续触发 10 次会话无崩溃、无内存单调下降

### 11.2 联调验收（手机端就绪后）

1. 手机采集线程在 Idle 期间持续更新帧缓存
2. 收到 `CONTROL_WAKE` 后，STT 可识别用户问句
3. 手机可选到唤醒窗口内足够清晰的 JPEG 帧
4. 分析链路不导致眼镜端推流中断

---

## 12. 风险与假设

| 风险 | 说明 | 缓解 |
|------|------|------|
| 摄像头视角 | 板载镜头与佩戴视野可能不一致 | 结构件阶段再评估；本期以协议打通为主 |
| 功耗发热 | 持续采图 + WiFi | Idle 压至 1fps；实机观察温升 |
| 无唤醒词 | 本期按键代替，体验与最终产品有差距 | 不阻塞协议与手机联调 |
| 升档间隙 | 切换分辨率时短暂停流 | 协议带 timestamp；手机用缓存帧兜底 |
| 手机端未就绪 | 无法端到端验收 | 本期以协议文档 + 脚本收包为准 |

---

## 13. 下期扩展（备忘，不进入本期实现）

1. 本地唤醒词，替换按键为正式触发
2. Idle 档改走 BLE（QVGA 0.5–1fps）
3. 唤醒后 BLE → WiFi 无缝切换
4. 迁移 ESP-IDF，优化内存、任务调度与长期稳定性
5. `CONTROL_WAKE` 扩展字段：传输层切换通知、升档参数确认

---

## 14. 最终结论

眼镜端本期 MVP 定义为：

**Idle 档 WiFi 持续推 JPEG → 唤醒后升档同时推 JPEG + PCM → 会话结束回落 Idle**

设计重点：

- 采集稳定
- 协议清晰
- 状态机可靠
- 为下期 BLE / 唤醒词 / ESP-IDF 预留扩展空间

不得在本期混入手机端推理逻辑、找物或历史记忆等第二阶段能力。
