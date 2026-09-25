#pragma once
#include "core/capture/CaptureTypes.h"
#include <QVector>
#include <memory>

// Every runtime source delivers interleaved 32-bit float samples at 48 kHz.
// timestampNs uses the shared process-monotonic media clock; channelCount
// preserves the source layout until the program mixer produces stereo output.
struct AudioBlock final {
    qint64 timestampNs = 0;
    int sampleRate = 0;
    int channelCount = 0;
    QVector<float> samples;
    quint64 channelMask = 0;
};

// Kept by a capture source so packet-by-packet rate conversion cannot drift
// from the source clock over a long recording.
struct AudioResampleState final {
    int inputSampleRate = 0;
    int channelCount = 0;
    int inputFormat = -1;
    quint64 channelMask = 0;
    std::shared_ptr<struct SwrContext> context;
    qint64 originNs = -1;
    qint64 outputFrames = 0;
};

struct AudioRuntime final {
    CaptureState state = CaptureState::Stopped;
    QString message;
    float peakDb = -90.0f;
};
