#include "YouTubeClient.h"

#include <QDateTime>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QDebug>
#include <utility>

namespace {
QByteArray formBody(const QMap<QString, QString> &values)
{
    QUrlQuery query;
    for (auto it = values.cbegin(); it != values.cend(); ++it) query.addQueryItem(it.key(), it.value());
    return query.query(QUrl::FullyEncoded).toUtf8();
}

struct SafeProviderFailure {
    QString code;
    QString category;
};

SafeProviderFailure safeProviderFailure(const QByteArray &body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    QString reason;
    const QJsonValue errorValue = root.value(QStringLiteral("error"));
    if (errorValue.isString()) {
        reason = errorValue.toString();
    } else if (errorValue.isObject()) {
        const QJsonObject error = errorValue.toObject();
        const QJsonArray errors = error.value(QStringLiteral("errors")).toArray();
        if (!errors.isEmpty()) reason = errors.first().toObject().value(QStringLiteral("reason")).toString();
        if (reason.isEmpty()) reason = error.value(QStringLiteral("status")).toString();
    }
    static const QRegularExpression safePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]{0,79}$"));
    SafeProviderFailure failure;
    if (safePattern.match(reason).hasMatch()) failure.code = reason;

    // Never surface arbitrary provider descriptions. Map only known,
    // non-sensitive parameter diagnostics into stable local categories.
    const QString description = root.value(QStringLiteral("error_description")).toString().toLower();
    if (description.contains(QStringLiteral("client_secret"))
        && (description.contains(QStringLiteral("missing"))
            || description.contains(QStringLiteral("required")))) {
        failure.category = QStringLiteral("missing_client_secret");
    } else {
        static const QStringList parameters = {
            QStringLiteral("client_id"), QStringLiteral("code_verifier"),
            QStringLiteral("redirect_uri"), QStringLiteral("grant_type"),
            QStringLiteral("refresh_token"), QStringLiteral("code")
        };
        for (const QString &parameter : parameters) {
            if (description.contains(parameter)
                && (description.contains(QStringLiteral("missing"))
                    || description.contains(QStringLiteral("required")))) {
                failure.category = QStringLiteral("missing_") + parameter;
                break;
            }
        }
    }
    return failure;
}

QString operationLabel(const QString &operation)
{
    if (operation == QStringLiteral("token_exchange")) return QStringLiteral("Google token exchange");
    if (operation == QStringLiteral("token_refresh")) return QStringLiteral("Google token refresh");
    if (operation == QStringLiteral("channel_lookup")) return QStringLiteral("YouTube channel lookup");
    return QStringLiteral("YouTube API request");
}
}

YouTubeClient::YouTubeClient(PlatformHttpTransport *transport, CredentialStore *credentials,
                             QString credentialKey, QString clientId, QString clientSecret,
                             QObject *parent)
    : QObject(parent), transport_(transport), credentials_(credentials),
      credentialKey_(std::move(credentialKey)), clientId_(std::move(clientId)),
      clientSecret_(std::move(clientSecret))
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

QString YouTubeClient::httpError(const PlatformHttpResult &result, const QString &operation)
{
    if (!result.error.isEmpty()) return result.error;
    const SafeProviderFailure failure = safeProviderFailure(result.body);
    const QString reason = failure.code;
    const QString diagnosticReason = failure.category.isEmpty()
        ? (reason.isEmpty() ? QStringLiteral("unavailable") : reason)
        : (reason.isEmpty() ? failure.category : reason + QLatin1Char('/') + failure.category);
    qWarning().noquote() << QStringLiteral("YouTube operation=%1 httpStatus=%2 providerReason=%3")
                                .arg(operation).arg(result.status).arg(diagnosticReason);
    const QString detail = reason.isEmpty()
        ? QStringLiteral("HTTP %1").arg(result.status)
        : QStringLiteral("HTTP %1, %2").arg(result.status).arg(reason);
    const QString categorizedDetail = failure.category.isEmpty()
        ? detail : detail + QStringLiteral(", ") + failure.category;
    if ((operation == QStringLiteral("token_exchange") || operation == QStringLiteral("token_refresh"))
        && failure.category == QStringLiteral("missing_client_secret")) {
        return QStringLiteral("%1 failed (%2). The matching Desktop OAuth client secret is missing from the request configuration.")
            .arg(operationLabel(operation), categorizedDetail);
    }
    if (operation == QStringLiteral("token_exchange")) {
        if (reason == QStringLiteral("invalid_grant"))
            return QStringLiteral("Google token exchange failed (%1). Start Connect YouTube again; the authorization code expired or its redirect/PKCE values did not match.").arg(detail);
        if (reason == QStringLiteral("invalid_client"))
            return QStringLiteral("Google token exchange failed (%1). The configured Desktop OAuth client was rejected.").arg(detail);
        if (reason == QStringLiteral("access_denied"))
            return QStringLiteral("Google denied account access (%1). If the app is in Testing, add this account as a test user.").arg(detail);
    }
    if (operation == QStringLiteral("channel_lookup") && result.status == 403) {
        if (reason == QStringLiteral("accessNotConfigured") || reason == QStringLiteral("SERVICE_DISABLED"))
            return QStringLiteral("YouTube channel lookup failed (%1). Enable YouTube Data API v3 for this Google Cloud project, then try again.").arg(detail);
        if (reason == QStringLiteral("insufficientPermissions") || reason == QStringLiteral("insufficient_scope"))
            return QStringLiteral("YouTube channel lookup failed (%1). Reconnect and grant YouTube access.").arg(detail);
    }
    if (result.status == 401) return QStringLiteral("%1 failed (%2). Reconnect your account.").arg(operationLabel(operation), categorizedDetail);
    if (result.status == 403) return QStringLiteral("%1 was denied (%2). Check API access, permissions, or quota.").arg(operationLabel(operation), categorizedDetail);
    if (result.status == 400) return QStringLiteral("%1 was rejected (%2).").arg(operationLabel(operation), categorizedDetail);
    if (result.status == 404) return QStringLiteral("%1 could not find the requested resource (%2).").arg(operationLabel(operation), categorizedDetail);
    return QStringLiteral("%1 failed (%2).").arg(operationLabel(operation), categorizedDetail);
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
    QMap<QString, QString> values{{"client_id", clientId_}, {"code", code}, {"code_verifier", verifier},
                                  {"grant_type", QStringLiteral("authorization_code")}, {"redirect_uri", redirectUri}};
    if (!clientSecret_.isEmpty()) values.insert(QStringLiteral("client_secret"), clientSecret_);
    request.body = formBody(values);
    transport_->send(std::move(request), [this, callback = std::move(callback)](PlatformHttpResult result) mutable {
        if (!result.ok()) { callback(httpError(result, QStringLiteral("token_exchange"))); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), false, std::move(callback));
    });
}

