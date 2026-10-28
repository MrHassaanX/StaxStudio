#include "PlatformProviders.h"

#include <QJsonArray>
#include <QSet>
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
                        {"scheduledStartTime", (metadata.scheduleForLater && metadata.scheduledStartTime.isValid()
                                ? metadata.scheduledStartTime : QDateTime::currentDateTimeUtc().addSecs(60)).toUTC().toString(Qt::ISODateWithMs)}};
    if (!metadata.categoryId.trimmed().isEmpty()) snippet.insert(QStringLiteral("categoryId"), metadata.categoryId.trimmed());

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

QMap<QString, QString> YouTubeProvider::broadcastListQuery(const QString &pageToken)
{
    // liveBroadcasts.list permits exactly one of broadcastStatus, id, or mine.
    // Request the authenticated account's broadcasts and filter lifecycle state locally.
    QMap<QString, QString> query{{QStringLiteral("part"), QStringLiteral("snippet,status,contentDetails")},
                                 {QStringLiteral("mine"), QStringLiteral("true")},
                                 {QStringLiteral("maxResults"), QStringLiteral("50")}};
    if (!pageToken.isEmpty()) query.insert(QStringLiteral("pageToken"), pageToken);
    return query;
}

QVariantList YouTubeProvider::eligibleBroadcasts(const QJsonObject &response)
{
    static const QSet<QString> eligibleStates{QStringLiteral("created"), QStringLiteral("ready"),
                                               QStringLiteral("testStarting"), QStringLiteral("testing"),
                                               QStringLiteral("liveStarting"), QStringLiteral("live")};
    QSet<QString> seenIds;
    QVariantList broadcasts;
    for (const QJsonValue &value : response.value(QStringLiteral("items")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString id = item.value(QStringLiteral("id")).toString();
        const QJsonObject status = item.value(QStringLiteral("status")).toObject();
        const QString lifecycle = status.value(QStringLiteral("lifeCycleStatus")).toString();
        if (id.isEmpty() || seenIds.contains(id) || !eligibleStates.contains(lifecycle)) continue;
        seenIds.insert(id);
        const QJsonObject snippet = item.value(QStringLiteral("snippet")).toObject();
        const QJsonObject content = item.value(QStringLiteral("contentDetails")).toObject();
        broadcasts.append(QVariantMap{{"id", id}, {"title", snippet.value(QStringLiteral("title")).toString()},
            {"description", snippet.value(QStringLiteral("description")).toString()},
            {"categoryId", snippet.value(QStringLiteral("categoryId")).toString()},
            {"scheduledStartTime", snippet.value(QStringLiteral("scheduledStartTime")).toString()},
            {"privacy", status.value(QStringLiteral("privacyStatus")).toString()},
            {"madeForKids", status.value(QStringLiteral("selfDeclaredMadeForKids")).toBool()},
            {"status", lifecycle},
            {"boundStreamId", content.value(QStringLiteral("boundStreamId")).toString()},
            {"enableDvr", content.value(QStringLiteral("enableDvr")).toBool()},
            {"latency", content.value(QStringLiteral("latencyPreference")).toString()},
            {"autoStart", content.value(QStringLiteral("enableAutoStart")).toBool()},
            {"autoStop", content.value(QStringLiteral("enableAutoStop")).toBool()}});
    }
    return broadcasts;
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
    return {QStringLiteral("channel:manage:broadcast"),
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
    if (!metadata.title.trimmed().isEmpty()) payload.insert(QStringLiteral("stream_title"), metadata.title.trimmed());
    bool categoryValid = false;
    const qlonglong categoryId = metadata.categoryId.trimmed().toLongLong(&categoryValid);
    if (categoryValid && categoryId > 0) payload.insert(QStringLiteral("category_id"), categoryId);
    return payload;
}

