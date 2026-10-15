#include "TwitchClient.h"

#include "PlatformProviders.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <utility>

namespace {
QByteArray encodeForm(const QMap<QString, QString> &values)
{
    QUrlQuery query;
    for (auto it = values.cbegin(); it != values.cend(); ++it) query.addQueryItem(it.key(), it.value());
    return query.query(QUrl::FullyEncoded).toUtf8();
}
}

TwitchClient::TwitchClient(PlatformHttpTransport *transport, CredentialStore *credentials,
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

void TwitchClient::disconnect()
{
    credentials_->deleteSecret(credentialKey_);
    accessToken_.clear();
    refreshToken_.clear();
    expiresAtMs_ = 0;
}

QString TwitchClient::errorFor(const PlatformHttpResult &result)
{
    if (!result.error.isEmpty()) return result.error;
    if (result.status == 401) return QStringLiteral("Twitch authorization expired. Reconnect your account.");
    if (result.status == 403) return QStringLiteral("Twitch denied this action. Check channel permissions and streaming eligibility.");
    if (result.status == 400) return QStringLiteral("Twitch rejected the request. Check your channel settings.");
    return QStringLiteral("Twitch request failed (HTTP %1).").arg(result.status);
}

void TwitchClient::saveToken(QJsonObject token, bool preserveRefresh, std::function<void(QString)> callback)
{
    const QString access = token.value(QStringLiteral("access_token")).toString();
    QString refresh = token.value(QStringLiteral("refresh_token")).toString();
    if (refresh.isEmpty() && preserveRefresh) refresh = refreshToken_;
    if (access.isEmpty() || refresh.isEmpty()) {
        callback(QStringLiteral("Twitch did not return usable credentials. Reconnect your account."));
        return;
    }
    const qint64 expiry = QDateTime::currentMSecsSinceEpoch() + qMax(60, token.value(QStringLiteral("expires_in")).toInt(14400)) * 1000LL;
    const QJsonObject stored{{"clientId", clientId_}, {"accessToken", access},
                             {"refreshToken", refresh}, {"expiresAtMs", expiry}};
    QString error;
    if (!credentials_->writeSecret(credentialKey_, QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)), &error)) {
        callback(error.isEmpty() ? QStringLiteral("Unable to save Twitch credentials securely.") : error);
        return;
    }
    accessToken_ = access;
    refreshToken_ = refresh;
    expiresAtMs_ = expiry;
    callback({});
}

void TwitchClient::beginDeviceAuthorization(Callback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://id.twitch.tv/oauth2/device"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = encodeForm({{"client_id", clientId_}, {"scopes", TwitchProvider::oauthScopes().join(QLatin1Char(' '))}});
    transport_->send(std::move(request), [callback = std::move(callback)](PlatformHttpResult result) {
        if (!result.ok()) { callback({}, errorFor(result)); return; }
        const QJsonObject json = QJsonDocument::fromJson(result.body).object();
        if (json.value(QStringLiteral("device_code")).toString().isEmpty()
            || json.value(QStringLiteral("verification_uri")).toString().isEmpty()
            || json.value(QStringLiteral("user_code")).toString().isEmpty()) {
            callback({}, QStringLiteral("Twitch did not return a device authorization code."));
            return;
        }
        callback(json, {});
    });
}

void TwitchClient::pollDeviceAuthorization(const QString &deviceCode, Callback callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://id.twitch.tv/oauth2/token"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = encodeForm({{"client_id", clientId_}, {"scopes", TwitchProvider::oauthScopes().join(QLatin1Char(' '))}, {"device_code", deviceCode},
                               {"grant_type", QStringLiteral("urn:ietf:params:oauth:grant-type:device_code")}});
    transport_->send(std::move(request), [this, callback = std::move(callback)](PlatformHttpResult result) {
        if (result.status == 400) {
            const QString message = QJsonDocument::fromJson(result.body).object().value(QStringLiteral("message")).toString();
            if (message == QStringLiteral("authorization_pending") || message == QStringLiteral("slow_down")) {
                callback({{"pending", true}, {"slowDown", message == QStringLiteral("slow_down")}}, {});
                return;
            }
        }
        if (!result.ok()) { callback({}, errorFor(result)); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), false,
                  [callback = std::move(callback)](const QString &error) {
            callback(error.isEmpty() ? QJsonObject{{"connected", true}} : QJsonObject{}, error);
        });
    });
}