void YouTubeClient::loadOwnChannel(ChannelCallback callback)
{
    PlatformHttpRequest request;
    request.url = apiUrl(QStringLiteral("channels"), {{QStringLiteral("part"), QStringLiteral("snippet")},
                                                       {QStringLiteral("mine"), QStringLiteral("true")}});
    authorizedRequest(std::move(request), [callback = std::move(callback)](QJsonObject json, QString error) mutable {
        if (!error.isEmpty()) { callback({}, {}, std::move(error)); return; }
        const QJsonArray channels = json.value(QStringLiteral("items")).toArray();
        const QJsonObject channel = channels.isEmpty() ? QJsonObject{} : channels.first().toObject();
        const QString id = channel.value(QStringLiteral("id")).toString();
        const QString name = channel.value(QStringLiteral("snippet")).toObject().value(QStringLiteral("title")).toString();
        if (id.isEmpty() || name.isEmpty()) {
            callback({}, {}, QStringLiteral("No YouTube channel is available for this Google account."));
            return;
        }
        callback(id, name, {});
    }, true, QStringLiteral("channel_lookup"));
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
    QMap<QString, QString> values{{"client_id", clientId_}, {"refresh_token", refreshToken_},
                                  {"grant_type", QStringLiteral("refresh_token")}};
    if (!clientSecret_.isEmpty()) values.insert(QStringLiteral("client_secret"), clientSecret_);
    request.body = formBody(values);
    transport_->send(std::move(request), [this](PlatformHttpResult result) {
        if (!result.ok()) { finishRefresh(httpError(result, QStringLiteral("token_refresh"))); return; }
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

void YouTubeClient::authorizedRequest(PlatformHttpRequest request, JsonCallback callback, bool retry, const QString &operation)
{
    const QString host = request.url.host();
    if (host != QStringLiteral("www.googleapis.com")) {
        callback({}, QStringLiteral("Unexpected YouTube API host."));
        return;
    }
    ensureToken([this, request = std::move(request), callback = std::move(callback), retry, operation](QString token, QString error) mutable {
        if (!error.isEmpty()) { callback({}, error); return; }
        request.bearerToken = token.toUtf8();
        const PlatformHttpRequest retryRequest = request;
        transport_->send(std::move(request), [this, request = retryRequest, callback = std::move(callback), retry, operation](PlatformHttpResult result) mutable {
            if (result.status == 401 && retry && !refreshToken_.isEmpty()) {
                accessToken_.clear();
                expiresAtMs_ = 0;
                authorizedRequest(std::move(request), std::move(callback), false, operation);
                return;
            }
            if (!result.ok()) { callback({}, httpError(result, operation)); return; }
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
    authorizedRequest(std::move(request), std::move(callback), true, QStringLiteral("api_request"));
}

void YouTubeClient::post(const QUrl &url, const QJsonObject &body, JsonCallback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = url;
    request.contentType = "application/json";
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    authorizedRequest(std::move(request), std::move(callback), true, QStringLiteral("api_request"));
}

void YouTubeClient::postEmpty(const QUrl &url, JsonCallback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = url;
    authorizedRequest(std::move(request), std::move(callback), true, QStringLiteral("api_request"));
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
    authorizedRequest(std::move(request), std::move(callback), true, QStringLiteral("api_request"));
}
