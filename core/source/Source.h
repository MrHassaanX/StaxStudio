#pragma once

#include <QJsonObject>
#include <QString>

enum class SourceType {
    DisplayCapture,
    WindowCapture,
    GameCapture,
    Webcam,
    Image,
    Text,
    Microphone,
    DesktopAudio,
    Browser,
    Media
};

QString sourceTypeName(SourceType type);
SourceType sourceTypeFromName(const QString &name, bool *ok = nullptr);

struct Source final {
    QString id;
    QString name;
    SourceType type = SourceType::DisplayCapture;
    bool enabled = true;
    QJsonObject configuration;
};