void TwitchClient::finishRefresh(const QString &error)
{
    refreshing_ = false;
    const auto pending = std::exchange(pending_, {});
    for (const auto &callback : pending) callback(error.isEmpty() ? accessToken_ : QString{}, error);
}

void TwitchClient::ensureToken(std::function<void(QString, QString)> callback)
{
    if (!accessToken_.isEmpty() && QDateTime::currentMSecsSinceEpoch() + 60000 < expiresAtMs_) {
        callback(accessToken_, {});
        return;
    }
    if (refreshToken_.isEmpty()) { callback({}, QStringLiteral("Connect Twitch before using its API.")); return; }
    pending_.append(std::move(callback));
    if (refreshing_) return;
    refreshing_ = true;
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://id.twitch.tv/oauth2/token"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = encodeForm({{"client_id", clientId_}, {"refresh_token", refreshToken_},
                               {"grant_type", QStringLiteral("refresh_token")}});
    transport_->send(std::move(request), [this](PlatformHttpResult result) {
        if (!result.ok()) { finishRefresh(errorFor(result)); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), true,
                  [this](const QString &error) { finishRefresh(error); });
    });
}

QUrl TwitchClient::apiUrl(const QString &path, const QMap<QString, QString> &query)
{
    QUrl url(QStringLiteral("https://api.twitch.tv/helix/") + path);
    QUrlQuery params;
    for (auto it = query.cbegin(); it != query.cend(); ++it) params.addQueryItem(it.key(), it.value());
    url.setQuery(params);
    return url;
}

void TwitchClient::authorizedRequest(PlatformHttpRequest request, Callback callback, bool retry)
{
    if (request.url.host() != QStringLiteral("api.twitch.tv") && request.url.host() != QStringLiteral("id.twitch.tv")) {
        callback({}, QStringLiteral("Unexpected Twitch API host."));
        return;
    }
    ensureToken([this, request = std::move(request), callback = std::move(callback), retry](QString token, QString error) mutable {
        if (!error.isEmpty()) { callback({}, error); return; }
        request.bearerToken = token.toUtf8();
        request.clientId = clientId_.toUtf8();
        const PlatformHttpRequest retryRequest = request;
        transport_->send(std::move(request), [this, request = retryRequest, callback = std::move(callback), retry](PlatformHttpResult result) mutable {
            if (result.status == 401 && retry && !refreshToken_.isEmpty()) {
                accessToken_.clear();
                expiresAtMs_ = 0;
                authorizedRequest(std::move(request), std::move(callback), false);
                return;
            }
            if (!result.ok()) { callback({}, errorFor(result)); return; }
            if (result.body.isEmpty()) { callback({}, {}); return; }
            const QJsonDocument json = QJsonDocument::fromJson(result.body);
            if (!json.isObject()) { callback({}, QStringLiteral("Twitch returned an invalid response.")); return; }
            callback(json.object(), {});
        });
    });
}

void TwitchClient::validate(Callback callback)
{
    PlatformHttpRequest request;
    request.url = QUrl(QStringLiteral("https://id.twitch.tv/oauth2/validate"));
    request.authScheme = "OAuth";
    authorizedRequest(std::move(request), std::move(callback));
}

void TwitchClient::get(const QUrl &url, Callback callback)
{
    PlatformHttpRequest request;
    request.url = url;
    authorizedRequest(std::move(request), std::move(callback));
}

void TwitchClient::updateChannel(const QString &broadcasterId, const PlatformMetadata &metadata, Callback callback)
{
    PlatformHttpRequest request;
    request.method = "PATCH";
    request.url = apiUrl(QStringLiteral("channels"), {{"broadcaster_id", broadcasterId}});
    request.contentType = "application/json";
    request.body = QJsonDocument(QJsonObject::fromVariantMap(TwitchProvider::updateChannelRequest(metadata))).toJson(QJsonDocument::Compact);
    authorizedRequest(std::move(request), std::move(callback));
}
