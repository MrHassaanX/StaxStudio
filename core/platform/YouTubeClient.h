#pragma once

#include "CredentialStore.h"
#include "PlatformHttp.h"

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QVector>

class YouTubeClient final : public QObject {
    Q_OBJECT
public:
    using JsonCallback = std::function<void(QJsonObject, QString)>;
    using TokenCallback = std::function<void(QString)>;
    using ChannelCallback = std::function<void(QString, QString, QString)>;

    YouTubeClient(PlatformHttpTransport *transport, CredentialStore *credentials,
                  QString credentialKey, QString clientId, QString clientSecret = {},
                  QObject *parent = nullptr);
    [[nodiscard]] bool hasCredentials() const;
    [[nodiscard]] QString clientId() const { return clientId_; }
    void exchangeCode(const QString &code, const QString &verifier, const QString &redirectUri, TokenCallback callback);
    void loadOwnChannel(ChannelCallback callback);
    void disconnect();
    void get(const QUrl &url, JsonCallback callback);
    void post(const QUrl &url, const QJsonObject &body, JsonCallback callback);
    void postEmpty(const QUrl &url, JsonCallback callback);
    void uploadThumbnail(const QString &broadcastId, const QString &path, JsonCallback callback);
    [[nodiscard]] static QUrl apiUrl(const QString &path, const QMap<QString, QString> &query = {});

private:
    void ensureToken(std::function<void(QString, QString)> callback);
    void refresh();
    void saveToken(const QJsonObject &json, bool preserveRefreshToken, TokenCallback callback);
    void authorizedRequest(PlatformHttpRequest request, JsonCallback callback, bool retry = true,
                           const QString &operation = QStringLiteral("api_request"));
    void finishRefresh(const QString &error);
    static QString httpError(const PlatformHttpResult &result, const QString &operation);

    PlatformHttpTransport *transport_;
    CredentialStore *credentials_;
    QString credentialKey_;
    QString clientId_;
    QString clientSecret_;
    QString accessToken_;
    QString refreshToken_;
    qint64 expiresAtMs_ = 0;
    bool refreshing_ = false;
    QVector<std::function<void(QString, QString)>> pendingTokenRequests_;
};
