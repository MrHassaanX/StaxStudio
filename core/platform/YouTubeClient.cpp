#include "YouTubeClient.h"

#include <QDateTime>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QUrlQuery>
#include <utility>

namespace {
QByteArray formBody(const QMap<QString, QString> &values)
{
    QUrlQuery query;
    for (auto it = values.cbegin(); it != values.cend(); ++it) query.addQueryItem(it.key(), it.value());
    return query.query(QUrl::FullyEncoded).toUtf8();
}
}

YouTubeClient::YouTubeClient(PlatformHttpTransport *transport, CredentialStore *credentials,
                             QString credentialKey, QString clientId, QObject *parent)
    : QObject(parent), transport_(transport), credentials_(credentials),
      credentialKey_(std::move(credentialKey)), clientId_(std::move(clientId))
{
    const QJsonObject stored = QJsonDocument::fromJson(credentials_->readSecret(credentialKey_).toUtf8()).object();
    if (stored.value(QStringLiteral("clientId")).toString() != clientId_) return;
    accessToken_ = stored.value(QStringLiteral("accessToken")).toString();
    refreshToken_ = stored.value(QStringLiteral("refreshToken")).toString();
    expiresAtMs_ = stored.value(QStringLiteral("expiresAtMs")).toVariant().toLongLong();
}

bool YouTubeClient::hasCredentials() const { return !refreshToken_.isEmpty(); }

void YouTubeClient::disconnect()
{
    accessToken_.clear();
    refreshToken_.clear();
    expiresAtMs_ = 0;
    credentials_->deleteSecret(credentialKey_);
}

QString YouTubeClient::httpError(const PlatformHttpResult &result)
{
    if (!result.error.isEmpty()) return result.error;
    if (result.status == 401) return QStringLiteral("YouTube authorization expired. Reconnect your account.");
    if (result.status == 403) return QStringLiteral("YouTube denied the request. Check live eligibility, permissions, or API quota.");
    if (result.status == 400) return QStringLiteral("YouTube rejected the request. Check broadcast settings.");
    if (result.status == 404) return QStringLiteral("YouTube broadcast or stream was not found.");
    return QStringLiteral("YouTube request failed (HTTP %1).").arg(result.status);
}

void YouTubeClient::saveToken(const QJsonObject &json, bool preserveRefreshToken, TokenCallback callback)
{
    const QString access = json.value(QStringLiteral("access_token")).toString();
    QString refresh = json.value(QStringLiteral("refresh_token")).toString();
    if (refresh.isEmpty() && preserveRefreshToken) refresh = refreshToken_;
    if (access.isEmpty() || refresh.isEmpty()) {
        callback(QStringLiteral("YouTube did not return a usable refresh token. Reconnect and grant access."));
        return;
    }
    const qint64 expiry = QDateTime::currentMSecsSinceEpoch() + qMax(60, json.value(QStringLiteral("expires_in")).toInt(3600)) * 1000LL;
    const QJsonObject stored{{"clientId", clientId_}, {"accessToken", access},
                             {"refreshToken", refresh}, {"expiresAtMs", expiry}};
    QString error;
    if (!credentials_->writeSecret(credentialKey_, QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)), &error)) {
        callback(error.isEmpty() ? QStringLiteral("Unable to save YouTube credentials securely.") : error);
        return;
    }
    accessToken_ = access;
    refreshToken_ = refresh;
    expiresAtMs_ = expiry;
    callback({});
}

void YouTubeClient::exchangeCode(const QString &code, const QString &verifier, const QString &redirectUri, TokenCallback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = formBody({{"client_id", clientId_}, {"code", code}, {"code_verifier", verifier},
                             {"grant_type", QStringLiteral("authorization_code")}, {"redirect_uri", redirectUri}});
    transport_->send(std::move(request), [this, callback = std::move(callback)](PlatformHttpResult result) mutable {
        if (!result.ok()) { callback(httpError(result)); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), false, std::move(callback));
    });
}

