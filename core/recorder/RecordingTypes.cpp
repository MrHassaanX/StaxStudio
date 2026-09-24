#include "RecordingTypes.h"

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
