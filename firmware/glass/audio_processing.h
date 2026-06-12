#pragma once

#include <stddef.h>
#include <stdint.h>

struct AudioProcessingConfig {
  float highpass_alpha;
  uint32_t rms_threshold;
  uint32_t silence_exit_ms;
};

struct AudioProcessingState {
  float previous_input;
  float previous_output;
  uint32_t silence_accumulator_ms;
};

struct AudioProcessingResult {
  uint32_t rms_estimate;
  bool voice_detected;
  bool silence_exit_reached;
};

void audio_processing_reset(AudioProcessingState *state);
AudioProcessingResult audio_processing_process_samples(
    int16_t *samples, size_t count, uint32_t chunk_ms,
    const AudioProcessingConfig &config, AudioProcessingState *state);
