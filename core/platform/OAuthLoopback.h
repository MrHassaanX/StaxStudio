#pragma once

#include "OAuthPkce.h"

#include <QObject>
#include <QTcpServer>
#include <QTimer>

class OAuthLoopback final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] QUrl begin(OAuthRequest request, QString *error = nullptr,
                             const QString &host = QStringLiteral("127.0.0.1"), quint16 port = 0);
    void cancel();
signals:
    void authorized(QString code, QString verifier, QString redirectUri);
    void failed(QString message);
private:
    void acceptConnection();
    QTcpServer server_;
    QTimer timeout_;
    QString state_;
    QString verifier_;
    QString redirectUri_;
};
