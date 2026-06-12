#include "session_controller.h"

#include "../capture/audio_capture.h"
#include "../capture/camera_capture.h"
#include "../transport/frame_protocol.h"
#include "../transport/tcp_server.h"
#include "../config.h"

namespace {
AppState g_state = APP_IDLE_STREAM;
uint16_t g_session_counter = 0;
uint16_t g_active_session_id = 0;
uint32_t g_last_wake_ts = 0;
uint32_t g_session_started_ms = 0;
AppEndReason g_last_end_reason = APP_END_REASON_NONE;

bool send_control_wake() {
  uint8_t payload[8];
  frame_protocol_build_wake_payload(payload);
  JGFrameHeader header{};
  header.type = JG_FRAME_CONTROL_WAKE;
  header.flags = 0;
  header.timestamp_ms = g_last_wake_ts;
  header.session_id = g_active_session_id;
  header.payload_len = sizeof(payload);
  return tcp_server_send_frame(header, payload);
}

bool send_control_end(uint32_t ts) {
  JGFrameHeader header{};
  header.type = JG_FRAME_CONTROL_SESSION_END;
  header.flags = 0;
  header.timestamp_ms = ts;
  header.session_id = g_active_session_id;
  header.payload_len = 0;
  return tcp_server_send_frame(header, nullptr);
}
}  // namespace

void session_controller_begin() {
  g_state = APP_IDLE_STREAM;
  g_session_counter = 0;
  g_active_session_id = 0;
  g_last_wake_ts = 0;
  g_session_started_ms = 0;
  g_last_end_reason = APP_END_REASON_NONE;
}

bool session_controller_request_wake(Stream &log) {
  if (g_state != APP_IDLE_STREAM) {
    return false;
  }

  g_state = APP_CAMERA_SWITCHING;
  ++g_session_counter;
  if (g_session_counter == 0) {
    ++g_session_counter;
  }
  g_active_session_id = g_session_counter;
  g_last_wake_ts = millis();
  g_session_started_ms = g_last_wake_ts;

  send_control_wake();
  log.print(F("[WAKE] sid="));
  log.print(g_active_session_id);
  log.print(F(" ts="));
  log.println(g_last_wake_ts);

  if (!camera_capture_switch_profile(CAPTURE_PROFILE_ACTIVE, log)) {
    log.println(F("[WAKE] camera switch failed"));
    camera_capture_switch_profile(CAPTURE_PROFILE_IDLE, log);
    g_state = APP_IDLE_STREAM;
    g_active_session_id = 0;
    return false;
  }

  if (!audio_capture_start(log)) {
    log.println(F("[WAKE] audio start failed"));
    camera_capture_switch_profile(CAPTURE_PROFILE_IDLE, log);
    g_state = APP_IDLE_STREAM;
    g_active_session_id = 0;
    return false;
  }

  g_state = APP_SESSION_ACTIVE;
  return true;
}

bool session_controller_request_end(AppEndReason reason, Stream &log) {
  if (g_state != APP_SESSION_ACTIVE) {
    return false;
  }

  g_state = APP_CAMERA_SWITCHING;
  const uint16_t finished_session = g_active_session_id;
  const uint32_t ts = millis();
  send_control_end(ts);
  audio_capture_stop();
  camera_capture_switch_profile(CAPTURE_PROFILE_IDLE, log);

  g_last_end_reason = reason;
  g_state = APP_IDLE_STREAM;
  g_active_session_id = 0;
  log.print(F("[END] sid="));
  log.print(finished_session);
  log.print(F(" reason="));
  log.println(session_controller_end_reason_name(reason));
  return true;
}

void session_controller_tick(Stream &log) {
  if (g_state == APP_SESSION_ACTIVE &&
      (millis() - g_session_started_ms) >= SESSION_TIMEOUT_MS) {
    session_controller_request_end(APP_END_REASON_TIMEOUT, log);
  }
}

AppState session_controller_state() {
  return g_state;
}

uint16_t session_controller_active_session_id() {
  return g_active_session_id;
}

uint16_t session_controller_frame_session_id() {
  return g_state == APP_SESSION_ACTIVE ? g_active_session_id : 0;
}

uint32_t session_controller_frame_interval_ms() {
  return g_state == APP_SESSION_ACTIVE ? ACTIVE_FPS_MS : IDLE_FPS_MS;
}

bool session_controller_is_idle_stream() {
  return g_state == APP_IDLE_STREAM;
}

bool session_controller_audio_running() {
  return g_state == APP_SESSION_ACTIVE && audio_capture_is_running();
}

bool session_controller_has_active_session() {
  return g_state == APP_SESSION_ACTIVE;
}

void session_controller_on_audio_last_sent(Stream &log) {
  session_controller_request_end(APP_END_REASON_AUDIO_MAX, log);
}

const char *session_controller_end_reason_name(AppEndReason reason) {
  switch (reason) {
    case APP_END_REASON_AUDIO_MAX:
      return "audio_max";
    case APP_END_REASON_TIMEOUT:
      return "timeout";
    case APP_END_REASON_FORCED:
      return "forced";
    default:
      return "none";
  }
}

AppEndReason session_controller_last_end_reason() {
  return g_last_end_reason;
}

uint32_t session_controller_last_wake_timestamp_ms() {
  return g_last_wake_ts;
}

uint32_t session_controller_session_started_ms() {
  return g_session_started_ms;
}
