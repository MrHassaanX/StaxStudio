#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QVariantList>

// All media timestamps are nanoseconds from one process-monotonic clock. They are not wall-clock values.
[[nodiscard]] qint64 mediaTimestampNs();

enum class CaptureState {
    Stopped,
    Starting,
    Active,
    Unavailable,
    Error
};

struct CaptureTarget final {
    QString id;
    QString name;
    QSize size;
    bool primary = false;
    QString detail;

    [[nodiscard]] QVariantMap toVariantMap() const
    {
        return {{"id", id}, {"name", name}, {"width", size.width()}, {"height", size.height()},
                {"primary", primary}, {"detail", detail}};
    }
};

struct CapturedVideoFrame final {
    QImage image;
    QSize pixelSize;
    qint64 timestampNs = 0;
    CaptureState state = CaptureState::Stopped;
    QString message;
};

[[nodiscard]] QString captureStateName(CaptureState state);
