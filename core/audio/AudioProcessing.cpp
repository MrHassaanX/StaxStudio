#include "AudioProcessing.h"
#include <QtMath>

namespace AudioProcessing {
AudioBlock normalizeToInternalFormat(AudioBlock block)
{
    if (block.sampleRate == InternalSampleRate || block.sampleRate <= 0 || block.channelCount <= 0) { block.sampleRate = InternalSampleRate; return block; }
    const int inputFrames = block.samples.size() / block.channelCount;
    if (inputFrames == 0) { block.sampleRate = InternalSampleRate; return block; }
    const int outputFrames = qMax(1, qRound(static_cast<double>(inputFrames) * InternalSampleRate / block.sampleRate));
    QVector<float> output(outputFrames * block.channelCount);
    const double scale = static_cast<double>(block.sampleRate) / InternalSampleRate;
    for (int frame = 0; frame < outputFrames; ++frame) {
        const double position = qMin((inputFrames - 1.0), frame * scale);
        const int before = static_cast<int>(position);
        const int after = qMin(before + 1, inputFrames - 1);
        const float fraction = static_cast<float>(position - before);
        for (int channel = 0; channel < block.channelCount; ++channel) {
            const float left = block.samples[before * block.channelCount + channel];
            const float right = block.samples[after * block.channelCount + channel];
            output[frame * block.channelCount + channel] = left + (right - left) * fraction;
        }
    }
    block.samples = std::move(output);
    block.sampleRate = InternalSampleRate;
    return block;
}

void applyGainAndMute(AudioBlock &block, const float gain, const bool muted)
{
    const float effectiveGain = muted ? 0.0f : qBound(0.0f, gain, 1.0f);
    if (effectiveGain == 1.0f) return;
    for (float &sample : block.samples) sample *= effectiveGain;
}

float peakDb(const AudioBlock &block)
{
    float peak = 0.0f;
    for (const float sample : block.samples) peak = qMax(peak, qAbs(sample));
    return peak > 0.0f ? qMax(-90.0f, 20.0f * qLn(peak) / qLn(10.0f)) : -90.0f;
}
}