#pragma once

#include <Arduino.h>
#include "esp_camera.h"

enum CaptureProfile : uint8_t {
  CAPTURE_PROFILE_IDLE = 0,
  CAPTURE_PROFILE_ACTIVE = 1,
};

struct JpegFrame {
  camera_fb_t *fb;
  const uint8_t *data;
  size_t len;
  uint32_t timestamp_ms;
};

bool camera_capture_begin_idle(Stream &log);
bool camera_capture_switch_profile(CaptureProfile profile, Stream &log);
CaptureProfile camera_capture_profile();
bool camera_capture_get_jpeg(JpegFrame *frame, Stream &log);
void camera_capture_release(const JpegFrame *frame);

