#pragma once
#include "core/audio/AudioTypes.h"

namespace AudioProcessing {
constexpr int InternalSampleRate = 48000;
// Preserves timestamp and channel layout; output samples are interleaved float32 at 48 kHz.
AudioBlock normalizeToInternalFormat(AudioBlock block);
float peakDb(const AudioBlock &block);
void applyGainAndMute(AudioBlock &block, float gain, bool muted);
}