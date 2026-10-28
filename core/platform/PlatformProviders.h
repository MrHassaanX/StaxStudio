#pragma once

#include "PlatformTypes.h"

#include <QJsonObject>
#include <QMap>
#include <QVariantList>
#include <QVariantMap>

class YouTubeProvider final
{
public:
    [[nodiscard]] static QStringList oauthScopes();
    [[nodiscard]] static QVariantMap createBroadcastRequest(const PlatformMetadata &metadata);
    [[nodiscard]] static QVariantMap createStreamRequest(const QString &title);
    [[nodiscard]] static QMap<QString, QString> broadcastListQuery(const QString &pageToken = {});
    [[nodiscard]] static QVariantList eligibleBroadcasts(const QJsonObject &response);
    [[nodiscard]] static QString bindBroadcastUrl(const QString &broadcastId, const QString &streamId);
    [[nodiscard]] static QString transitionBroadcastUrl(const QString &broadcastId, const QString &status);
};

class TwitchProvider final
{
public:
    [[nodiscard]] static QStringList oauthScopes();
    [[nodiscard]] static QVariantMap updateChannelRequest(const PlatformMetadata &metadata);
};

class KickProvider final
{
public:
    [[nodiscard]] static QStringList oauthScopes();
    [[nodiscard]] static QVariantMap updateLivestreamRequest(const PlatformMetadata &metadata);
};

