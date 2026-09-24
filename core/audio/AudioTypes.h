#pragma once
#include "core/capture/CaptureTypes.h"
#include <QVector>

// Future encoders receive interleaved 32-bit float samples at their native rate.
struct AudioBlock final {
    qint64 timestampNs = 0;
    int sampleRate = 0;
    int channelCount = 0;
    QVector<float> samples;
};

struct AudioRuntime final {
    CaptureState state = CaptureState::Stopped;
    QString message;
    float peakDb = -90.0f;
};