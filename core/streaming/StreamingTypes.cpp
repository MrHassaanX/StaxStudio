#include "StreamingTypes.h"

QString streamingStateName(const StreamingState state)
{
    switch (state) {
    case StreamingState::Idle: return QStringLiteral("Idle");
    case StreamingState::Connecting: return QStringLiteral("Connecting");
    case StreamingState::Live: return QStringLiteral("Live");
    case StreamingState::Reconnecting: return QStringLiteral("Reconnecting");
    case StreamingState::Stopping: return QStringLiteral("Stopping");
    case StreamingState::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Idle");
}

QString redactedStreamUrl(const StreamSettings &settings)
{
    QString url = settings.serverUrl.trimmed();
    const QString key = settings.streamKey.trimmed();
    if (url.isEmpty()) return {};
    if (key.isEmpty()) return url;
    if (!url.endsWith(QLatin1Char('/'))) url.append(QLatin1Char('/'));
    return url + QStringLiteral("<stream-key>");
}

