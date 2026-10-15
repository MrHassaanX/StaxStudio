#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <functional>

struct PlatformHttpRequest {
    QByteArray method = "GET";
    QUrl url;
    QByteArray body;
    QByteArray contentType;
    QByteArray bearerToken;
    QByteArray authScheme = "Bearer";
    QByteArray clientId;
};

struct PlatformHttpResult {
    int status = 0;
    QByteArray body;
    QString error;
    [[nodiscard]] bool ok() const { return error.isEmpty() && status >= 200 && status < 300; }
};

class PlatformHttpTransport : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(PlatformHttpResult)>;
    using QObject::QObject;
    virtual void send(PlatformHttpRequest request, Callback callback) = 0;
};

class QtPlatformHttpTransport final : public PlatformHttpTransport {
    Q_OBJECT
public:
    using PlatformHttpTransport::PlatformHttpTransport;
    void send(PlatformHttpRequest request, Callback callback) override;
private:
    QNetworkAccessManager manager_;
};