void YouTubeClient::finishRefresh(const QString &error)
{
    refreshing_ = false;
    const auto pending = std::exchange(pendingTokenRequests_, {});
    for (const auto &callback : pending) callback(error.isEmpty() ? accessToken_ : QString{}, error);
}

void YouTubeClient::refresh()
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = formBody({{"client_id", clientId_}, {"refresh_token", refreshToken_},
                             {"grant_type", QStringLiteral("refresh_token")}});
    transport_->send(std::move(request), [this](PlatformHttpResult result) {
        if (!result.ok()) { finishRefresh(httpError(result)); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), true,
                  [this](const QString &error) { finishRefresh(error); });
    });
}

void YouTubeClient::ensureToken(std::function<void(QString, QString)> callback)
{
    if (!accessToken_.isEmpty() && QDateTime::currentMSecsSinceEpoch() + 60000 < expiresAtMs_) {
        callback(accessToken_, {});
        return;
    }
    if (refreshToken_.isEmpty()) { callback({}, QStringLiteral("Connect YouTube before using its API.")); return; }
    pendingTokenRequests_.append(std::move(callback));
    if (!refreshing_) { refreshing_ = true; refresh(); }
}

QUrl YouTubeClient::apiUrl(const QString &path, const QMap<QString, QString> &query)
{
    QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/") + path);
    QUrlQuery params;
    for (auto it = query.cbegin(); it != query.cend(); ++it) params.addQueryItem(it.key(), it.value());
    url.setQuery(params);
    return url;
}

void YouTubeClient::authorizedRequest(PlatformHttpRequest request, JsonCallback callback, bool retry)
{
    const QString host = request.url.host();
    if (host != QStringLiteral("www.googleapis.com")) {
        callback({}, QStringLiteral("Unexpected YouTube API host."));
        return;
    }
    ensureToken([this, request = std::move(request), callback = std::move(callback), retry](QString token, QString error) mutable {
        if (!error.isEmpty()) { callback({}, error); return; }
        request.bearerToken = token.toUtf8();
        const PlatformHttpRequest retryRequest = request;
        transport_->send(std::move(request), [this, request = retryRequest, callback = std::move(callback), retry](PlatformHttpResult result) mutable {
            if (result.status == 401 && retry && !refreshToken_.isEmpty()) {
                accessToken_.clear();
                expiresAtMs_ = 0;
                authorizedRequest(std::move(request), std::move(callback), false);
                return;
            }
            if (!result.ok()) { callback({}, httpError(result)); return; }
            const QJsonDocument document = QJsonDocument::fromJson(result.body);
            if (!document.isObject()) { callback({}, QStringLiteral("YouTube returned an invalid response.")); return; }
            callback(document.object(), {});
        });
    });
}

void YouTubeClient::get(const QUrl &url, JsonCallback callback)
{
    PlatformHttpRequest request;
    request.url = url;
    authorizedRequest(std::move(request), std::move(callback));
}

void YouTubeClient::post(const QUrl &url, const QJsonObject &body, JsonCallback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = url;
    request.contentType = "application/json";
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    authorizedRequest(std::move(request), std::move(callback));
}

void YouTubeClient::postEmpty(const QUrl &url, JsonCallback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = url;
    authorizedRequest(std::move(request), std::move(callback));
}

void YouTubeClient::uploadThumbnail(const QString &broadcastId, const QString &path, JsonCallback callback)
{
    const QString localPath = path.startsWith(QStringLiteral("file:")) ? QUrl(path).toLocalFile() : path;
    QFile file(localPath);
    QImageReader reader(localPath);
    const QByteArray format = reader.format().toLower();
    if (broadcastId.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024
        || (format != "jpeg" && format != "jpg" && format != "png")) {
        callback({}, QStringLiteral("Choose a JPEG or PNG thumbnail smaller than 2 MB."));
        return;
    }
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://www.googleapis.com/upload/youtube/v3/thumbnails/set"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("videoId"), broadcastId);
    query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("media"));
    request.url.setQuery(query);
    request.contentType = format == "png" ? "image/png" : "image/jpeg";
    request.body = file.readAll();
    authorizedRequest(std::move(request), std::move(callback));
}
