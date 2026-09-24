#include "CaptureTypes.h"

#include <QElapsedTimer>

qint64 mediaTimestampNs()
{
    static const QElapsedTimer clock = [] { QElapsedTimer value; value.start(); return value; }();
    return clock.nsecsElapsed();
}

QString captureStateName(const CaptureState state)
{
    switch (state) {
    case CaptureState::Stopped: return QStringLiteral("Stopped");
    case CaptureState::Starting: return QStringLiteral("Starting");
    case CaptureState::Active: return QStringLiteral("Active");
    case CaptureState::Unavailable: return QStringLiteral("Unavailable");
    case CaptureState::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Unavailable");
}
