#pragma once

#include "CredentialStore.h"
#include "PlatformHttp.h"
#include "PlatformTypes.h"

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <functional>

class TwitchClient final : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(QJsonObject, QString)>;
    TwitchClient(PlatformHttpTransport *transport, CredentialStore *credentials,
                 QString credentialKey, QString clientId, QObject *parent = nullptr);
    [[nodiscard]] bool hasCredentials() const { return !refreshToken_.isEmpty(); }
    [[nodiscard]] QString clientId() const { return clientId_; }
    void beginDeviceAuthorization(Callback callback);
    void pollDeviceAuthorization(const QString &deviceCode, Callback callback);
    void validate(Callback callback);
    void get(const QUrl &url, Callback callback);
    void updateChannel(const QString &broadcasterId, const PlatformMetadata &metadata, Callback callback);
    void disconnect();
    [[nodiscard]] static QUrl apiUrl(const QString &path, const QMap<QString, QString> &query = {});

private:
    void saveToken(QJsonObject token, bool preserveRefresh, std::function<void(QString)> callback);
    void ensureToken(std::function<void(QString, QString)> callback);
    void authorizedRequest(PlatformHttpRequest request, Callback callback, bool retry = true);
    void finishRefresh(const QString &error);
    static QString errorFor(const PlatformHttpResult &result);
    PlatformHttpTransport *transport_;
    CredentialStore *credentials_;
    QString credentialKey_;
    QString clientId_;
    QString accessToken_;
    QString refreshToken_;
    qint64 expiresAtMs_ = 0;
    bool refreshing_ = false;
    QVector<std::function<void(QString, QString)>> pending_;
};
