#include <assert.h>
#include <stdint.h>

#include "../audio_processing.h"

int main() {
  AudioProcessingConfig config{};
  config.highpass_alpha = 0.95f;
  config.rms_threshold = 200;
  config.silence_exit_ms = 3000;

  AudioProcessingState state{};
  audio_processing_reset(&state);

  int16_t dc_noise[160];
  for (auto &sample : dc_noise) {
    sample = 500;
  }
  AudioProcessingResult result = audio_processing_process_samples(
      dc_noise, 160, 320, config, &state);
  assert(!result.voice_detected);
  assert(state.silence_accumulator_ms == 320);

  int16_t voice[160];
  for (int i = 0; i < 160; ++i) {
    voice[i] = (i % 2 == 0) ? 2500 : -2500;
  }
  result = audio_processing_process_samples(voice, 160, 320, config, &state);
  assert(result.voice_detected);
  assert(state.silence_accumulator_ms == 0);

  audio_processing_reset(&state);
  bool exited = false;
  for (int i = 0; i < 10; ++i) {
    result = audio_processing_process_samples(dc_noise, 160, 320, config, &state);
    if (result.silence_exit_reached) {
      exited = true;
      break;
    }
  }
  assert(exited);
  assert(state.silence_accumulator_ms >= 3000);
  return 0;
}
