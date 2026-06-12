#include "audio_capture.h"

#include <ESP_I2S.h>

#include "../audio_processing.h"
#include "../config.h"

namespace {
I2SClass g_i2s;
uint8_t *g_audio_buffer = nullptr;
bool g_running = false;
uint32_t g_started_ms = 0;
AudioProcessingConfig g_processing_config{};
AudioProcessingState g_processing_state{};
}  // namespace

bool audio_capture_start(Stream &log) {
  if (g_running) {
    return true;
  }

  if (g_audio_buffer == nullptr) {
    g_audio_buffer = static_cast<uint8_t *>(malloc(AUDIO_CHUNK_BYTES));
  }
  if (g_audio_buffer == nullptr) {
    log.println(F("[AUDIO] failed to allocate chunk buffer"));
    return false;
  }

  g_i2s.setPinsPdmRx(42, 41);
  if (!g_i2s.begin(I2S_MODE_PDM_RX, AUDIO_SAMPLE_RATE,
                   I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    log.println(F("[AUDIO] i2s begin failed"));
    return false;
  }

  g_running = true;
  g_started_ms = millis();
  g_processing_config.highpass_alpha = AUDIO_HIGHPASS_ALPHA;
  g_processing_config.rms_threshold = AUDIO_RMS_THRESHOLD;
  g_processing_config.silence_exit_ms = SILENCE_EXIT_SECONDS * 1000UL;
  audio_processing_reset(&g_processing_state);
  log.println(F("[AUDIO] started"));
  return true;
}

bool audio_capture_read_chunk(AudioChunkView *chunk, Stream &log) {
  if (!g_running || chunk == nullptr || g_audio_buffer == nullptr) {
    return false;
  }

  const int read = g_i2s.readBytes(reinterpret_cast<char *>(g_audio_buffer), AUDIO_CHUNK_BYTES);
  if (read <= 0) {
    log.println(F("[AUDIO] read failed"));
    return false;
  }

  const size_t samples = static_cast<size_t>(read / sizeof(int16_t));
  AudioProcessingResult processed = audio_processing_process_samples(
      reinterpret_cast<int16_t *>(g_audio_buffer), samples, AUDIO_CHUNK_MS,
      g_processing_config, &g_processing_state);

  chunk->data = g_audio_buffer;
  chunk->len = static_cast<size_t>(read);
  chunk->is_last = (millis() - g_started_ms) >= AUDIO_MAX_DURATION_MS;
  chunk->rms_estimate = processed.rms_estimate;
  chunk->voice_detected = processed.voice_detected;
  chunk->silence_exit_reached = processed.silence_exit_reached;
  chunk->silence_accumulator_ms = g_processing_state.silence_accumulator_ms;
  return true;
}

void audio_capture_stop() {
  if (!g_running) {
    return;
  }
  g_i2s.end();
  g_running = false;
}

bool audio_capture_is_running() {
  return g_running;
}

uint32_t audio_capture_started_ms() {
  return g_started_ms;
}
