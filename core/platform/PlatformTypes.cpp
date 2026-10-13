#include "PlatformTypes.h"

#include <QUuid>

namespace {
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

QString platformProviderName(const PlatformProviderId provider)
{
    switch (provider) {
    case PlatformProviderId::CustomRtmp: return QStringLiteral("custom-rtmp");
    case PlatformProviderId::YouTube: return QStringLiteral("youtube");
    case PlatformProviderId::Twitch: return QStringLiteral("twitch");
    case PlatformProviderId::Kick: return QStringLiteral("kick");
    }
    return QStringLiteral("custom-rtmp");
}

QString platformProviderLabel(const PlatformProviderId provider)
{
    switch (provider) {
    case PlatformProviderId::CustomRtmp: return QStringLiteral("Custom RTMP");
    case PlatformProviderId::YouTube: return QStringLiteral("YouTube");
    case PlatformProviderId::Twitch: return QStringLiteral("Twitch");
    case PlatformProviderId::Kick: return QStringLiteral("Kick");
    }
    return QStringLiteral("Custom RTMP");
}

PlatformProviderId platformProviderFromName(const QString &name, bool *ok)
{
    const QString value = name.trimmed().toLower();
    if (ok) *ok = true;
    if (value == QStringLiteral("custom-rtmp") || value == QStringLiteral("custom rtmp")) return PlatformProviderId::CustomRtmp;
    if (value == QStringLiteral("youtube")) return PlatformProviderId::YouTube;
    if (value == QStringLiteral("twitch")) return PlatformProviderId::Twitch;
    if (value == QStringLiteral("kick")) return PlatformProviderId::Kick;
    if (ok) *ok = false;
    return PlatformProviderId::CustomRtmp;
}

PlatformCapabilities platformCapabilities(const PlatformProviderId provider)
{
    PlatformCapabilities value;
    switch (provider) {
    case PlatformProviderId::CustomRtmp:
        value.supportsStreamKey = true;
        value.supportsTitle = false;
        return value;
    case PlatformProviderId::YouTube:
        value.supportsOAuth = true;
        value.supportsAutomaticIngestion = true;
        value.supportsTitle = true;
        value.supportsDescription = true;
        value.supportsThumbnail = true;
        value.supportsCategory = true;
        value.supportsPrivacy = true;
        value.supportsScheduling = true;
        value.supportsLatency = true;
        value.supportsDvr = true;
        value.supportsMadeForKids = true;
        value.supportsAutoStartStop = true;
        return value;
    case PlatformProviderId::Twitch:
        value.supportsOAuth = true;
        value.supportsAutomaticIngestion = true;
        value.supportsTitle = true;
        value.supportsCategory = true;
        value.supportsTags = true;
        return value;
    case PlatformProviderId::Kick:
        value.supportsOAuth = true;
        value.supportsTitle = true;
        value.supportsCategory = true;
        return value;
    }
    return value;
}

QVariantMap capabilitiesToVariant(const PlatformCapabilities &c)
{
    return {{"supportsOAuth", c.supportsOAuth},
            {"supportsAutomaticIngestion", c.supportsAutomaticIngestion},
            {"supportsStreamKey", c.supportsStreamKey},
            {"supportsTitle", c.supportsTitle},
            {"supportsDescription", c.supportsDescription},
            {"supportsThumbnail", c.supportsThumbnail},
            {"supportsCategory", c.supportsCategory},
            {"supportsPrivacy", c.supportsPrivacy},
            {"supportsScheduling", c.supportsScheduling},
            {"supportsLatency", c.supportsLatency},
            {"supportsTags", c.supportsTags},
            {"supportsDvr", c.supportsDvr},
            {"supportsMadeForKids", c.supportsMadeForKids},
            {"supportsAutoStartStop", c.supportsAutoStartStop}};
}

QVariantMap metadataToVariant(const PlatformMetadata &m)
{
    return {{"title", m.title},
            {"description", m.description},
            {"privacy", m.privacy},
            {"categoryId", m.categoryId},
            {"thumbnailPath", m.thumbnailPath},
            {"latency", m.latency},
            {"language", m.language},
            {"tags", m.tags},
            {"madeForKids", m.madeForKids},
            {"enableDvr", m.enableDvr},
            {"autoStart", m.autoStart},
            {"autoStop", m.autoStop},
            {"scheduleForLater", m.scheduleForLater},
            {"scheduledStartTime", m.scheduledStartTime.toUTC().toString(Qt::ISODateWithMs)}};
}

PlatformMetadata metadataFromVariant(const QVariantMap &values)
{
    PlatformMetadata m;
    m.title = values.value(QStringLiteral("title")).toString();
    m.description = values.value(QStringLiteral("description")).toString();
    m.privacy = values.value(QStringLiteral("privacy"), QStringLiteral("private")).toString();
    m.categoryId = values.value(QStringLiteral("categoryId")).toString();
    m.thumbnailPath = values.value(QStringLiteral("thumbnailPath")).toString();
    m.latency = values.value(QStringLiteral("latency"), QStringLiteral("low")).toString();
    m.language = values.value(QStringLiteral("language")).toString();
    const QVariant tags = values.value(QStringLiteral("tags"));
    if (tags.canConvert<QStringList>()) {
        m.tags = tags.toStringList();
    } else {
        for (const QVariant &tag : tags.toList()) {
            const QString value = tag.toString().trimmed();
            if (!value.isEmpty()) m.tags.append(value);
        }
    }
    m.madeForKids = values.value(QStringLiteral("madeForKids"), false).toBool();
    m.enableDvr = values.value(QStringLiteral("enableDvr"), true).toBool();
    m.autoStart = values.value(QStringLiteral("autoStart"), true).toBool();
    m.autoStop = values.value(QStringLiteral("autoStop"), true).toBool();
    m.scheduleForLater = values.value(QStringLiteral("scheduleForLater"), false).toBool();
    m.scheduledStartTime = QDateTime::fromString(values.value(QStringLiteral("scheduledStartTime")).toString(), Qt::ISODateWithMs);
    return m;
}

StreamDestination defaultDestination(const PlatformProviderId provider)
{
    StreamDestination d;
    d.id = newId();
    d.provider = provider;
    d.displayName = platformProviderLabel(provider);
    d.connected = provider == PlatformProviderId::CustomRtmp;
    d.metadata.title = QStringLiteral("StaxStudio Live");
    return d;
}

QString redactedDestinationSummary(const StreamDestination &destination)
{
    QString label = platformProviderLabel(destination.provider);
    if (!destination.accountName.isEmpty()) label += QStringLiteral(" - ") + destination.accountName;
    if (destination.provider == PlatformProviderId::CustomRtmp) {
        const QString server = destination.configuration.value(QStringLiteral("serverUrl")).toString();
        if (!server.isEmpty()) label += QStringLiteral(" (") + server + QStringLiteral(")");
    }
    return label;
}

