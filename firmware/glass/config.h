#pragma once

#include <Arduino.h>
#include "esp_camera.h"

#define JG_FW_VERSION "0.1.0"

// WiFi
// 眼镜端连接的热点名称，通常填手机热点 SSID。
#define WIFI_SSID "YourPhoneHotspot"
// 眼镜端连接热点使用的密码。
#define WIFI_PASSWORD "YourPassword"

// TCP
// 眼镜端作为 TCP Server 监听的端口，recv_stream.py / 手机端都连这个端口。
#define JG_TCP_PORT 8765

// Idle capture profile
// Idle 普通采集状态下的分辨率。
#define IDLE_FRAME_SIZE FRAMESIZE_QVGA
// Idle JPEG 质量，数值越小画质越高。
#define IDLE_JPEG_QUALITY 18
// Idle 推图间隔，单位毫秒。1000 = 1fps。
#define IDLE_FPS_MS 1000UL

// Active capture profile
// 唤醒进入连续对话模式后的高档分辨率。
#define ACTIVE_FRAME_SIZE FRAMESIZE_VGA
// Active JPEG 质量，数值越小画质越高。
#define ACTIVE_JPEG_QUALITY 12
// Active 推图间隔，单位毫秒。400 = 2.5fps。
#define ACTIVE_FPS_MS 400UL
// 写入 CONTROL_WAKE payload 的帧率信息，单位是 fps x10。
#define ACTIVE_FPS_X10 25

// Audio
// 音频采样率，当前固定 16kHz。
#define AUDIO_SAMPLE_RATE 16000
// 单个采样位宽，当前固定 16bit。
#define AUDIO_BITS 16
// 每次读取/发送一块音频的时长，单位毫秒。越小实时性越高，但包会更多。
#define AUDIO_CHUNK_MS 320UL
// 连续静音多少秒后自动退出连续对话，回到 Idle。
#define SILENCE_EXIT_SECONDS 3UL
// 单次连续对话中，音频允许采集的最长时长，单位秒。
// 这是安全上限，不是主退出条件。
#define AUDIO_MAX_DURATION_SECONDS 60UL
// 一阶高通滤波系数，用来削弱直流和低频背景噪声。
#define AUDIO_HIGHPASS_ALPHA 0.95f
// 过滤后音频能量阈值。高于这个值视为“检测到有效声音”。
// 这个值需要根据实际环境做微调。
#define AUDIO_RMS_THRESHOLD 200UL
// 根据上面的秒级配置换算出来的毫秒上限，供运行时逻辑直接使用。
#define AUDIO_MAX_DURATION_MS (AUDIO_MAX_DURATION_SECONDS * 1000UL)
// 按当前采样率、位宽和 chunk 时长计算出的单块音频字节数。
#define AUDIO_CHUNK_BYTES ((AUDIO_SAMPLE_RATE * (AUDIO_BITS / 8) * AUDIO_CHUNK_MS) / 1000)

// Session
// 整个连续对话会话的最长存活时间，单位秒。
// 这是兜底超时，防止异常情况下长时间不退出。
#define SESSION_TIMEOUT_SECONDS 300UL
// 换算后的毫秒值，供运行时逻辑直接使用。
#define SESSION_TIMEOUT_MS (SESSION_TIMEOUT_SECONDS * 1000UL)
// 按键消抖时间，单位毫秒。
#define DEBOUNCE_MS 50UL
// 用作唤醒键的 GPIO，当前使用板载 Boot 键。
#define WAKE_BTN_PIN 0

// Camera
// 相机 frame buffer 数量，依赖 PSRAM。
#define CAM_FB_COUNT 2
// 摄像头 XCLK 频率。
#define CAM_XCLK_FREQ_HZ 20000000
// 连续抓帧失败达到这个次数后，触发相机重初始化。
#define CAM_REINIT_FAILURE_THRESHOLD 5

// Logging
// 串口日志波特率。
#define SERIAL_BAUD 115200
