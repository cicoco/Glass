#include "audio_processing.h"

namespace {
int16_t clamp_int16(float value) {
  if (value > 32767.0f) {
    return 32767;
  }
  if (value < -32768.0f) {
    return -32768;
  }
  return static_cast<int16_t>(value);
}
}  // namespace

void audio_processing_reset(AudioProcessingState *state) {
  if (state == nullptr) {
    return;
  }
  state->previous_input = 0.0f;
  state->previous_output = 0.0f;
  state->silence_accumulator_ms = 0;
}

AudioProcessingResult audio_processing_process_samples(
    int16_t *samples, size_t count, uint32_t chunk_ms,
    const AudioProcessingConfig &config, AudioProcessingState *state) {
  AudioProcessingResult result{};
  if (samples == nullptr || count == 0 || state == nullptr) {
    return result;
  }

  uint64_t abs_sum = 0;
  for (size_t i = 0; i < count; ++i) {
    const float input = static_cast<float>(samples[i]);
    const float filtered =
        config.highpass_alpha * (state->previous_output + input - state->previous_input);
    state->previous_input = input;
    state->previous_output = filtered;

    const int16_t output = clamp_int16(filtered);
    samples[i] = output;
    abs_sum += static_cast<uint64_t>(output >= 0 ? output : -output);
  }

  result.rms_estimate = static_cast<uint32_t>(abs_sum / count);
  result.voice_detected = result.rms_estimate >= config.rms_threshold;
  if (result.voice_detected) {
    state->silence_accumulator_ms = 0;
  } else {
    state->silence_accumulator_ms += chunk_ms;
  }
  result.silence_exit_reached = state->silence_accumulator_ms >= config.silence_exit_ms;
  return result;
}
