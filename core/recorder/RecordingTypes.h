#pragma once

#include <QImage>
#include <QString>

enum class RecordingState { Idle, Starting, Recording, Stopping, Error };

struct RecordingSettings final {
    QString outputDirectory;
    QString encoder = QStringLiteral("Auto");
    int frameRate = 60;
};

struct RecordedVideoFrame final {
    QImage image;
    qint64 timestampNs = 0;
};

// The independent program/output scheduler uses this clock. Preview repainting
// never determines recorder timestamps or whether an output frame exists.
[[nodiscard]] qint64 recordingFrameIndexAt(qint64 elapsedNs, int frameRate);
[[nodiscard]] qint64 recordingFrameTimestampNs(qint64 frameIndex, int frameRate);

[[nodiscard]] QString recordingStateName(RecordingState state);
