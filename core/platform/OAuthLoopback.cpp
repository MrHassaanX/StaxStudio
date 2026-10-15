#include "OAuthLoopback.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QUrlQuery>
#include <memory>

QUrl OAuthLoopback::begin(OAuthRequest request, QString *error, const QString &host, quint16 port)
{
    cancel();
    if (request.clientId.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("OAuth Client ID is not configured.");
        return {};
    }
    if (host != QStringLiteral("localhost") && host != QStringLiteral("127.0.0.1")) {
        if (error) *error = QStringLiteral("Only localhost OAuth callbacks are allowed.");
        return {};
    }
    if (!server_.listen(QHostAddress::LocalHost, port)) {
        if (error) *error = QStringLiteral("Unable to listen for the browser sign-in callback.");
        return {};
    }
    state_ = OAuthPkce::generateState();
    verifier_ = OAuthPkce::generateVerifier();
    redirectUri_ = QStringLiteral("http://%1:%2/oauth/callback").arg(host).arg(server_.serverPort());
    request.redirectUri = redirectUri_;
    request.state = state_;
    request.codeVerifier = verifier_;
    request.codeChallenge = OAuthPkce::challengeForVerifier(verifier_);
    connect(&server_, &QTcpServer::newConnection, this, &OAuthLoopback::acceptConnection, Qt::UniqueConnection);
    timeout_.setSingleShot(true);
    timeout_.disconnect(this);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        cancel();
        emit failed(QStringLiteral("Sign-in timed out. Try connecting again."));
    });
    timeout_.start(120000);
    return OAuthPkce::authorizationUrl(request);
}

void OAuthLoopback::cancel()
{
    timeout_.stop();
    server_.close();
    state_.clear();
    verifier_.clear();
    redirectUri_.clear();
}

void OAuthLoopback::acceptConnection()
{
    while (QTcpSocket *socket = server_.nextPendingConnection()) {
        auto request = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, request] {
            request->append(socket->readAll());
            if (request->size() > 8192) { socket->disconnectFromHost(); return; }
            const int end = request->indexOf("\r\n");
            if (end < 0) return;
            const QList<QByteArray> parts = request->left(end).split(' ');
            if (parts.size() < 3 || parts[0] != "GET") { socket->disconnectFromHost(); return; }
            const QUrl callbackUrl(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts[1]));
            if (callbackUrl.path() != QStringLiteral("/oauth/callback") || state_.isEmpty()) {
                socket->disconnectFromHost();
                return;
            }
            const QUrlQuery query(callbackUrl);
            const bool stateValid = query.queryItemValue(QStringLiteral("state")) == state_;
            const QString code = query.queryItemValue(QStringLiteral("code"));
            const QString browserError = query.queryItemValue(QStringLiteral("error"));
            const QString verifier = verifier_;
            const QString redirect = redirectUri_;
            const bool success = stateValid && !code.isEmpty() && browserError.isEmpty();
            const QByteArray page = success ? "<html><body>StaxStudio is connected. You may close this tab.</body></html>"
                                            : "<html><body>StaxStudio sign-in failed. Return to the app and try again.</body></html>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nContent-Length: " + QByteArray::number(page.size()) + "\r\nConnection: close\r\n\r\n" + page);
            socket->disconnectFromHost();
            cancel();
            if (success) emit authorized(code, verifier, redirect);
            else emit failed(!stateValid ? QStringLiteral("Sign-in security check failed. Try again.")
                                         : browserError.isEmpty() ? QStringLiteral("Sign-in response was incomplete.")
                                                                  : QStringLiteral("Sign-in was cancelled or denied."));
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}
