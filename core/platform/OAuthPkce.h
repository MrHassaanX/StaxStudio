#pragma once

#include <QUrl>
#include <QVariantMap>

struct OAuthRequest final {
    QString authorizationEndpoint;
    QString clientId;
    QString redirectUri;
    QString scope;
    QString state;
    QString codeVerifier;
    QString codeChallenge;
    QVariantMap extraParameters;
};

class OAuthPkce final
{
public:
    [[nodiscard]] static QString generateVerifier(int byteCount = 32);
    [[nodiscard]] static QString generateState(int byteCount = 24);
    [[nodiscard]] static QString challengeForVerifier(const QString &verifier);
    [[nodiscard]] static QUrl authorizationUrl(const OAuthRequest &request);
    [[nodiscard]] static QString redact(QString text, const QStringList &secrets);
};

