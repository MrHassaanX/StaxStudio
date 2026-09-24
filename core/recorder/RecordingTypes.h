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

[[nodiscard]] QString recordingStateName(RecordingState state);
