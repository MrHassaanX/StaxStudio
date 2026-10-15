#include "PlatformHttp.h"

#include <QNetworkReply>
#include <QNetworkRequest>

void QtPlatformHttpTransport::send(PlatformHttpRequest request, Callback callback)
{
    if (request.url.scheme() != QStringLiteral("https")) {
        callback({0, {}, QStringLiteral("Platform API requires HTTPS.")});
        return;
    }
    QNetworkRequest networkRequest(request.url);
    networkRequest.setTransferTimeout(30000);
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!request.contentType.isEmpty()) networkRequest.setRawHeader("Content-Type", request.contentType);
    if (!request.bearerToken.isEmpty()) networkRequest.setRawHeader("Authorization", request.authScheme + " " + request.bearerToken);
    if (!request.clientId.isEmpty()) networkRequest.setRawHeader("Client-Id", request.clientId);
    QNetworkReply *reply = manager_.sendCustomRequest(networkRequest, request.method, request.body);
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() mutable {
        PlatformHttpResult result;
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && result.status == 0)
            result.error = QStringLiteral("Network request failed. Check your connection.");
        reply->deleteLater();
        callback(std::move(result));
    });
}
