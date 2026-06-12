#include <Arduino.h>
#include <ESP.h>

#include "config.h"
#include "app/app_state.h"
#include "app/input_handler.h"
#include "app/session_controller.h"
#include "capture/audio_capture.h"
#include "capture/camera_capture.h"
#include "transport/frame_protocol.h"
#include "transport/tcp_server.h"

namespace {
AppState g_last_state = APP_CAMERA_SWITCHING;
uint32_t g_next_image_due_ms = 0;
uint32_t g_next_audio_due_ms = 0;

const char *state_name(AppState state) {
  switch (state) {
    case APP_IDLE_STREAM:
      return "idle";
    case APP_SESSION_ACTIVE:
      return "active";
    case APP_CAMERA_SWITCHING:
      return "switching";
    default:
      return "unknown";
  }
}

void print_network_info() {
  Serial.print(F("[INFO] IP="));
  Serial.print(tcp_server_local_ip());
  Serial.print(F(" port="));
  Serial.println(JG_TCP_PORT);
}

void print_status() {
  Serial.print(F("[STATUS] state="));
  Serial.print(state_name(session_controller_state()));
  Serial.print(F(" sid="));
  Serial.print(session_controller_active_session_id());
  Serial.print(F(" heap="));
  Serial.print(ESP.getFreeHeap());
  Serial.print(F(" psram="));
  Serial.print(ESP.getFreePsram());
  Serial.print(F(" drop="));
  Serial.print(tcp_server_drop_count());
  Serial.print(F(" rssi="));
  Serial.println(tcp_server_rssi());
}

void maybe_send_image(uint32_t now) {
  if (g_next_image_due_ms != 0 && now < g_next_image_due_ms) {
    return;
  }

  JpegFrame frame{};
  if (!camera_capture_get_jpeg(&frame, Serial)) {
    g_next_image_due_ms = now + session_controller_frame_interval_ms();
    return;
  }

  JGFrameHeader header{};
  header.type = JG_FRAME_IMAGE;
  header.flags = session_controller_is_idle_stream() ? JG_FLAG_STREAM_IDLE : 0;
  header.timestamp_ms = frame.timestamp_ms;
  header.session_id = session_controller_frame_session_id();
  header.payload_len = static_cast<uint32_t>(frame.len);

  tcp_server_send_frame(header, frame.data);
  camera_capture_release(&frame);
  g_next_image_due_ms = now + session_controller_frame_interval_ms();
}

void maybe_send_audio(uint32_t now) {
  if (!session_controller_audio_running()) {
    return;
  }
  if (g_next_audio_due_ms != 0 && now < g_next_audio_due_ms) {
    return;
  }

  AudioChunkView chunk{};
  if (!audio_capture_read_chunk(&chunk, Serial)) {
    g_next_audio_due_ms = now + AUDIO_CHUNK_MS;
    return;
  }
  const bool terminal_chunk = chunk.is_last || chunk.silence_exit_reached;

  JGFrameHeader header{};
  header.type = JG_FRAME_AUDIO;
  header.flags = terminal_chunk ? JG_FLAG_AUDIO_LAST : 0;
  header.timestamp_ms = millis();
  header.session_id = session_controller_frame_session_id();
  header.payload_len = static_cast<uint32_t>(chunk.len);

  tcp_server_send_frame(header, chunk.data);
  g_next_audio_due_ms = now + AUDIO_CHUNK_MS;

  Serial.print(F("[VOICE] rms="));
  Serial.print(chunk.rms_estimate);
  Serial.print(F(" silence_ms="));
  Serial.println(chunk.silence_accumulator_ms);

  if (chunk.is_last) {
    session_controller_on_audio_last_sent(Serial);
    return;
  }
  if (chunk.silence_exit_reached) {
    session_controller_request_end(APP_END_REASON_SILENCE, Serial);
  }
}

void on_state_transition(AppState new_state) {
  g_last_state = new_state;
  g_next_image_due_ms = 0;
  g_next_audio_due_ms = 0;
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.print(F("[BOOT] JuneGlass Glass FW "));
  Serial.println(JG_FW_VERSION);

  input_handler_begin();
  if (!tcp_server_begin(Serial)) {
    Serial.println(F("[BOOT] WiFi/TCP init failed"));
  }
  if (!camera_capture_begin_idle(Serial)) {
    Serial.println(F("[BOOT] camera init failed"));
  }
  session_controller_begin();
}

void loop() {
  tcp_server_tick(Serial);

  InputCommandFlags commands{};
  input_handler_poll(&commands);
  if (commands.wake_requested) {
    session_controller_request_wake(Serial);
  }
  if (commands.force_end_requested) {
    session_controller_request_end(APP_END_REASON_FORCED, Serial);
  }
  if (commands.print_status_requested) {
    print_status();
  }
  if (commands.print_network_requested) {
    print_network_info();
  }

  session_controller_tick(Serial);

  const AppState current_state = session_controller_state();
  if (current_state != g_last_state) {
    on_state_transition(current_state);
  }

  const uint32_t now = millis();
  maybe_send_image(now);
  maybe_send_audio(now);
  delay(5);
}
