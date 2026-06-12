#pragma once

#include <Arduino.h>
#include "esp_camera.h"

#define JG_FW_VERSION "0.1.0"

// WiFi
#define WIFI_SSID "YourPhoneHotspot"
#define WIFI_PASSWORD "YourPassword"

// TCP
#define JG_TCP_PORT 8765

// Idle capture profile
#define IDLE_FRAME_SIZE FRAMESIZE_QVGA
#define IDLE_JPEG_QUALITY 18
#define IDLE_FPS_MS 1000UL

// Active capture profile
#define ACTIVE_FRAME_SIZE FRAMESIZE_VGA
#define ACTIVE_JPEG_QUALITY 12
#define ACTIVE_FPS_MS 400UL
#define ACTIVE_FPS_X10 25

// Audio
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BITS 16
#define AUDIO_CHUNK_MS 320UL
#define AUDIO_MAX_DURATION_MS 5000UL
#define AUDIO_CHUNK_BYTES ((AUDIO_SAMPLE_RATE * (AUDIO_BITS / 8) * AUDIO_CHUNK_MS) / 1000)

// Session
#define SESSION_TIMEOUT_MS 8000UL
#define DEBOUNCE_MS 50UL
#define WAKE_BTN_PIN 0

// Camera
#define CAM_FB_COUNT 2
#define CAM_XCLK_FREQ_HZ 20000000
#define CAM_REINIT_FAILURE_THRESHOLD 5

// Logging
#define SERIAL_BAUD 115200

