#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

enum class PlatformProviderId { CustomRtmp, YouTube, Twitch, Kick };

struct PlatformCapabilities final {
    bool supportsOAuth = false;
    bool supportsAutomaticIngestion = false;
    bool supportsStreamKey = false;
    bool supportsTitle = false;
    bool supportsDescription = false;
    bool supportsThumbnail = false;
    bool supportsCategory = false;
    bool supportsPrivacy = false;
    bool supportsScheduling = false;
    bool supportsLatency = false;
    bool supportsTags = false;
    bool supportsDvr = false;
    bool supportsMadeForKids = false;
    bool supportsAutoStartStop = false;
};

struct PlatformMetadata final {
    QString title;
    QString description;
    QString privacy = QStringLiteral("private");
    QString categoryId;
    QString thumbnailPath;
    QString latency = QStringLiteral("low");
    QString language;
    QStringList tags;
    bool madeForKids = false;
    bool enableDvr = true;
    bool autoStart = true;
    bool autoStop = true;
    bool scheduleForLater = false;
    QDateTime scheduledStartTime;
};

struct StreamDestination final {
    QString id;
    PlatformProviderId provider = PlatformProviderId::CustomRtmp;
    QString displayName;
    QString accountName;
    bool enabled = true;
    bool connected = false;
    QVariantMap configuration;
    PlatformMetadata metadata;
};

[[nodiscard]] QString platformProviderName(PlatformProviderId provider);
[[nodiscard]] QString platformProviderLabel(PlatformProviderId provider);
[[nodiscard]] PlatformProviderId platformProviderFromName(const QString &name, bool *ok = nullptr);
[[nodiscard]] PlatformCapabilities platformCapabilities(PlatformProviderId provider);
[[nodiscard]] QVariantMap capabilitiesToVariant(const PlatformCapabilities &capabilities);
[[nodiscard]] QVariantMap metadataToVariant(const PlatformMetadata &metadata);
[[nodiscard]] PlatformMetadata metadataFromVariant(const QVariantMap &values);
[[nodiscard]] StreamDestination defaultDestination(PlatformProviderId provider);
[[nodiscard]] QString redactedDestinationSummary(const StreamDestination &destination);

