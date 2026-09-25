#pragma once
#include "core/audio/AudioTypes.h"

namespace AudioProcessing {
constexpr int InternalSampleRate = 48000;
// Preserves timestamp and channel layout; output samples are interleaved float32 at 48 kHz.
AudioBlock normalizeToInternalFormat(AudioBlock block, AudioResampleState *state = nullptr);
AudioBlock convertNativePcm(const void *data, int frames, int rate, int channels,
                           int bits, bool floatingPoint, quint64 channelMask,
                           qint64 timestampNs, AudioResampleState &state);
float peakDb(const AudioBlock &block);
void applyGainAndMute(AudioBlock &block, float gain, bool muted);
// Combines already gain-adjusted source blocks into the recorder's stereo program mix.
AudioBlock mixToStereo(const QVector<AudioBlock> &blocks);
}
