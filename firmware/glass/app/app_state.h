#pragma once

#include <Arduino.h>

enum AppState : uint8_t {
  APP_IDLE_STREAM = 0,
  APP_SESSION_ACTIVE = 1,
  APP_CAMERA_SWITCHING = 2,
};

enum AppEndReason : uint8_t {
  APP_END_REASON_NONE = 0,
  APP_END_REASON_AUDIO_MAX = 1,
  APP_END_REASON_TIMEOUT = 2,
  APP_END_REASON_FORCED = 3,
};

struct InputCommandFlags {
  bool wake_requested;
  bool force_end_requested;
  bool print_status_requested;
  bool print_network_requested;
};

