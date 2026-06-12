#pragma once

#include <Arduino.h>

struct AudioChunkView {
  const uint8_t *data;
  size_t len;
  bool is_last;
  uint32_t rms_estimate;
  bool voice_detected;
  bool silence_exit_reached;
  uint32_t silence_accumulator_ms;
};

bool audio_capture_start(Stream &log);
bool audio_capture_read_chunk(AudioChunkView *chunk, Stream &log);
void audio_capture_stop();
bool audio_capture_is_running();
uint32_t audio_capture_started_ms();
