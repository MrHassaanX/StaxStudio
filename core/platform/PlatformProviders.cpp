#include "PlatformProviders.h"

#include <QUrl>
#include <QUrlQuery>

QStringList YouTubeProvider::oauthScopes()
{
    return {QStringLiteral("https://www.googleapis.com/auth/youtube"),
            QStringLiteral("https://www.googleapis.com/auth/youtube.force-ssl")};
}

QVariantMap YouTubeProvider::createBroadcastRequest(const PlatformMetadata &metadata)
{
    QVariantMap snippet{{"title", metadata.title.trimmed().isEmpty() ? QStringLiteral("StaxStudio Live") : metadata.title.trimmed()},
                        {"description", metadata.description},
                        {"categoryId", metadata.categoryId}};
    if (metadata.scheduleForLater && metadata.scheduledStartTime.isValid())
        snippet.insert(QStringLiteral("scheduledStartTime"), metadata.scheduledStartTime.toUTC().toString(Qt::ISODateWithMs));

    const QVariantMap status{{"privacyStatus", metadata.privacy.isEmpty() ? QStringLiteral("private") : metadata.privacy},
                             {"selfDeclaredMadeForKids", metadata.madeForKids}};
    const QVariantMap contentDetails{{"enableDvr", metadata.enableDvr},
                                     {"enableAutoStart", metadata.autoStart},
                                     {"enableAutoStop", metadata.autoStop},
                                     {"latencyPreference", metadata.latency.isEmpty() ? QStringLiteral("low") : metadata.latency}};
    return {{"snippet", snippet}, {"status", status}, {"contentDetails", contentDetails}};
}

QVariantMap YouTubeProvider::createStreamRequest(const QString &title)
{
    return {{"snippet", QVariantMap{{"title", title.trimmed().isEmpty() ? QStringLiteral("StaxStudio Stream") : title.trimmed()}}},
            {"cdn", QVariantMap{{"frameRate", QStringLiteral("variable")},
                                {"ingestionType", QStringLiteral("rtmp")},
                                {"resolution", QStringLiteral("variable")}}}};
}

QString YouTubeProvider::bindBroadcastUrl(const QString &broadcastId, const QString &streamId)
{
    QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/liveBroadcasts/bind"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("part"), QStringLiteral("id,contentDetails"));
    query.addQueryItem(QStringLiteral("id"), broadcastId);
    query.addQueryItem(QStringLiteral("streamId"), streamId);
    url.setQuery(query);
    return url.toString();
}

QString YouTubeProvider::transitionBroadcastUrl(const QString &broadcastId, const QString &status)
{
    QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/liveBroadcasts/transition"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("part"), QStringLiteral("id,status"));
    query.addQueryItem(QStringLiteral("id"), broadcastId);
    query.addQueryItem(QStringLiteral("broadcastStatus"), status);
    url.setQuery(query);
    return url.toString();
}

QStringList TwitchProvider::oauthScopes()
{
    return {QStringLiteral("user:read:email"),
            QStringLiteral("channel:manage:broadcast"),
            QStringLiteral("channel:read:stream_key")};
}

QVariantMap TwitchProvider::updateChannelRequest(const PlatformMetadata &metadata)
{
    QVariantMap payload;
    if (!metadata.title.trimmed().isEmpty()) payload.insert(QStringLiteral("title"), metadata.title.trimmed());
    if (!metadata.categoryId.trimmed().isEmpty()) payload.insert(QStringLiteral("game_id"), metadata.categoryId.trimmed());
    if (!metadata.language.trimmed().isEmpty()) payload.insert(QStringLiteral("broadcaster_language"), metadata.language.trimmed());
    if (!metadata.tags.isEmpty()) payload.insert(QStringLiteral("tags"), metadata.tags);
    return payload;
}

QStringList KickProvider::oauthScopes()
{
    return {QStringLiteral("user:read"), QStringLiteral("channel:read"), QStringLiteral("channel:write")};
}

QVariantMap KickProvider::updateLivestreamRequest(const PlatformMetadata &metadata)
{
    QVariantMap payload;
    if (!metadata.title.trimmed().isEmpty()) payload.insert(QStringLiteral("title"), metadata.title.trimmed());
    if (!metadata.categoryId.trimmed().isEmpty()) payload.insert(QStringLiteral("category_id"), metadata.categoryId.trimmed());
    if (!metadata.language.trimmed().isEmpty()) payload.insert(QStringLiteral("language"), metadata.language.trimmed());
    return payload;
}

