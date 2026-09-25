#include "RecordingTypes.h"

#include <QtGlobal>

qint64 recordingFrameIndexAt(const qint64 elapsedNs, const int frameRate)
{
    return qMax<qint64>(0, elapsedNs) * qBound(1, frameRate, 240) / 1'000'000'000LL;
}

qint64 recordingFrameTimestampNs(const qint64 frameIndex, const int frameRate)
{
    return qMax<qint64>(0, frameIndex) * 1'000'000'000LL / qBound(1, frameRate, 240);
}

QString recordingStateName(const RecordingState state)
{
    switch (state) {
    case RecordingState::Idle: return QStringLiteral("Idle");
    case RecordingState::Starting: return QStringLiteral("Starting");
    case RecordingState::Recording: return QStringLiteral("Recording");
    case RecordingState::Stopping: return QStringLiteral("Stopping");
    case RecordingState::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Error");
}
