#pragma once

#include <Arduino.h>
#include "app_state.h"

void session_controller_begin();
bool session_controller_request_wake(Stream &log);
bool session_controller_request_end(AppEndReason reason, Stream &log);
void session_controller_tick(Stream &log);

AppState session_controller_state();
uint16_t session_controller_active_session_id();
uint16_t session_controller_frame_session_id();
uint32_t session_controller_frame_interval_ms();
bool session_controller_is_idle_stream();
bool session_controller_audio_running();
bool session_controller_has_active_session();

void session_controller_on_audio_last_sent(Stream &log);
const char *session_controller_end_reason_name(AppEndReason reason);
AppEndReason session_controller_last_end_reason();
uint32_t session_controller_last_wake_timestamp_ms();
uint32_t session_controller_session_started_ms();

