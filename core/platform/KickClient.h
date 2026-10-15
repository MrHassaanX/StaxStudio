#pragma once

#include "CredentialStore.h"
#include "PlatformHttp.h"
#include "PlatformTypes.h"

#include <QJsonObject>
#include <QObject>
#include <QVector>
#include <functional>

class KickClient final : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(QJsonObject, QString)>;
    KickClient(PlatformHttpTransport *transport, CredentialStore *credentials,
               QString credentialKey, QString clientId, QString clientSecret, QObject *parent = nullptr);
    [[nodiscard]] bool configured() const { return !clientId_.isEmpty() && !clientSecret_.isEmpty(); }
    [[nodiscard]] bool hasCredentials() const { return !refreshToken_.isEmpty(); }
    [[nodiscard]] QString clientId() const { return clientId_; }
    void exchangeCode(const QString &code, const QString &verifier, const QString &redirectUri,
                      std::function<void(QString)> callback);
    void get(const QUrl &url, Callback callback);
    void updateChannel(const PlatformMetadata &metadata, Callback callback);
    void disconnect();
    [[nodiscard]] static QUrl apiUrl(const QString &path);

private:
    void saveToken(const QJsonObject &json, bool preserveRefresh, std::function<void(QString)> callback);
    void ensureToken(std::function<void(QString, QString)> callback);
    void finishRefresh(const QString &error);
    void authorizedRequest(PlatformHttpRequest request, Callback callback, bool retry = true);
    static QString errorFor(const PlatformHttpResult &result);
    PlatformHttpTransport *transport_;
    CredentialStore *credentials_;
    QString credentialKey_;
    QString clientId_;
    QString clientSecret_;
    QString accessToken_;
    QString refreshToken_;
    qint64 expiresAtMs_ = 0;
    bool refreshing_ = false;
    QVector<std::function<void(QString, QString)>> pending_;
};
