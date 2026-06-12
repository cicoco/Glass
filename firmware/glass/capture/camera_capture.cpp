#include "camera_capture.h"

#include "esp_camera.h"

#include "../camera_pins.h"
#include "../config.h"

namespace {
CaptureProfile g_profile = CAPTURE_PROFILE_IDLE;
bool g_initialized = false;
uint8_t g_failure_count = 0;

camera_config_t build_config(CaptureProfile profile) {
  camera_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));

  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = Y2_GPIO_NUM;
  cfg.pin_d1 = Y3_GPIO_NUM;
  cfg.pin_d2 = Y4_GPIO_NUM;
  cfg.pin_d3 = Y5_GPIO_NUM;
  cfg.pin_d4 = Y6_GPIO_NUM;
  cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM;
  cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk = XCLK_GPIO_NUM;
  cfg.pin_pclk = PCLK_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM;
  cfg.pin_href = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn = PWDN_GPIO_NUM;
  cfg.pin_reset = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = CAM_XCLK_FREQ_HZ;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = profile == CAPTURE_PROFILE_IDLE ? IDLE_FRAME_SIZE : ACTIVE_FRAME_SIZE;
  cfg.jpeg_quality = profile == CAPTURE_PROFILE_IDLE ? IDLE_JPEG_QUALITY : ACTIVE_JPEG_QUALITY;
  cfg.fb_count = CAM_FB_COUNT;
  cfg.grab_mode = CAMERA_GRAB_LATEST;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  return cfg;
}

bool init_profile(CaptureProfile profile, Stream &log) {
  camera_config_t cfg = build_config(profile);
  const esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    log.print(F("[CAM] init failed: 0x"));
    log.println(err, HEX);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_framesize(sensor, cfg.frame_size);
    sensor->set_quality(sensor, cfg.jpeg_quality);
  }

  g_profile = profile;
  g_initialized = true;
  g_failure_count = 0;
  log.print(F("[CAM] "));
  if (profile == CAPTURE_PROFILE_IDLE) {
    log.println(F("idle qvga q=18"));
  } else {
    log.println(F("active vga q=12"));
  }
  return true;
}
}  // namespace

bool camera_capture_begin_idle(Stream &log) {
  return init_profile(CAPTURE_PROFILE_IDLE, log);
}

bool camera_capture_switch_profile(CaptureProfile profile, Stream &log) {
  if (g_initialized) {
    esp_camera_deinit();
    g_initialized = false;
    delay(100);
  }
  if (!init_profile(profile, log)) {
    return false;
  }
  delay(100);
  return true;
}

CaptureProfile camera_capture_profile() {
  return g_profile;
}

bool camera_capture_get_jpeg(JpegFrame *frame, Stream &log) {
  if (!g_initialized || frame == nullptr) {
    return false;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (fb == nullptr) {
    ++g_failure_count;
    log.print(F("[CAM] capture failed count="));
    log.println(g_failure_count);
    if (g_failure_count >= CAM_REINIT_FAILURE_THRESHOLD) {
      log.println(F("[CAM] reinit after repeated capture failures"));
      camera_capture_switch_profile(g_profile, log);
    }
    return false;
  }

  g_failure_count = 0;
  frame->fb = fb;
  frame->data = fb->buf;
  frame->len = fb->len;
  frame->timestamp_ms = millis();
  return true;
}

void camera_capture_release(const JpegFrame *frame) {
  if (frame != nullptr && frame->fb != nullptr) {
    esp_camera_fb_return(frame->fb);
  }
}
