#include "KickClient.h"

#include "PlatformProviders.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QMap>
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

KickClient::KickClient(PlatformHttpTransport *transport, CredentialStore *credentials,
                       QString credentialKey, QString clientId, QString clientSecret, QObject *parent)
    : QObject(parent), transport_(transport), credentials_(credentials), credentialKey_(std::move(credentialKey)),
      clientId_(std::move(clientId)), clientSecret_(std::move(clientSecret))
{
    const QJsonObject stored = QJsonDocument::fromJson(credentials_->readSecret(credentialKey_).toUtf8()).object();
    if (stored.value(QStringLiteral("clientId")).toString() != clientId_) return;
    accessToken_ = stored.value(QStringLiteral("accessToken")).toString();
    refreshToken_ = stored.value(QStringLiteral("refreshToken")).toString();
    expiresAtMs_ = stored.value(QStringLiteral("expiresAtMs")).toVariant().toLongLong();
}

void KickClient::disconnect()
{
    credentials_->deleteSecret(credentialKey_);
    accessToken_.clear();
    refreshToken_.clear();
    expiresAtMs_ = 0;
}

QString KickClient::errorFor(const PlatformHttpResult &result)
{
    if (!result.error.isEmpty()) return result.error;
    if (result.status == 401) return QStringLiteral("Kick authorization expired. Reconnect your account.");
    if (result.status == 403) return QStringLiteral("Kick denied this request. Check app scopes and channel permissions.");
    if (result.status == 400) return QStringLiteral("Kick rejected the request. Check channel metadata.");
    return QStringLiteral("Kick request failed (HTTP %1).").arg(result.status);
}

void KickClient::saveToken(const QJsonObject &json, bool preserveRefresh, std::function<void(QString)> callback)
{
    const QString access = json.value(QStringLiteral("access_token")).toString();
    QString refresh = json.value(QStringLiteral("refresh_token")).toString();
    if (refresh.isEmpty() && preserveRefresh) refresh = refreshToken_;
    if (access.isEmpty() || refresh.isEmpty()) {
        callback(QStringLiteral("Kick did not return usable credentials. Reconnect your account."));
        return;
    }
    const qint64 expiry = QDateTime::currentMSecsSinceEpoch() + qMax(60, json.value(QStringLiteral("expires_in")).toInt(3600)) * 1000LL;
    const QJsonObject stored{{"clientId", clientId_}, {"accessToken", access},
                             {"refreshToken", refresh}, {"expiresAtMs", expiry}};
    QString error;
    if (!credentials_->writeSecret(credentialKey_, QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)), &error)) {
        callback(error.isEmpty() ? QStringLiteral("Unable to store Kick credentials securely.") : error);
        return;
    }
    accessToken_ = access;
    refreshToken_ = refresh;
    expiresAtMs_ = expiry;
    callback({});
}

void KickClient::exchangeCode(const QString &code, const QString &verifier, const QString &redirectUri,
                              std::function<void(QString)> callback)
{
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://id.kick.com/oauth/token"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = encodeForm({{"client_id", clientId_}, {"client_secret", clientSecret_}, {"code", code},
                               {"code_verifier", verifier}, {"redirect_uri", redirectUri},
                               {"grant_type", QStringLiteral("authorization_code")}});
    transport_->send(std::move(request), [this, callback = std::move(callback)](PlatformHttpResult result) mutable {
        if (!result.ok()) { callback(errorFor(result)); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), false, std::move(callback));
    });
}

void KickClient::finishRefresh(const QString &error)
{
    refreshing_ = false;
    const auto pending = std::exchange(pending_, {});
    for (const auto &callback : pending) callback(error.isEmpty() ? accessToken_ : QString{}, error);
}

void KickClient::ensureToken(std::function<void(QString, QString)> callback)
{
    if (!accessToken_.isEmpty() && QDateTime::currentMSecsSinceEpoch() + 60000 < expiresAtMs_) {
        callback(accessToken_, {});
        return;
    }
    if (refreshToken_.isEmpty()) { callback({}, QStringLiteral("Connect Kick before using its API.")); return; }
    pending_.append(std::move(callback));
    if (refreshing_) return;
    refreshing_ = true;
    PlatformHttpRequest request;
    request.method = "POST";
    request.url = QUrl(QStringLiteral("https://id.kick.com/oauth/token"));
    request.contentType = "application/x-www-form-urlencoded";
    request.body = encodeForm({{"client_id", clientId_}, {"client_secret", clientSecret_},
                               {"refresh_token", refreshToken_}, {"grant_type", QStringLiteral("refresh_token")}});
    transport_->send(std::move(request), [this](PlatformHttpResult result) {
        if (!result.ok()) { finishRefresh(errorFor(result)); return; }
        saveToken(QJsonDocument::fromJson(result.body).object(), true,
                  [this](const QString &error) { finishRefresh(error); });
    });
}

QUrl KickClient::apiUrl(const QString &path)
{
    return QUrl(QStringLiteral("https://api.kick.com/public/v1/") + path);
}

void KickClient::authorizedRequest(PlatformHttpRequest request, Callback callback, bool retry)
{
    if (request.url.host() != QStringLiteral("api.kick.com") || request.url.scheme() != QStringLiteral("https")) {
        callback({}, QStringLiteral("Unexpected Kick API address."));
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
            if (!result.ok()) { callback({}, errorFor(result)); return; }
            if (result.body.isEmpty()) { callback({}, {}); return; }
            const QJsonDocument json = QJsonDocument::fromJson(result.body);
            if (!json.isObject()) { callback({}, QStringLiteral("Kick returned an invalid response.")); return; }
            callback(json.object(), {});
        });
    });
}

void KickClient::get(const QUrl &url, Callback callback)
{
    PlatformHttpRequest request;
    request.url = url;
    authorizedRequest(std::move(request), std::move(callback));
}

void KickClient::updateChannel(const PlatformMetadata &metadata, Callback callback)
{
    PlatformHttpRequest request;
    request.method = "PATCH";
    request.url = apiUrl(QStringLiteral("channels"));
    request.contentType = "application/json";
    request.body = QJsonDocument(QJsonObject::fromVariantMap(KickProvider::updateLivestreamRequest(metadata))).toJson(QJsonDocument::Compact);
    authorizedRequest(std::move(request), std::move(callback));
}
