#include "Source.h"

QString sourceTypeName(SourceType type)
{
    switch (type) {
    case SourceType::DisplayCapture: return QStringLiteral("Display Capture");
    case SourceType::WindowCapture: return QStringLiteral("Window Capture");
    case SourceType::GameCapture: return QStringLiteral("Game Capture");
    case SourceType::Webcam: return QStringLiteral("Webcam");
    case SourceType::Image: return QStringLiteral("Image");
    case SourceType::Text: return QStringLiteral("Text");
    case SourceType::Microphone: return QStringLiteral("Microphone");
    case SourceType::DesktopAudio: return QStringLiteral("Desktop Audio");
    case SourceType::Browser: return QStringLiteral("Browser Source");
    case SourceType::Media: return QStringLiteral("Media Source");
    }
    return QStringLiteral("Unknown");
}

SourceType sourceTypeFromName(const QString &name, bool *ok)
{
    const QString normalized = name.trimmed();
    for (const auto type : {SourceType::DisplayCapture, SourceType::WindowCapture,
                            SourceType::GameCapture, SourceType::Webcam, SourceType::Image,
                            SourceType::Text, SourceType::Microphone, SourceType::DesktopAudio,
                            SourceType::Browser, SourceType::Media}) {
        if (sourceTypeName(type).compare(normalized, Qt::CaseInsensitive) == 0) {
            if (ok) *ok = true;
            return type;
        }
    }
    if (ok) *ok = false;
    return SourceType::DisplayCapture;
}
